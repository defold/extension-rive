#version 460
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define PLS_IMPL_RW_TEXTURE
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#define ENABLE_HSL_BLEND_MODES
#define DRAW_IMAGE
#define DRAW_IMAGE_MESH

#include "/utils/pls-shaders/generated/glsl.minified.glsl"
#include "/utils/pls-shaders/generated/constants.minified.glsl"
#include "/utils/pls-shaders/generated/common.minified.glsl"
#include "/utils/pls-shaders/generated/advanced_blend.minified.glsl"
#include "/utils/pls-shaders/generated/draw_image_mesh.minified.glsl"
