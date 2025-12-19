cmake_minimum_required(VERSION 3.16)
project(viewer LANGUAGES CXX)

set(DYNAMO_HOME $ENV{DYNAMO_HOME})
if(NOT DYNAMO_HOME)
    message(FATAL_ERROR "DYNAMO_HOME environment variable is required")
endif()

if(NOT TARGET_PLATFORM)
    set(TARGET_PLATFORM $ENV{TARGET_PLATFORM})
endif()

if(NOT TARGET_PLATFORM)
    message(FATAL_ERROR "TARGET_PLATFORM must be supplied as a CMake variable or environment variable")
endif()

if(${TARGET_PLATFORM} MATCHES "macos")
    enable_language(OBJCXX)
endif()

option(WITH_ASAN "Build viewer with Address Sanitizer" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(ASAN_SUPPORTED FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(ASAN_SUPPORTED TRUE)
endif()
if(WITH_ASAN AND NOT ASAN_SUPPORTED)
    message(WARNING "Address Sanitizer is not supported for ${CMAKE_CXX_COMPILER_ID}; disabling")
    set(WITH_ASAN OFF)
endif()
add_executable(viewer
    "${CMAKE_CURRENT_SOURCE_DIR}/viewer.cpp"
)

file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.." EXTENSION_ROOT)

file(GLOB RIVE_COMMON_SRC
    "${EXTENSION_ROOT}/defold-rive/commonsrc/*.cpp"
    "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/*.cpp"
)
target_sources(viewer PRIVATE ${RIVE_COMMON_SRC})

target_include_directories(viewer PRIVATE
    "${EXTENSION_ROOT}/defold-rive/include"
    "${DYNAMO_HOME}/sdk/include/${TARGET_PLATFORM}"
    "${DYNAMO_HOME}/sdk/include"
    "${DYNAMO_HOME}/include/${TARGET_PLATFORM}"
    "${DYNAMO_HOME}/include"
    "${DYNAMO_HOME}/ext/include/${TARGET_PLATFORM}"
    "${DYNAMO_HOME}/ext/include"
)

target_link_directories(viewer PRIVATE
    "${DYNAMO_HOME}/lib/${TARGET_PLATFORM}"
    "${DYNAMO_HOME}/ext/lib/${TARGET_PLATFORM}"
)

if(${TARGET_PLATFORM} MATCHES "macos")
    target_link_directories(viewer PRIVATE "${EXTENSION_ROOT}/defold-rive/lib/arm64-osx")
else()
    target_link_directories(viewer PRIVATE "${EXTENSION_ROOT}/defold-rive/lib/{TARGET_PLATFORM}")
endif()

set(TARGET_LIBS
    dlib
    profile
    profile_basic
    profilerext
    platform
    graphics

    rive
    rive_decoders
    rive_harfbuzz
    rive_pls_renderer
    rive_sheenbidi
    rive_yoga
    libjpeg
    libpng
    libwebp
    miniaudio
)

if(${TARGET_PLATFORM} MATCHES "macos")
    file(GLOB RIVE_COMMON_OBJCXX_SRC
        "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/*.mm"
    )
    target_sources(viewer PRIVATE ${RIVE_COMMON_OBJCXX_SRC})

    list(APPEND TARGET_LIBS
        platform_vulkan
        graphics_vulkan
        glfw3
        MoltenVK
    )
    target_link_libraries(viewer PRIVATE
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework IOKit"
        "-framework IOSurface"
        "-framework AppKit"
        "-framework Metal"
        "-framework QuartzCore")

    target_compile_definitions(viewer PRIVATE DM_PLATFORM_MACOS)
endif()

target_link_libraries(viewer PRIVATE ${TARGET_LIBS})

target_compile_options(viewer PRIVATE
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Release>:-g>
    $<$<NOT:$<CONFIG:Release>>:-O0>
    $<$<NOT:$<CONFIG:Release>>:-g>
)
if(WITH_ASAN)
    target_compile_options(viewer PRIVATE -fsanitize=address)
    target_link_options(viewer PRIVATE -fsanitize=address)
endif()

target_compile_definitions(viewer PRIVATE DLIB_LOG_DOMAIN=VIEWER)

set_target_properties(viewer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
