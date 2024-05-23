#ifndef GLSL_VERSION
#define GLSL_VERSION __VERSION__
#endif
#define d vec2
#define D1 vec3
#define H3 vec3
#define g vec4
#define h mediump float
#define m0 mediump vec2
#define N mediump vec3
#define i mediump vec4
#define F0 float
#define Z1 vec2
#define k0 vec3
#define N0 vec4
#define n0 ivec2
#define G5 ivec4
#define a6 mediump int
#define A0 uvec2
#define S uvec4
#define L mediump uint
#define P0 uint
#define v mat2
#define j5 mat3x4
#define p
#define y2(h4) out h4
#ifdef GL_ANGLE_base_vertex_base_instance_shader_builtin
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin:require
#endif
#ifdef ENABLE_BINDLESS_TEXTURES
#extension GL_ARB_bindless_texture:require
#endif
#ifdef ENABLE_KHR_BLEND
#extension GL_KHR_blend_equation_advanced:require
#endif
#ifdef USING_DEPTH_STENCIL
#ifdef ENABLE_CLIP_RECT
#ifdef GL_EXT_clip_cull_distance
#extension GL_EXT_clip_cull_distance:require
#elif defined(GL_ANGLE_clip_cull_distance)
#extension GL_ANGLE_clip_cull_distance:require
#endif
#endif
#endif
#if GLSL_VERSION>=310
#define X3(c,a) layout(binding=c,std140)uniform a{
#else
#define X3(c,a) layout(std140)uniform a{
#endif
#define G4(a) }a;
#define V0(a)
#define r0(c,G,a) layout(location=c)in G a
#define W0
#define x0(c6,A,a,G)
#ifdef VERTEX
#if GLSL_VERSION>=310
#define H(c,G,a) layout(location=c)out G a
#else
#define H(c,G,a) out G a
#endif
#else
#if GLSL_VERSION>=310
#define H(c,G,a) layout(location=c)in G a
#else
#define H(c,G,a) in G a
#endif
#endif
#define d6 flat
#define A1
#define B1
#ifdef TARGET_VULKAN
#define l0
#else
#ifdef GL_NV_shader_noperspective_interpolation
#extension GL_NV_shader_noperspective_interpolation:require
#define l0 noperspective
#else
#define l0
#endif
#endif
#ifdef VERTEX
#define O1
#define P1
#endif
#ifdef FRAGMENT
#define F2
#define G2
#endif
#ifdef TARGET_VULKAN
#define P2(c,a) layout(binding=c)uniform highp utexture2D a
#define i4(c,a) layout(binding=c)uniform highp texture2D a
#define x1(c,a) layout(binding=c)uniform mediump texture2D a
#elif GLSL_VERSION>=310
#define P2(c,a) layout(binding=c)uniform highp usampler2D a
#define i4(c,a) layout(binding=c)uniform highp sampler2D a
#define x1(c,a) layout(binding=c)uniform mediump sampler2D a
#else
#define P2(c,a) uniform highp usampler2D a
#define i4(c,a) uniform highp sampler2D a
#define x1(c,a) uniform mediump sampler2D a
#endif
#define Z9(c,a) P2(c,a)
#ifdef TARGET_VULKAN
#define J3(d2,a) layout(binding=d2,set=y7)uniform mediump sampler a;
#define j3(d2,a) layout(binding=d2,set=y7)uniform mediump sampler a;
#define M2(a,q0,F) texture(sampler2D(a,q0),F)
#define R3(a,q0,F,V2) textureLod(sampler2D(a,q0),F,V2)
#define Q3(a,q0,F,W2,X2) textureGrad(sampler2D(a,q0),F,W2,X2)
#else
#define J3(d2,a)
#define j3(d2,a)
#define M2(a,q0,F) texture(a,F)
#define R3(a,q0,F,V2) textureLod(a,F,V2)
#define Q3(a,q0,F,W2,X2) textureGrad(a,F,W2,X2)
#endif
#define I1(a,F) texelFetch(a,F,0)
#define V1
#define W1
#define L3
#define O3
#ifdef DISABLE_SHADER_STORAGE_BUFFERS
#define M3(c,c1,a) P2(c,a)
#define Q2(c,c1,a) Z9(c,a)
#define N3(c,c1,a) i4(c,a)
#define B0(a,C0) I1(a,n0((C0)&k7,(C0)>>j7))
#define q2(a,C0) I1(a,n0((C0)&k7,(C0)>>j7)).xy
#else
#ifdef GL_ARB_shader_storage_buffer_object
#extension GL_ARB_shader_storage_buffer_object:require
#endif
#define M3(c,c1,a) layout(std430,binding=c)readonly buffer c1{A0 U4[];}a
#define Q2(c,c1,a) layout(std430,binding=c)readonly buffer c1{S U4[];}a
#define N3(c,c1,a) layout(std430,binding=c)readonly buffer c1{g U4[];}a
#define B0(a,C0) a.U4[C0]
#define q2(a,C0) a.U4[C0]
#endif
#ifdef PLS_IMPL_WEBGL
#extension GL_ANGLE_shader_pixel_local_storage:require
#define E1
#define E0(c,a) layout(binding=c,rgba8)uniform lowp pixelLocalANGLE a
#define z0(c,a) layout(binding=c,r32ui)uniform highp upixelLocalANGLE a
#define F1
#define O0(e) pixelLocalLoadANGLE(e)
#define G0(e) pixelLocalLoadANGLE(e).x
#define H0(e,o) pixelLocalStoreANGLE(e,o)
#define R0(e,o) pixelLocalStoreANGLE(e,uvec4(o))
#define d0(e)
#define m1
#define n1
#endif
#ifdef PLS_IMPL_EXT_NATIVE
#extension GL_EXT_shader_pixel_local_storage:enable
#extension GL_ARM_shader_framebuffer_fetch:enable
#extension GL_EXT_shader_framebuffer_fetch:enable
#define E1 __pixel_localEXT J0{
#define E0(c,a) layout(rgba8)lowp vec4 a
#define z0(c,a) layout(r32ui)highp uint a
#define F1 };
#define O0(e) e
#define G0(e) e
#define H0(e,o) e=(o)
#define R0(e,o) e=(o)
#define d0(e)
#define m1
#define n1
#endif
#ifdef PLS_IMPL_FRAMEBUFFER_FETCH
#extension GL_EXT_shader_framebuffer_fetch:require
#define E1
#define E0(c,a) layout(location=c)inout lowp vec4 a
#define z0(c,a) layout(location=c)inout highp uvec4 a
#define F1
#define O0(e) e
#define G0(e) e.x
#define H0(e,o) e=(o)
#define R0(e,o) e.x=(o)
#define d0(e) e=e
#define m1
#define n1
#endif
#ifdef PLS_IMPL_RW_TEXTURE
#ifdef GL_ARB_shader_image_load_store
#extension GL_ARB_shader_image_load_store:require
#endif
#if defined(GL_ARB_fragment_shader_interlock)
#extension GL_ARB_fragment_shader_interlock:require
#define m1 beginInvocationInterlockARB()
#define n1 endInvocationInterlockARB()
#elif defined(GL_INTEL_fragment_shader_ordering)
#extension GL_INTEL_fragment_shader_ordering:require
#define m1 beginFragmentShaderOrderingINTEL()
#define n1
#else
#define m1
#define n1
#endif
#define E1
#ifdef TARGET_VULKAN
#define E0(c,a) layout(set=K4,binding=c,rgba8)uniform lowp coherent image2D a
#define z0(c,a) layout(set=K4,binding=c,r32ui)uniform highp coherent uimage2D a
#else
#define E0(c,a) layout(binding=c,rgba8)uniform lowp coherent image2D a
#define z0(c,a) layout(binding=c,r32ui)uniform highp coherent uimage2D a
#endif
#define F1
#define O0(e) imageLoad(e,J)
#define G0(e) imageLoad(e,J).x
#define H0(e,o) imageStore(e,J,o)
#define R0(e,o) imageStore(e,J,uvec4(o))
#define K3 z0
#define m3 G0
#define n3 R0
#define B4(e,T0) imageAtomicMax(e,J,T0)
#define C4(e,T0) imageAtomicAdd(e,J,T0)
#define d0(e)
#endif
#ifdef PLS_IMPL_SUBPASS_LOAD
#define E1
#define E0(c,a) layout(input_attachment_index=c,binding=c,set=K4)uniform lowp subpassInput V4##a;layout(location=c)out lowp vec4 a
#define z0(c,a) layout(input_attachment_index=c,binding=c,set=K4)uniform lowp usubpassInput V4##a;layout(location=c)out highp uvec4 a
#define F1
#define O0(e) subpassLoad(V4##e)
#define G0(e) subpassLoad(V4##e).x
#define H0(e,o) e=(o)
#define R0(e,o) e.x=(o)
#define d0(e) e=subpassLoad(V4##e)
#define m1
#define n1
#endif
#ifdef PLS_IMPL_NONE
#define E1
#define E0(c,a) layout(location=c)out lowp vec4 a
#define z0(c,a) layout(location=c)out highp uvec4 a
#define F1
#define O0(e) vec4(0)
#define G0(e) 0u
#define H0(e,o) e=(o)
#define R0(e,o) e.x=(o)
#define d0(e)
#define m1
#define n1
#endif
#define D4(e)
#ifdef TARGET_VULKAN
#define gl_VertexID gl_VertexIndex
#endif
#ifdef ENABLE_INSTANCE_INDEX
#ifdef TARGET_VULKAN
#define W4 gl_InstanceIndex
#else
#ifdef ENABLE_SPIRV_CROSS_BASE_INSTANCE
uniform int SPIRV_Cross_BaseInstance;
#define W4 (gl_InstanceID+SPIRV_Cross_BaseInstance)
#else
#define W4 (gl_InstanceID+gl_BaseInstance)
#endif
#endif
#else
#define W4 0
#endif
#define z3
#define h3
#define w1(a,V,A,k,O) void main(){int k=gl_VertexID;int O=W4;
#define k5 w1
#define o4(a,X1,Y1,l2,m2,k) w1(a,X1,Y1,k,O)
#define P(a,G)
#define R(a)
#define M(a,G)
#define f1(p1) gl_Position=p1;}
#define r2(j4,a) layout(location=0)out j4 aa;void main()
#define v2(o) aa=o
#define o0 gl_FragCoord.xy
#define w4
#define L2
#ifdef PLS_IMPL_RW_TEXTURE
#define H2 ,n0 J
#define j1 ,J
#define R1(a) void main(){n0 J=ivec2(floor(o0));
#define c2 }
#else
#define H2
#define j1
#define R1(a) void main()
#define c2
#endif
#define U3(a) R1(a)
#define l3(a) layout(location=0)out i Q0;R1(a);
#define z4(a) layout(location=0)out i Q0;R1(a)
#define V3 c2
#define i0(e0,h0) ((e0)*(h0))
#if GLSL_VERSION<310
p i unpackUnorm4x8(uint u){S e2=S(u&0xffu,(u>>8)&0xffu,(u>>16)&0xffu,u>>24);return g(e2)*(1./255.);}
#endif
#ifndef TARGET_VULKAN
#define x9
#endif
precision highp float;precision highp int;