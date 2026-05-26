cmake_minimum_required(VERSION 3.22)

project(
	skyrim_perf_monitor
	VERSION 1.0.0
	LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

# Set compiled plugins path from environment or default to dist folder
if(NOT DEFINED CompiledPluginsPath)
	if(DEFINED ENV{CompiledPluginsPath})
		set(CompiledPluginsPath "$ENV{CompiledPluginsPath}")
	else()
		set(CompiledPluginsPath "${PROJECT_SOURCE_DIR}/dist")
	endif()
endif()

# Find CommonLibSSE from the existing hdtSMP64 path
set(CommonLibSSEPath "C:/dev/hdtSMP64/extern/CommonLibSSE")
if(NOT EXISTS "${CommonLibSSEPath}")
	message(FATAL_ERROR "Could not find CommonLibSSE at ${CommonLibSSEPath}")
endif()

# Configure CommonLibSSE options prior to including it
set(BUILD_TESTS OFF CACHE BOOL "Disable unit tests for CommonLibSSE" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "Enable trampoline support for Xbyak" FORCE)

add_subdirectory("${CommonLibSSEPath}" CommonLibSSE EXCLUDE_FROM_ALL)

# Add our shared library plugin target
add_library("${PROJECT_NAME}" SHARED
	"src/main.cpp"
)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE "/sdl" # Enable Additional Security Checks
				"/utf-8" # Set Source and Executable character sets to UTF-8
				"/Zi" # Debug Information Format
				"/permissive-" # Standards conformance
				"/Zc:preprocessor" # Enable preprocessor conformance mode
				"/bigobj" # Increase Number of Sections in .obj file
				"/WX" # Warnings as errors
				"$<$<OR:$<CONFIG:RELEASE>,$<CONFIG:PROFILE>>:/Zc:inline;/JMC-;/Ob3>")

	target_link_options("${PROJECT_NAME}" PRIVATE "$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
						"$<$<OR:$<CONFIG:RELEASE>,$<CONFIG:PROFILE>>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>")
endif()

target_include_directories("${PROJECT_NAME}" PRIVATE "src")

# We link against CommonLibSSE
target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE CommonLibSSE::CommonLibSSE
)

install(
	FILES "$<TARGET_FILE:${PROJECT_NAME}>"
	DESTINATION "SKSE/Plugins"
	COMPONENT "main")

install(
	FILES "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
	DESTINATION "/"
	COMPONENT "pdbs")

# Custom command to copy output after build
add_custom_command(
	TARGET "${PROJECT_NAME}"
	POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${CompiledPluginsPath}/SKSE/Plugins/"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>"
			"${CompiledPluginsPath}/SKSE/Plugins/"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
			"${CompiledPluginsPath}/SKSE/Plugins/"
	VERBATIM)
