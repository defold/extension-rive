cmake_minimum_required(VERSION 3.24)
project(defold_rive_plugin LANGUAGES C CXX)

if(POLICY CMP0152)
    cmake_policy(SET CMP0152 NEW)
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE)

if(TARGET_PLATFORM STREQUAL "x86_64-win32")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Zi")
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

option(TARGET_PLATFORM "" "Target platform identifier, e.g. x86_64-linux, x86_64-win32, x86_64-macos, arm64-macos")
if(NOT TARGET_PLATFORM)
    message(FATAL_ERROR "TARGET_PLATFORM must be provided (via -DTARGET_PLATFORM=...)")
endif()

if(NOT DEFINED ENV{DYNAMO_HOME} OR "$ENV{DYNAMO_HOME}" STREQUAL "")
    message(FATAL_ERROR "DYNAMO_HOME environment variable is required")
endif()

set(DYNAMO_HOME "$ENV{DYNAMO_HOME}")

find_program(JAVA_EXECUTABLE NAMES java)
if(NOT JAVA_EXECUTABLE)
    message(FATAL_ERROR "Java executable not found on PATH")
endif()

find_program(JAVAC_EXECUTABLE NAMES javac)
if(NOT JAVAC_EXECUTABLE)
    message(FATAL_ERROR "javac executable not found on PATH")
endif()

find_program(JAR_EXECUTABLE NAMES jar)
if(NOT JAR_EXECUTABLE)
    message(FATAL_ERROR "jar executable not found on PATH")
endif()

if(NOT DEFINED ENV{BOB} OR "$ENV{BOB}" STREQUAL "")
    message(FATAL_ERROR "Environment variable BOB must be set to the bob.jar classpath")
endif()

set(BOB_JAR "$ENV{BOB}")
if(NOT EXISTS "${BOB_JAR}")
    message(FATAL_ERROR "BOB=${BOB_JAR} does not exist")
endif()
message(STATUS "BOB=${BOB_JAR}")

if(NOT EXISTS "${DYNAMO_HOME}/ext/bin")
    message(FATAL_ERROR "Dynamo extension bin directory missing under '${DYNAMO_HOME}/ext/bin'")
endif()

find_program(PROTOC_EXECUTABLE NAMES protoc)
if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc executable not found on PATH")
endif()
message(STATUS "PROTOC=${PROTOC_EXECUTABLE}")

execute_process(
    COMMAND "${JAVA_EXECUTABLE}" --version
    RESULT_VARIABLE JAVA_VERSION_RESULT
    OUTPUT_VARIABLE JAVA_VERSION_STDOUT
    ERROR_VARIABLE JAVA_VERSION_STDERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT JAVA_VERSION_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to run 'java --version': ${JAVA_VERSION_STDERR}")
endif()

set(JAVA_VERSION_OUTPUT "${JAVA_VERSION_STDOUT}\n${JAVA_VERSION_STDERR}")
string(REGEX MATCH "21" _JAVA_MATCH "${JAVA_VERSION_OUTPUT}")
if(NOT _JAVA_MATCH)
    message(FATAL_ERROR "Java 21 is required (found: ${JAVA_VERSION_OUTPUT})")
endif()

if(NOT DEFINED ENV{JAVA_HOME} OR "$ENV{JAVA_HOME}" STREQUAL "")
    execute_process(
        COMMAND "${JAVA_EXECUTABLE}" -XshowSettings:properties -version
        RESULT_VARIABLE JAVA_HOME_RESULT
        OUTPUT_VARIABLE JAVA_HOME_STDOUT
        ERROR_VARIABLE JAVA_HOME_STDERR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(NOT JAVA_HOME_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to resolve JAVA_HOME via 'java -XshowSettings:properties': ${JAVA_HOME_STDERR}")
    endif()
    string(REGEX MATCH "java\\.home = ([^\r\n]+)" _JAVA_HOME_MATCH "${JAVA_HOME_STDOUT}\n${JAVA_HOME_STDERR}")
    if(NOT _JAVA_HOME_MATCH)
        message(FATAL_ERROR "Unable to parse java_home from 'java -XshowSettings:properties'")
    endif()
    set(JAVA_HOME "${CMAKE_MATCH_1}")
else()
    set(JAVA_HOME "$ENV{JAVA_HOME}")
endif()

message(STATUS "JAVA_HOME=${JAVA_HOME}")
message(STATUS "DYNAMO_HOME=${DYNAMO_HOME}")
message(STATUS "TARGET_PLATFORM=${TARGET_PLATFORM}")
if(TARGET_PLATFORM MATCHES "arm64")
    set(TARGET_ARCH "arm64")
elseif(TARGET_PLATFORM MATCHES "x86_64")
    set(TARGET_ARCH "x86_64")
elseif(TARGET_PLATFORM MATCHES "x86")
    set(TARGET_ARCH "x86")
else()
    set(TARGET_ARCH "")
endif()
message(STATUS "TARGET_ARCH=${TARGET_ARCH}")

if(TARGET_ARCH AND CMAKE_HOST_APPLE)
    set(CMAKE_OSX_ARCHITECTURES "${TARGET_ARCH}")
endif()

file(GLOB_RECURSE COMMON_SOURCES
        CONFIGURE_DEPENDS
        "${REPO_ROOT}/defold-rive/commonsrc/*.c"
        "${REPO_ROOT}/defold-rive/commonsrc/*.cpp"
        "${REPO_ROOT}/defold-rive/commonsrc/*.cc"
        "${REPO_ROOT}/defold-rive/commonsrc/*.cxx"
)

# Rust file?
file(GLOB_RECURSE GLOB_JAVA_SOURCES
    CONFIGURE_DEPENDS
    "${REPO_ROOT}/defold-rive/pluginsrc/com/*.java"
)
set(GENERATED_PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-java")
set(GENERATED_PROTO_JAVA "${GENERATED_PROTO_DIR}/com/dynamo/rive/proto/Rive.java")
set(PROTO_FILE "${REPO_ROOT}/defold-rive/commonsrc/rive_ddf.proto")

add_custom_command(
  OUTPUT "${GENERATED_PROTO_JAVA}"
  COMMAND
    "${CMAKE_COMMAND}" -E make_directory "${GENERATED_PROTO_DIR}/com/dynamo/rive/proto"
  COMMAND
    "${PROTOC_EXECUTABLE}"
    --proto_path="${REPO_ROOT}/defold-rive/commonsrc/"
    --java_out="${GENERATED_PROTO_DIR}"
    "-I${DYNAMO_HOME}/share/proto"
    "-I${DYNAMO_HOME}/ext/include"
    "${PROTO_FILE}"
  DEPENDS "${PROTO_FILE}"
  WORKING_DIRECTORY "${REPO_ROOT}"
  COMMENT "Generating Java sources via protoc"
)
add_custom_target(plugin_proto ALL DEPENDS "${GENERATED_PROTO_JAVA}")

set(JAVA_SOURCES ${GLOB_JAVA_SOURCES} "${GENERATED_PROTO_JAVA}")

file(GLOB_RECURSE PLUGIN_SOURCES
        CONFIGURE_DEPENDS
        "${REPO_ROOT}/defold-rive/pluginsrc/*.c"
        "${REPO_ROOT}/defold-rive/pluginsrc/*.cpp"
        "${REPO_ROOT}/defold-rive/pluginsrc/*.cc"
        "${REPO_ROOT}/defold-rive/pluginsrc/*.cxx"
)

set(ALL_SOURCES ${COMMON_SOURCES} ${PLUGIN_SOURCES})
list(REMOVE_DUPLICATES ALL_SOURCES)

if(NOT ALL_SOURCES)
    message(FATAL_ERROR "No C/C++ sources found under defold-rive/commonsrc or defold-rive/pluginsrc")
endif()

add_compile_definitions(DLIB_LOG_DOMAIN="RIVE")

if(JAVA_SOURCES)
    set(JAVA_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/java/classes")
    set(PLUGIN_JAR "${CMAKE_CURRENT_BINARY_DIR}/pluginRiveExt.jar")
    add_custom_command(
        OUTPUT "${PLUGIN_JAR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${JAVA_CLASSES_DIR}"
        COMMAND "${JAVAC_EXECUTABLE}" -d "${JAVA_CLASSES_DIR}" -classpath "${BOB_JAR}" ${JAVA_SOURCES}
        COMMAND "${JAR_EXECUTABLE}" cf "${PLUGIN_JAR}" -C "${JAVA_CLASSES_DIR}" .
        DEPENDS ${JAVA_SOURCES}
        WORKING_DIRECTORY "${REPO_ROOT}"
        COMMENT "Compiling Defold Rive plugin Java sources"
    )
    add_custom_target(plugin_java ALL DEPENDS "${PLUGIN_JAR}")
    add_dependencies(plugin_java plugin_proto)
else()
    set(PLUGIN_JAR "")
endif()

add_library(rive_plugin SHARED ${ALL_SOURCES})

target_include_directories(rive_plugin PRIVATE
        "${REPO_ROOT}/defold-rive/include"
        "${REPO_ROOT}/defold-rive/commonsrc"
        "${REPO_ROOT}/defold-rive/pluginsrc"
        "${DYNAMO_HOME}/include"
        "${DYNAMO_HOME}/sdk/include"
)

target_include_directories(rive_plugin PRIVATE
        "${JAVA_HOME}/include"
)

target_link_libraries(rive_plugin PRIVATE
        dlib mbedtls ddf
        profile_null profilerext_null
        platform_null render graphics_null
        resource script gamesys lua
        rivetess tess2
        rive rive_sheenbidi rive_harfbuzz rive_yoga zlib)

set(EXTENDER_PLATFORM "${TARGET_PLATFORM}")
if(TARGET_PLATFORM STREQUAL "arm64-macos")
        set(EXTENDER_PLATFORM "arm64-osx")
elseif(TARGET_PLATFORM STREQUAL "x86_64-macos")
        set(EXTENDER_PLATFORM "x86_64-osx")
endif()
message(STATUS "EXTENDER_PLATFORM=${EXTENDER_PLATFORM}")

set(DYNAMO_LINK_DIRS
        "${REPO_ROOT}/defold-rive/lib/${EXTENDER_PLATFORM}"
        "${DYNAMO_HOME}/lib/${TARGET_PLATFORM}"
        "${DYNAMO_HOME}/ext/lib/${TARGET_PLATFORM}"
)

foreach(dir IN LISTS DYNAMO_LINK_DIRS)
    if(EXISTS "${dir}")
        target_link_directories(rive_plugin PRIVATE "${dir}")
        message("LIBPATH+=${dir}")
    else()
        message(FATAL_ERROR "Directory doesn't exist: ${dir}")
    endif()
endforeach()

set_target_properties(rive_plugin PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "RiveExt"
)

if(WIN32)
    set_target_properties(rive_plugin PROPERTIES PREFIX "lib")
else()
    set_target_properties(rive_plugin PROPERTIES PREFIX "lib")
endif()

if(TARGET_PLATFORM MATCHES "macos")
    target_compile_options(rive_plugin PRIVATE "-mmacosx-version-min=10.15")
    target_include_directories(rive_plugin PRIVATE
        "${REPO_ROOT}/defold-rive/include/osx"
    )
    target_link_libraries(rive_plugin PRIVATE "-framework CoreFoundation")
    target_link_libraries(rive_plugin PRIVATE "-framework Foundation")
    target_link_libraries(rive_plugin PRIVATE "-framework AppKit")
    target_compile_options(rive_plugin PRIVATE "-fno-exceptions" "-fno-rtti")
    set(JAVA_PLATFORM "darwin")
elseif(TARGET_PLATFORM MATCHES "win32")
    target_include_directories(rive_plugin PRIVATE
        "${REPO_ROOT}/defold-rive/include/win32"
    )
    set(JAVA_PLATFORM "win32")
elseif(TARGET_PLATFORM MATCHES "linux")
    target_include_directories(rive_plugin PRIVATE
        "${REPO_ROOT}/defold-rive/include/linux"
    )
    target_compile_options(rive_plugin PRIVATE "-fno-exceptions" "-fno-rtti")
    set(JAVA_PLATFORM "linux")
endif()

if(MSVC)
    target_compile_options(rive_plugin PRIVATE /bigobj)
endif()
