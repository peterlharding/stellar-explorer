cmake_minimum_required(VERSION 3.20)

# On Apple platforms, build for the same oldest OS as Unreal 5.8 links the app for, or the
# linker warns that the core was built for a newer OS than the app: macOS 14.0
# (Engine/Config/Mac/Mac_SDK.json) and visionOS 1.0 (the minimum in the app's binary).
# An explicit -DCMAKE_OSX_DEPLOYMENT_TARGET still wins.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
	if(CMAKE_SYSTEM_NAME STREQUAL "visionOS")
		set(CMAKE_OSX_DEPLOYMENT_TARGET "1.0" CACHE STRING "Oldest visionOS to run on" FORCE)
	elseif(NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Oldest macOS to run on" FORCE)
	endif()
endif()

project(stellar_core VERSION 0.1.0 LANGUAGES C CXX)

# The engine-independent core (ADR 0008), built as a static library with a C ABI
# for the Unreal front end to link (ADR 0014). It builds with its own toolchain on
# each target, so the C++ standard here is independent of the engine's.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# SQLite is vendored and compiled in (ADR 0014), as its own target so the project's
# warnings do not apply to third-party code. See third_party/sqlite/README.md.
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_THREADSAFE=1)
target_link_libraries(sqlite3 PUBLIC Threads::Threads)
# No warnings at all for it: Xcode's generator adds warnings of its own (such as
# -Wshorten-64-to-32), and the amalgamation is upstream's to change, not ours.
target_compile_options(sqlite3 PRIVATE
	$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>
	$<$<C_COMPILER_ID:MSVC>:/w>)

add_library(stellar_core STATIC
	src/astrometry.cpp
	src/catalogue.cpp
	src/voice.cpp
	src/api.cpp)
target_include_directories(stellar_core
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(stellar_core PRIVATE sqlite3)
target_compile_options(stellar_core PRIVATE
	$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
	$<$<CXX_COMPILER_ID:MSVC>:/W4>)

# The libraries the app links land in <build>/<config>/ on every generator and SDK:
# Xcode would otherwise put a device build in Release-xros/, not the Release/ that
# src/app/Source/StellarCore/StellarCore.Build.cs looks in.
set_target_properties(sqlite3 stellar_core PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

option(STELLAR_CORE_TESTS "Build the core's tests" ON)
if(STELLAR_CORE_TESTS)
	enable_testing()

	add_executable(test_astrometry tests/test_astrometry.cpp)
	target_link_libraries(test_astrometry PRIVATE stellar_core)
	target_include_directories(test_astrometry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	add_test(NAME astrometry COMMAND test_astrometry)

	# The catalogue test builds its own fixture database, so it links SQLite directly.
	add_executable(test_catalogue tests/test_catalogue.cpp)
	target_link_libraries(test_catalogue PRIVATE stellar_core sqlite3)
	add_test(NAME catalogue COMMAND test_catalogue)

	add_executable(test_voice tests/test_voice.cpp)
	target_link_libraries(test_voice PRIVATE stellar_core)
	target_include_directories(test_voice PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	add_test(NAME voice COMMAND test_voice)
endif()
