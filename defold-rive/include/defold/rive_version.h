// Generated. Do not edit! See ./utils/write_rive_version_header.py

//fix: Always use WGPUWagyuShaderLanguage_GLSLRAW on Wagyu/GLES (#10372) 0cc0b7e63c
//Rive shaders tend to be long and prone to vendor bugs in the compiler.
//Always send down the raw Rive GLSL sources when running on Wagyu/GLES,
//which have various workarounds for known issues and are tested
//regularly.
//
//Co-authored-by: Chris Dalton <99840794+csmartdalton@users.noreply.github.com>
const char* RIVE_RUNTIME_AUTHOR="csmartdalton";
const char* RIVE_RUNTIME_DATE="2025-08-14T00:38:27Z";
const char* RIVE_RUNTIME_SHA1="3f6e5319f6998d54cd756faf5f82e4e05e4f23b9";
