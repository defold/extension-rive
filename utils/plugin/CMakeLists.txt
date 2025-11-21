cmake_minimum_required(VERSION 3.24)
project(defold_rive_plugin LANGUAGES C CXX)

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
  message(FATAL_ERROR "Java executable not found on PATH (required for version 25)")
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
  message(FATAL_ERROR "Environment variable BOB must be set to bob.jar")
endif()
set(BOB_JAR "$ENV{BOB}")
if(NOT EXISTS "${BOB_JAR}")
    message(FATAL_ERROR "BOB=${BOB_JAR} does not exist!")
endif()
message(STATUS "BOB=${BOB_JAR}")

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
string(REGEX MATCH "25" _JAVA_MATCH "${JAVA_VERSION_OUTPUT}")
if(NOT _JAVA_MATCH)
  message(FATAL_ERROR "Java 25 is required (found: ${JAVA_VERSION_OUTPUT})")
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

file(GLOB_RECURSE COMMON_SOURCES
    CONFIGURE_DEPENDS
    "${REPO_ROOT}/defold-rive/commonsrc/*.c"
    "${REPO_ROOT}/defold-rive/commonsrc/*.cpp"
    "${REPO_ROOT}/defold-rive/commonsrc/*.cc"
    "${REPO_ROOT}/defold-rive/commonsrc/*.cxx"
)

file(GLOB_RECURSE JAVA_SOURCES
    CONFIGURE_DEPENDS
    "${REPO_ROOT}/defold-rive/pluginsrc/com/*.java"
)

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
else()
  set(PLUGIN_JAR "")
endif()

add_library(rive_plugin SHARED ${ALL_SOURCES})

target_include_directories(rive_plugin PRIVATE
    "${DYNAMO_HOME}/include"
    "${DYNAMO_HOME}/sdk/include"
    "${REPO_ROOT}/defold-rive/include"
    "${REPO_ROOT}/defold-rive/commonsrc"
    "${REPO_ROOT}/defold-rive/pluginsrc"
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
