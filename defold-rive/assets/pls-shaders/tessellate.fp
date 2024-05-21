#version 460
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define OPTIONALLY_FLAT flat

#include "generated/glsl.minified.glsl"
#include "generated/constants.minified.glsl"
#include "generated/common.minified.glsl"
#include "generated/tessellate.minified.glsl"
