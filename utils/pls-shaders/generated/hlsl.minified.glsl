#pragma warning(disable:3550)
#ifndef _ARE_TOKEN_NAMES_PRESERVED
#define h half
#define m0 half2
#define N half3
#define i half4
#define a6 short
#define L ushort
#define d float2
#define D1 float3
#define g float4
#define Z7 bool3
#define A0 uint2
#define S uint4
#define n0 int2
#define G5 int4
#define L ushort
#define v float2x2
#define e6 half3x4
#endif
typedef D1 H3;
#define F0 float
#define Z1 d
#define k0 D1
#define N0 g
typedef min16int a6;typedef min16uint L;
#define P0 min16uint
#define j5 half3x4
#define p inline
#define y2(h4) out h4
#define V0(a) struct a{
#define r0(c,G,a) G a:a
#define W0 };
#define x0(c6,A,a,G) G a=A.a
#define a8(c) register(b##c)
#define X3(c,a) cbuffer a:a8(c){struct{
#define G4(a) }a;}
#define A1 struct f0{
#define l0 noperspective
#define OPTIONALLY_FLAT nointerpolation
#define d6 nointerpolation
#define H(c,G,a) G a:TEXCOORD##c
#define B1 g p1:SV_Position;};
#define P(a,G) G a
#define R(a) K.a=a
#define M(a,G) G a=K.a
#ifdef VERTEX
#define O1
#define P1
#endif
#ifdef FRAGMENT
#define F2
#define G2
#endif
#define P2(c,a) uniform Texture2D<S>a:register(t##c)
#define i4(c,a) uniform Texture2D<g>a:register(t##c)
#define x1(c,a) uniform Texture2D<unorm g>a:register(t##c)
#define c8(d2,a) SamplerState a:register(s##d2);
#define J3 c8
#define j3 c8
#define I1(a,F) a[F]
#define M2(a,q0,F) a.Sample(q0,F)
#define R3(a,q0,F,V2) a.SampleLevel(q0,F,V2)
#define Q3(a,q0,F,W2,X2) a.SampleGrad(q0,F,W2,X2)
#define m1
#define n1
#ifdef ENABLE_RASTERIZER_ORDERED_VIEWS
#define B3 RasterizerOrderedTexture2D
#else
#define B3 RWTexture2D
#endif
#define E1
#ifdef ENABLE_TYPED_UAV_LOAD_STORE
#define E0(c,a) uniform B3<unorm i>a:register(u##c)
#else
#define E0(c,a) uniform B3<uint>a:register(u##c)
#endif
#define z0(c,a) uniform B3<uint>a:register(u##c)
#define K3 z0
#define m3 G0
#define n3 R0
#define F1
#ifdef ENABLE_TYPED_UAV_LOAD_STORE
#define O0(e) e[J]
#else
#define O0(e) unpackUnorm4x8(e[J])
#endif
#define G0(e) e[J]
#ifdef ENABLE_TYPED_UAV_LOAD_STORE
#define H0(e,o) e[J]=(o)
#else
#define H0(e,o) e[J]=packUnorm4x8(o)
#endif
#define R0(e,o) e[J]=(o)
p uint f6(B3<uint>g6,n0 J,uint x){uint L1;InterlockedMax(g6[J],x,L1);return L1;}
#define B4(e,T0) f6(e,J,T0)
p uint h6(B3<uint>g6,n0 J,uint x){uint L1;InterlockedAdd(g6[J],x,L1);return L1;}
#define C4(e,T0) h6(e,J,T0)
#define d0(e)
#define D4(v0)
#define z3
#define h3
#define w1(a,V,A,k,O) cbuffer Va:a8(v9){uint fa;uint a##Wa;uint a##Xa;uint a##Ya;}f0 a(V A,uint k:SV_VertexID,uint ga:SV_InstanceID){uint O=ga+fa;f0 K;
#define k5(a,V,A,k,O) f0 a(V A,uint k:SV_VertexID){f0 K;g p1;
#define o4(a,X1,Y1,l2,m2,k) f0 a(X1 Y1,l2 m2,uint k:SV_VertexID){f0 K;g p1;
#define f1(i6) K.p1=i6;}return K;
#define r2(j4,a) j4 a(f0 K):SV_Target{
#define v2(o) return o;}
#define w4 ,d o0
#define L2 ,o0
#define H2 ,n0 J
#define j1 ,J
#define R1(a) [earlydepthstencil]void a(f0 K){d o0=K.p1.xy;n0 J=n0(floor(o0));
#define U3(a) R1(a)
#define c2 }
#define l3(a) [earlydepthstencil]i a(f0 K):SV_Target{d o0=K.p1.xy;n0 J=n0(floor(o0));i Q0;
#define z4(a) l3(a)
#define V3 }return Q0;
#define uintBitsToFloat asfloat
#define floatBitsToInt asint
#define floatBitsToUint asuint
#define fract frac
#define mix lerp
#define inversesqrt rsqrt
#define notEqual(e0,h0) ((e0)!=(h0))
#define lessThanEqual(e0,h0) ((e0)<=(h0))
#define greaterThanEqual(e0,h0) ((e0)>=(h0))
#define i0(e0,h0) mul(h0,e0)
#define V1
#define W1
#define L3
#define O3
#define M3(c,c1,a) StructuredBuffer<A0>a:register(t##c)
#define Q2(c,c1,a) StructuredBuffer<S>a:register(t##c)
#define N3(c,c1,a) StructuredBuffer<g>a:register(t##c)
#define B0(a,C0) a[C0]
#define q2(a,C0) a[C0]
p m0 unpackHalf2x16(uint u){uint y=(u>>16);uint x=u&0xffffu;return Z1(f16tof32(x),f16tof32(y));}p uint packHalf2x16(d S0){uint x=f32tof16(S0.x);uint y=f32tof16(S0.y);return(y<<16)|x;}p i unpackUnorm4x8(uint u){S e2=S(u&0xffu,(u>>8)&0xffu,(u>>16)&0xffu,u>>24);return g(e2)*(1./255.);}p uint packUnorm4x8(i f){S e2=(S(f*255.)&0xff)<<S(0,8,16,24);e2.xy|=e2.zw;e2.x|=e2.y;return e2.x;}p float atan(float y,float x){return atan2(y,x);}p v inverse(v d1){v ha=v(d1[1][1],-d1[0][1],-d1[1][0],d1[0][0]);return ha*(1./determinant(d1));}