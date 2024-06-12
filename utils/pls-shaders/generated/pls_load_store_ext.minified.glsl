#ifdef VERTEX
void main(){gl_Position=vec4(mix(vec2(-1,1),vec2(1,-1),equal(gl_VertexID&ivec2(1,2),ivec2(0))),0,1);}
#endif
#ifdef FRAGMENT
#extension GL_EXT_shader_pixel_local_storage:enable
#extension GL_ARM_shader_framebuffer_fetch:enable
#extension GL_EXT_shader_framebuffer_fetch:enable
#ifdef CLEAR_COLOR
#if __VERSION__>=310
layout(binding=0,std140)uniform bb{uniform highp vec4 oa;}pa;
#else
uniform mediump vec4 WC;
#endif
#endif
#ifdef GL_EXT_shader_pixel_local_storage
#ifdef STORE_COLOR
__pixel_local_inEXT J0
#else
__pixel_local_outEXT J0
#endif
{layout(rgba8)mediump vec4 W;layout(r32ui)highp uint y0;layout(rgba8)mediump vec4 w2;layout(r32ui)highp uint v0;};
#ifndef GL_ARM_shader_framebuffer_fetch
#ifdef LOAD_COLOR
layout(location=0)inout mediump vec4 l6;
#endif
#endif
#ifdef STORE_COLOR
layout(location=0)out mediump vec4 l6;
#endif
void main(){
#ifdef CLEAR_COLOR
#if __VERSION__>=310
W=pa.oa;
#else
W=WC;
#endif
#endif
#ifdef LOAD_COLOR
#ifdef GL_ARM_shader_framebuffer_fetch
W=gl_LastFragColorARM;
#else
W=l6;
#endif
#endif
#ifdef CLEAR_COVERAGE
y0=0u;
#endif
#ifdef CLEAR_CLIP
v0=0u;
#endif
#ifdef STORE_COLOR
l6=W;
#endif
}
#else
layout(location=0)out mediump vec4 qa;void main(){qa=vec4(0,1,0,1);}
#endif
#endif
