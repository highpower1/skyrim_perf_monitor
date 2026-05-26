#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <shlobj.h>
#include <windows.h>
#include <psapi.h>

#ifdef MAX_PATH
#define WINDOWS_MAX_PATH 260
#undef MAX_PATH
#endif

#define DLLEXPORT __declspec(dllexport)

#include <chrono>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <string>

// Structure to capture frame performance details
struct FrameMetric {
	uint64_t frameIndex;
	float engineDeltaMs;
	float engineRealDeltaMs;
	float realFrameTimeMs;
	float realFps;
	float ramUsageMB;
	int highActorsCount;
};

// Configuration Structure
struct Config {
	int warmupFrames = 100;
	int flushInterval = 1000;
	bool logRAM = true;
	bool logHighActors = true;
	std::string outputFileName = "skyrim_perf_log.csv";
};

Config g_config;

void WriteImmediateLog(const char* message)
{
	OutputDebugStringA(message);
	OutputDebugStringA("\n");

	std::ofstream logFile("skyrim_perf_monitor_boot.log", std::ios::app);
	if (logFile.is_open()) {
		auto now = std::chrono::system_clock::now();
		auto time_t_now = std::chrono::system_clock::to_time_t(now);
		struct tm buf;
		localtime_s(&buf, &time_t_now);
		logFile << "[" << buf.tm_hour << ":" << buf.tm_min << ":" << buf.tm_sec << "] " << message << std::endl;
		logFile.close();
	}
}

void LoadConfig()
{
	// Skyrim relative path is rooted at the main game directory (where SkyrimSE.exe sits)
	const char* relativeIniPath = ".\\Data\\SKSE\\Plugins\\skyrim_perf_monitor.ini";

	char absIniPath[WINDOWS_MAX_PATH];
	GetFullPathNameA(relativeIniPath, WINDOWS_MAX_PATH, absIniPath, NULL);

	WriteImmediateLog((std::string("DEBUG: Loading INI config from: ") + absIniPath).c_str());

	// If INI doesn't exist, these will automatically fall back to the default values specified
	g_config.warmupFrames = GetPrivateProfileIntA("Settings", "WarmupFrames", 100, absIniPath);
	g_config.flushInterval = GetPrivateProfileIntA("Settings", "FlushInterval", 1000, absIniPath);
	g_config.logRAM = GetPrivateProfileIntA("Settings", "LogRAM", 1, absIniPath) != 0;
	g_config.logHighActors = GetPrivateProfileIntA("Settings", "LogHighActors", 1, absIniPath) != 0;

	char fileNameBuf[WINDOWS_MAX_PATH];
	GetPrivateProfileStringA("Settings", "OutputFileName", "skyrim_perf_log.csv", fileNameBuf, WINDOWS_MAX_PATH, absIniPath);
	g_config.outputFileName = fileNameBuf;

	char debugBuf[512];
	sprintf_s(debugBuf, "DEBUG: Config loaded - WarmupFrames: %d, FlushInterval: %d, LogRAM: %d, LogHighActors: %d, OutputFileName: %s",
		g_config.warmupFrames, g_config.flushInterval, g_config.logRAM, g_config.logHighActors, g_config.outputFileName.c_str());
	WriteImmediateLog(debugBuf);
}

float GetRAMUsageMB()
{
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
		return static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
	}
	return 0.0f;
}

int GetHighActorsCount()
{
	auto* processLists = RE::ProcessLists::GetSingleton();
	if (processLists) {
		return processLists->numberHighActors;
	}
	return 0;
}

namespace MetricLogger
{
	std::vector<FrameMetric> g_metricsBuffer;
	std::mutex g_metricsMutex;
	std::atomic<uint64_t> g_frameCounter{ 0 };

	std::queue<std::vector<FrameMetric>> g_writeQueue;
	std::mutex g_writeMutex;
	std::condition_variable g_writeCV;
	std::thread g_writerThread;
	std::atomic<bool> g_shutdown{ false };
	std::atomic<bool> g_initialized{ false };
	std::atomic<bool> g_shutdownDone{ false };

	std::filesystem::path g_csvPath;

	void WriterThreadFunc()
	{
		std::ofstream csvFile(g_csvPath, std::ios::out | std::ios::app);
		if (!csvFile.is_open()) return;

		while (true) {
			std::vector<FrameMetric> batch;
			{
				std::unique_lock<std::mutex> lock(g_writeMutex);
				g_writeCV.wait(lock, [] { return !g_writeQueue.empty() || g_shutdown; });

				if (g_writeQueue.empty() && g_shutdown) {
					break;
				}

				if (!g_writeQueue.empty()) {
					batch = std::move(g_writeQueue.front());
					g_writeQueue.pop();
				}
			}

			if (!batch.empty()) {
				for (const auto& m : batch) {
					csvFile << m.frameIndex << ","
							<< m.engineDeltaMs << ","
							<< m.engineRealDeltaMs << ","
							<< m.realFrameTimeMs << ","
							<< m.realFps;
					if (g_config.logRAM) {
						csvFile << "," << m.ramUsageMB;
					}
					if (g_config.logHighActors) {
						csvFile << "," << m.highActorsCount;
					}
					csvFile << "\n";
				}
				csvFile.flush();
			}
		}

		// Write remaining queued batches on exit
		while (true) {
			std::vector<FrameMetric> batch;
			{
				std::lock_guard<std::mutex> lock(g_writeMutex);
				if (g_writeQueue.empty()) break;
				batch = std::move(g_writeQueue.front());
				g_writeQueue.pop();
			}
			if (!batch.empty()) {
				for (const auto& m : batch) {
					csvFile << m.frameIndex << ","
							<< m.engineDeltaMs << ","
							<< m.engineRealDeltaMs << ","
							<< m.realFrameTimeMs << ","
							<< m.realFps;
					if (g_config.logRAM) {
						csvFile << "," << m.ramUsageMB;
					}
					if (g_config.logHighActors) {
						csvFile << "," << m.highActorsCount;
					}
					csvFile << "\n";
				}
			}
		}
		csvFile.close();
	}

	void Initialize(const std::filesystem::path& csvPath)
	{
		if (g_initialized.exchange(true)) return;

		g_csvPath = csvPath;
		std::filesystem::create_directories(g_csvPath.parent_path());

		// Start with a clean header matching active INI column preferences
		std::ofstream csvFile(g_csvPath, std::ios::out);
		if (csvFile.is_open()) {
			csvFile << "FrameIndex,Engine_Delta_ms,Engine_RealDelta_ms,Measured_FrameTime_ms,Measured_FPS";
			if (g_config.logRAM) {
				csvFile << ",RAM_MB";
			}
			if (g_config.logHighActors) {
				csvFile << ",High_Actors";
			}
			csvFile << "\n";
			csvFile.close();
		}

		g_shutdown = false;
		g_shutdownDone = false;
		g_writerThread = std::thread(WriterThreadFunc);
		WriteImmediateLog("DEBUG: MetricLogger Thread Setup Complete");
	}

	void Shutdown()
	{
		if (!g_initialized) return;
		if (g_shutdownDone.exchange(true)) return;

		WriteImmediateLog("DEBUG: MetricLogger Asynchronous Flush Start...");

		// Flush remaining local metrics buffer
		std::vector<FrameMetric> remainingBatch;
		{
			std::lock_guard<std::mutex> lock(g_metricsMutex);
			if (!g_metricsBuffer.empty()) {
				remainingBatch = std::move(g_metricsBuffer);
				g_metricsBuffer.clear();
			}
		}

		if (!remainingBatch.empty()) {
			std::lock_guard<std::mutex> lock(g_writeMutex);
			g_writeQueue.push(std::move(remainingBatch));
		}

		g_shutdown = true;
		g_writeCV.notify_all();

		if (g_writerThread.joinable()) {
			g_writerThread.join();
		}
		WriteImmediateLog("DEBUG: MetricLogger Flush Success");
	}

	void Record(float engineDeltaMs, float engineRealDeltaMs, float realFrameTimeMs, float realFps, float ramUsageMB, int highActorsCount)
	{
		uint64_t currentFrame = ++g_frameCounter;

		// Bypass warm-up frames to ensure startup jitters don't warp benchmark averages
		if (currentFrame <= static_cast<uint64_t>(g_config.warmupFrames)) return;

		std::vector<FrameMetric> batchToQueue;
		{
			std::lock_guard<std::mutex> lock(g_metricsMutex);
			g_metricsBuffer.push_back({ currentFrame, engineDeltaMs, engineRealDeltaMs, realFrameTimeMs, realFps, ramUsageMB, highActorsCount });

			// Append batch to background queue when buffer hits flush limit
			if (g_metricsBuffer.size() >= static_cast<size_t>(g_config.flushInterval)) {
				batchToQueue = std::move(g_metricsBuffer);
				g_metricsBuffer.clear();
			}
		}

		if (!batchToQueue.empty()) {
			std::lock_guard<std::mutex> lock(g_writeMutex);
			g_writeQueue.push(std::move(batchToQueue));
			g_writeCV.notify_one();
		}
	}
}

namespace Hooks
{
	using Update_t = void(RE::Main*);
	static inline REL::Relocation<Update_t*> _Update;

	void Update(RE::Main* const a_this)
	{
		static auto lastTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();

		_Update(a_this);

		std::chrono::duration<float, std::milli> elapsed = currentTime - lastTime;
		float realFrameTimeMs = elapsed.count();
		lastTime = currentTime;

		const auto& runtimeData = a_this->GetRuntimeData();

		if (runtimeData.quitGame) {
			MetricLogger::Shutdown();
		} else {
			auto* timer = RE::BSTimer::GetSingleton();
			if (timer) {
				float engineDeltaMs = timer->delta * 1000.0f;
				float engineRealDeltaMs = timer->realTimeDelta * 1000.0f;
				float realFps = (realFrameTimeMs > 0.0f) ? (1000.0f / realFrameTimeMs) : 0.0f;

				float ramUsageMB = g_config.logRAM ? GetRAMUsageMB() : 0.0f;
				int highActorsCount = g_config.logHighActors ? GetHighActorsCount() : 0;

				MetricLogger::Record(engineDeltaMs, engineRealDeltaMs, realFrameTimeMs, realFps, ramUsageMB, highActorsCount);
			}
		}
	}

	void Hook()
	{
		WriteImmediateLog("DEBUG: MainHooks::Hook Detour Initiated");

		// RE::Main::Update hook address
		REL::Relocation<uintptr_t> UpdateHook1{ REL::VariantID(35551, 36544, 0x05B6D70), REL::VariantOffset(0x11F, 0x160, 0x11F) };

		auto& trampoline = SKSE::GetTrampoline();
		SKSE::AllocTrampoline(14);

		_Update = trampoline.write_call<5>(UpdateHook1.address(), Update);

		WriteImmediateLog("DEBUG: MainHooks::Hook Success");
	}
}

std::filesystem::path GetCSVPath()
{
	char my_documents[WINDOWS_MAX_PATH];
	HRESULT result = SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, my_documents);
	if (result == S_OK) {
		std::filesystem::path path(my_documents);
		path /= "My Games";
		path /= "Skyrim Special Edition";
		path /= "SKSE";
		return path / g_config.outputFileName;
	}
	return std::filesystem::path(g_config.outputFileName);
}

void InitializeLog()
{
	auto path = SKSE::log::log_directory();
	if (!path) {
		return;
	}
	*path /= "skyrim_perf_monitor.log";
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
	auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
#endif
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%l] %v");
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	WriteImmediateLog("DEBUG: SKSEPlugin_Query");
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "skyrim_perf_monitor";
	a_info->version = 1;

	if (a_skse->IsEditor()) {
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (REL::Module::IsSE() && ver < SKSE::RUNTIME_SSE_1_5_39 || REL::Module::IsVR() && ver < SKSE::RUNTIME_LATEST_VR) {
		return false;
	}

	return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;

	v.PluginVersion({ 1, 0, 0 });
	v.PluginName("skyrim_perf_monitor");
	v.AuthorName("Antigravity");
	v.UsesAddressLibrary();
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST_SE, SKSE::RUNTIME_SSE_LATEST, SKSE::RUNTIME_1_6_1179, SKSE::RUNTIME_LATEST_VR });
	v.UsesNoStructs();

	return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	WriteImmediateLog("DEBUG: SKSEPlugin_Load Started");

	SKSE::Init(a_skse);
	InitializeLog();

	spdlog::info("skyrim_perf_monitor general-purpose benchmark loaded successfully!");

	// Load dynamic .ini preferences
	LoadConfig();

	// Initialize the CSV metric logging system
	MetricLogger::Initialize(GetCSVPath());

	// Install the RE::Main::Update Trampoline hook
	Hooks::Hook();

	return true;
}
