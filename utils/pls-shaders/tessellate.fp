#version 460
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define OPTIONALLY_FLAT flat

#include "/utils/pls-shaders/generated/glsl.minified.glsl"
#include "/utils/pls-shaders/generated/constants.minified.glsl"
#include "/utils/pls-shaders/generated/common.minified.glsl"
#include "/utils/pls-shaders/generated/tessellate.minified.glsl"
