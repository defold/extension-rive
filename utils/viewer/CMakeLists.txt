cmake_minimum_required(VERSION 3.16)
project(viewer LANGUAGES CXX)
include(CheckCXXCompilerFlag)

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

if(${TARGET_PLATFORM} MATCHES "win32" AND NOT MSVC)
    message(FATAL_ERROR
        "TARGET_PLATFORM=${TARGET_PLATFORM} must be built with MSVC on Windows. "
        "Use ./utils/viewer/build.sh x86_64-win32 (which selects Visual Studio generator) "
        "or configure CMake with -G \"Visual Studio 17 2022\" -A x64.")
endif()

option(WITH_ASAN "Build viewer with Address Sanitizer" OFF)
option(WITH_VULKAN "Build viewer with Vulkan graphics backend" ON)
option(WITH_OPENGL "Build viewer with OpenGL graphics backend" OFF)

if(WITH_VULKAN AND WITH_OPENGL)
    message(FATAL_ERROR "WITH_VULKAN and WITH_OPENGL cannot both be ON")
endif()

if(NOT WITH_VULKAN AND NOT WITH_OPENGL)
    set(WITH_VULKAN ON)
endif()

set(WITH_METAL FALSE)
if(TARGET_PLATFORM MATCHES "macos" AND WITH_VULKAN)
    set(WITH_METAL TRUE)
endif()

if(WITH_METAL)
    enable_language(OBJCXX)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

function(strip_ndebug flag_var)
    if(DEFINED ${flag_var})
        set(_value "${${flag_var}}")
        string(REPLACE "/DNDEBUG" "" _value "${_value}")
        string(REPLACE "-DNDEBUG" "" _value "${_value}")
        set(${flag_var} "${_value}" PARENT_SCOPE)
    endif()
endfunction()

foreach(_flag_var IN ITEMS
    CMAKE_C_FLAGS
    CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO
    CMAKE_C_FLAGS_MINSIZEREL
    CMAKE_CXX_FLAGS
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_RELWITHDEBINFO
    CMAKE_CXX_FLAGS_MINSIZEREL
)
    strip_ndebug(${_flag_var})
endforeach()

unset(_flag_var)

file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.." EXTENSION_ROOT)

set(VIEWER_WIN32_RIVE_LIB_DIR "" CACHE PATH
    "Override directory containing Windows Rive static libraries for viewer")

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

# ****************************************************************************************************************
# Sources

file(GLOB RIVE_COMMON_SRC
    "${EXTENSION_ROOT}/defold-rive/commonsrc/*.cpp"
    "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/renderer_private.cpp"
)

if(WITH_METAL)
    list(APPEND RIVE_COMMON_SRC "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/renderer_context_metal.mm")

    file(GLOB RIVE_COMMON_OBJCXX_SRC
        "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/*.mm"
    )
    target_sources(viewer PRIVATE ${RIVE_COMMON_OBJCXX_SRC})

elseif(WITH_VULKAN)
    list(APPEND RIVE_COMMON_SRC "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/renderer_context_vulkan.cpp")
elseif(WITH_OPENGL)
    list(APPEND RIVE_COMMON_SRC "${EXTENSION_ROOT}/defold-rive/commonsrc/renderer/renderer_context_opengl.cpp")
else()
    message(FATAL_ERROR "No renderer context selected for viewer")
endif()

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

set(RIVE_TARGET_PLATFORM "${TARGET_PLATFORM}")
if(TARGET_PLATFORM STREQUAL "arm64-macos")
    set(RIVE_TARGET_PLATFORM "arm64-osx")
elseif(TARGET_PLATFORM STREQUAL "x86_64-macos")
    set(RIVE_TARGET_PLATFORM "x86_64-osx")
endif()

set(RIVE_LINK_DIR "${EXTENSION_ROOT}/defold-rive/lib/${RIVE_TARGET_PLATFORM}")

if(NOT EXISTS "${RIVE_LINK_DIR}")
    message(FATAL_ERROR "Rive library directory does not exist: ${RIVE_LINK_DIR}")
endif()

target_link_directories(viewer PRIVATE "${RIVE_LINK_DIR}")


# ******************************************************************************************************
# Defines

if(TARGET_PLATFORM MATCHES "macos")
    target_compile_definitions(viewer PRIVATE
        DM_PLATFORM_MACOS
        DM_GRAPHICS_USE_METAL)

elseif(TARGET_PLATFORM MATCHES "win32")
    target_compile_definitions(viewer PRIVATE
            DM_PLATFORM_WINDOWS
            RIVE_WINDOWS
            NOMINMAX
            WIN32_LEAN_AND_MEAN)

elseif(TARGET_PLATFORM MATCHES "linux")
    target_compile_definitions(viewer PRIVATE
        DM_PLATFORM_LINUX
        RIVE_LINUX)
endif()

if(WITH_VULKAN)
    target_compile_definitions(viewer PRIVATE DM_GRAPHICS_USE_VULKAN DM_RIVE_USE_VULKAN RIVE_VULKAN)
    if(WITH_METAL)
        target_compile_definitions(viewer PRIVATE DM_GRAPHICS_USE_METAL VIEWER_USE_SPIRV=1)
    endif()
elseif(WITH_OPENGL)
    target_compile_definitions(viewer PRIVATE DM_GRAPHICS_USE_OPENGL DM_RIVE_USE_OPENGL RIVE_DESKTOP_GL)
else()
    message(FATAL_ERROR "No Windows viewer backend selected")
endif()

# ******************************************************************************************************
# Libraries

if(${TARGET_PLATFORM} MATCHES "win32")
    set(ENGINE_LIBS
        libddf
        libdlib
        libprofile_null
        librender
    )
else()
    set(ENGINE_LIBS
        ddf
        dlib
        profile_null
        render
    )
endif()

set(RIVE_RUNTIME_LIBS
    rive
    rive_harfbuzz
    rive_pls_renderer
    rive_sheenbidi
    rive_yoga
    rive_decoders
    zlib
    libjpeg
    libpng
    libwebp
    miniaudio
)

set(SYSTEM_LIBS "")
set(GRAPHICS_LIBS "")
if(WITH_METAL)
    # macOS viewer uses the Metal renderer path, backed by Defold's Vulkan bridge.
    list(APPEND GRAPHICS_LIBS MoltenVK)
endif()

if(${TARGET_PLATFORM} MATCHES "win32")
    target_compile_definitions(viewer PRIVATE DM_PLATFORM_WINDOWS RIVE_WINDOWS)

    if(WITH_VULKAN)
        list(APPEND GRAPHICS_LIBS libgraphics_vulkan platform_vulkan)
        target_link_libraries(viewer PRIVATE Iphlpapi.lib libglfw3 ws2_32 vulkan-1)
    elseif(WITH_OPENGL)
        list(APPEND GRAPHICS_LIBS libgraphics platform)
        target_link_libraries(viewer PRIVATE Iphlpapi.lib libglfw3 ws2_32 opengl32)
    else()
        message(FATAL_ERROR "No Windows viewer backend selected")
    endif()

    # defold-rive Windows libs are built with /MT.
    set_property(TARGET viewer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

else()
    if(${TARGET_PLATFORM} MATCHES "linux")
        if(WITH_VULKAN)
            find_package(Vulkan REQUIRED)
            set(SYSTEM_LIBS Vulkan::Vulkan pthread m dl)
        else()
            set(OpenGL_GL_PREFERENCE LEGACY)
            find_package(OpenGL REQUIRED)
            set(SYSTEM_LIBS OpenGL::GL pthread m dl)
        endif()
    endif()

    if (WITH_METAL)
        target_link_libraries(viewer PRIVATE
            "-framework CoreGraphics"
            "-framework CoreFoundation"
            "-framework IOKit"
            "-framework IOSurface"
            "-framework AppKit"
            "-framework Metal"
            "-framework QuartzCore")
    endif()

    if(WITH_VULKAN)
        list(APPEND GRAPHICS_LIBS platform_vulkan graphics_vulkan glfw3)
    else()
        list(APPEND GRAPHICS_LIBS platform graphics glfw3)
    endif()

    if(TARGET_PLATFORM MATCHES "macos" AND WITH_VULKAN)
        list(APPEND GRAPHICS_LIBS MoltenVK)
    endif()
endif()

set(TARGET_LIBS
    ${ENGINE_LIBS}
    ${GRAPHICS_LIBS}
    ${RIVE_RUNTIME_LIBS}
    ${SYSTEM_LIBS})

if(TARGET_PLATFORM MATCHES "linux")
    target_link_libraries(viewer PRIVATE "-Wl,--start-group" ${TARGET_LIBS} "-Wl,--end-group")
else()
    target_link_libraries(viewer PRIVATE ${TARGET_LIBS})
endif()

# **************************************************************************************************

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
if(MSVC)
    target_compile_definitions(viewer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(WITH_ASAN)
    target_compile_options(viewer PRIVATE -fsanitize=address)
    target_link_options(viewer PRIVATE -fsanitize=address)
endif()

target_compile_definitions(viewer PRIVATE DLIB_LOG_DOMAIN=\"VIEWER\" DM_RIVE_VIEWER)

if(MSVC)
    target_compile_options(viewer PRIVATE /std:c++20)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    check_cxx_compiler_flag("-std=gnu++20" COMPILER_SUPPORTS_GNUXX20)
    check_cxx_compiler_flag("-std=gnu++2a" COMPILER_SUPPORTS_GNUXX2A)

    if(COMPILER_SUPPORTS_GNUXX20)
        target_compile_options(viewer PRIVATE -std=gnu++20)
    elseif(COMPILER_SUPPORTS_GNUXX2A)
        target_compile_options(viewer PRIVATE -std=gnu++2a)
    else()
        message(FATAL_ERROR
            "Compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} does not support C++20 "
            "(expected -std=gnu++20 or -std=gnu++2a).")
    endif()
endif()

if(MSVC)
    # Make F5 run the viewer target in generated Visual Studio solutions.
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT viewer)
endif()
