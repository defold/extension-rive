// Generated. Do not edit! See ./utils/write_rive_version_header.py

//fix: GL async shader compilation improvements/fixes (#10544) 2d7b1b788f
//This improves the handling of GL parallel shader compile and the ubershader fallback, centered around fixing the Galaxy S22 goldens
//
//- GL ubershaders needed to not set ENABLE_CLIP_RECT in MSAA mode when missing the EXT_clip_cull_distance extension
//- avoidMaxShaderCompilerThreadsKHR is no longer necessary because the extension is actually working as expected
//- Similarly an explicit null check on glMaxShaderCompilerThreadsKHR is no longer necessary because the Android egl extension function loading code already does so
//- The recommended way to use KHR_parallel_shader_compile is to compile the shaders, then start the link, then wait on the link to finish (and then display compilation errors after that if compilation failed). This simplifies the compilation handling
//- There was an assert in tryGetPipeline that was intended as a "this should be impossible" test but it turns out it is possible (when the ubershader had completed, but errored), so now on an ubershader error case we will return nullptr (and let the renderer not draw) rather than assert
//
//Co-authored-by: Josh Jersild <joshua@rive.app>
const char* RIVE_RUNTIME_AUTHOR="JoshJRive";
const char* RIVE_RUNTIME_DATE="2025-09-10T00:51:23Z";
const char* RIVE_RUNTIME_SHA1="17be3ff7e08c342d24c25482027ad709c951d38a";
