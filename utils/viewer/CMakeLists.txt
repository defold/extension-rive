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

if(MSVC)
    string(REPLACE "/DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
else()
    string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.." EXTENSION_ROOT)

set(ASAN_SUPPORTED FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(ASAN_SUPPORTED TRUE)
endif()
if(WITH_ASAN AND NOT ASAN_SUPPORTED)
    message(WARNING "Address Sanitizer is not supported for ${CMAKE_CXX_COMPILER_ID}; disabling")
    set(WITH_ASAN OFF)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(RIVE_BLIT_SHADER_VP "${EXTENSION_ROOT}/defold-rive/assets/shader-library/rivemodel_blit.vp")
set(RIVE_BLIT_SHADER_FP "${EXTENSION_ROOT}/defold-rive/assets/shader-library/rivemodel_blit.fp")
set(RIVE_BLIT_SHADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders.cpp")
set(RIVE_SHADER_GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/gen_shaders.py")

set(BOB_PATH $ENV{BOB})
if(NOT BOB_PATH)
    if(DEFINED ENV{DYNAMO_HOME})
        set(BOB_PATH "$ENV{DYNAMO_HOME}/share/java/bob.jar")
    else()
        set(BOB_PATH "${EXTENSION_ROOT}/bob.jar")
    endif()
endif()

add_custom_command(
    OUTPUT "${RIVE_BLIT_SHADER_OUT}"
    COMMAND "${Python3_EXECUTABLE}" "${RIVE_SHADER_GEN_SCRIPT}"
        --bob "${BOB_PATH}"
        --platform "${TARGET_PLATFORM}"
        --content-root "${EXTENSION_ROOT}"
        --vertex "${RIVE_BLIT_SHADER_VP}"
        --fragment "${RIVE_BLIT_SHADER_FP}"
        --output "${RIVE_BLIT_SHADER_OUT}"
    DEPENDS
        "${RIVE_SHADER_GEN_SCRIPT}"
        "${RIVE_BLIT_SHADER_VP}"
        "${RIVE_BLIT_SHADER_FP}"
    VERBATIM
)

set(VIEWER_SHADER_SOURCE "${RIVE_BLIT_SHADER_OUT}")

add_executable(viewer
    "${CMAKE_CURRENT_SOURCE_DIR}/viewer.cpp"
    "${VIEWER_SHADER_SOURCE}"
)

file(GLOB RIVE_COMMON_SRC
    "${EXTENSION_ROOT}/defold-rive/commonsrc/*.cpp"
    "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/*.cpp"
)
target_sources(viewer PRIVATE ${RIVE_COMMON_SRC})

target_include_directories(viewer PRIVATE
    "${EXTENSION_ROOT}/defold-rive/include"
    "${EXTENSION_ROOT}/defold-rive/commonsrc"
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
    target_link_directories(viewer PRIVATE "${EXTENSION_ROOT}/defold-rive/lib/${TARGET_PLATFORM}")
endif()

set(TARGET_LIBS
    ddf
    dlib
    profile_null
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

set(USE_VULKAN_BACKEND FALSE)
if(${TARGET_PLATFORM} MATCHES "macos" OR ${TARGET_PLATFORM} MATCHES "linux")
    set(USE_VULKAN_BACKEND TRUE)
endif()

if(USE_VULKAN_BACKEND)
    target_compile_definitions(viewer PRIVATE DM_GRAPHICS_USE_VULKAN)
    list(APPEND TARGET_LIBS
        platform_vulkan
        graphics_vulkan
        glfw3
    )
endif()

if(${TARGET_PLATFORM} MATCHES "macos")
    file(GLOB RIVE_COMMON_OBJCXX_SRC
        "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/*.mm"
    )
    target_sources(viewer PRIVATE ${RIVE_COMMON_OBJCXX_SRC})

    list(APPEND TARGET_LIBS
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

    target_compile_definitions(viewer PRIVATE DM_PLATFORM_MACOS VIEWER_USE_SPIRV=1)
endif()

if(${TARGET_PLATFORM} MATCHES "win32")
    target_compile_definitions(viewer PRIVATE DM_PLATFORM_WINDOWS)
elseif(${TARGET_PLATFORM} MATCHES "linux")
    target_compile_definitions(viewer PRIVATE DM_PLATFORM_LINUX)
    list(APPEND TARGET_LIBS
        vulkan
    )
    target_link_libraries(viewer PRIVATE pthread dl m)
endif()

target_link_libraries(viewer PRIVATE ${TARGET_LIBS})

target_compile_options(viewer PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/Zi>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Release>>>:/Od>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Release>>>:/Zi>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O2>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-g>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<NOT:$<CONFIG:Release>>>:-O0>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<NOT:$<CONFIG:Release>>>:-g>
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(viewer PRIVATE -fno-rtti)
endif()
if(WITH_ASAN)
    target_compile_options(viewer PRIVATE -fsanitize=address)
    target_link_options(viewer PRIVATE -fsanitize=address)
endif()

target_compile_definitions(viewer PRIVATE DLIB_LOG_DOMAIN=\"VIEWER\" DM_RIVE_VIEWER)

set_target_properties(viewer PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
