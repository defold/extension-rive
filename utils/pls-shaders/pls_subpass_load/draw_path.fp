#version 460
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define PLS_IMPL_SUBPASS_LOAD
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#define ENABLE_EVEN_ODD
#define ENABLE_NESTED_CLIPPING
#define ENABLE_HSL_BLEND_MODES

#include "/utils/pls-shaders/generated/glsl.minified.glsl"
#include "/utils/pls-shaders/generated/constants.minified.glsl"
#include "/utils/pls-shaders/generated/common.minified.glsl"
#include "/utils/pls-shaders/generated/advanced_blend.minified.glsl"
#include "/utils/pls-shaders/generated/draw_path.minified.glsl"
