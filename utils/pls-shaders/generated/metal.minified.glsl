#ifndef _ARE_TOKEN_NAMES_PRESERVED
#define h half
#define m0 half2
#define N half3
#define i half4
#define a6 short
#define L ushort
#define d float2
#define D1 float3
#define H3 packed_float3
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
#define N0 i
#define k0 N
#define Z1 m0
#define F0 h
#define P0 L
#define p inline
#define y2(h4) thread h4&
#define notEqual(e0,h0) ((e0)!=(h0))
#define lessThanEqual(e0,h0) ((e0)<=(h0))
#define greaterThanEqual(e0,h0) ((e0)>=(h0))
#define i0(e0,h0) ((e0)*(h0))
#define atan atan2
#define inversesqrt rsqrt
#define X3(c,a) struct a{
#define G4(a) };
#define V0(a) struct a{
#define r0(c,G,a) G a
#define W0 };
#define x0(c6,A,a,G) G a=A[c6].a
#define A1 struct f0{
#define H(c,G,a) G a
#define d6 [[flat]]
#define l0 [[center_no_perspective]]
#define OPTIONALLY_FLAT
#define B1 g p1[[position]][[invariant]];};
#define P(a,G) thread G&a=K.a
#define R(a)
#define M(a,G) G a=K.a
#define V1 struct ia{
#define W1 };
#define L3 struct Y2{
#define O3 };
#define M3(c,c1,a) constant A0*a[[buffer(c)]]
#define Q2(c,c1,a) constant S*a[[buffer(c)]]
#define N3(c,c1,a) constant g*a[[buffer(c)]]
#define B0(a,C0) M1.a[C0]
#define q2(a,C0) M1.a[C0]
#define O1 struct ja{
#define P1 };
#define F2 struct Z2{
#define G2 };
#define P2(c,a) [[texture(c)]]texture2d<uint>a
#define i4(c,a) [[texture(c)]]texture2d<float>a
#define x1(c,a) [[texture(c)]]texture2d<h>a
#define J3(d2,a) constexpr sampler a(filter::linear,mip_filter::none);
#define j3(d2,a) constexpr sampler a(filter::linear,mip_filter::linear);
#define I1(a3,F) q1.a3.read(A0(F))
#define M2(a3,q0,F) q1.a3.sample(q0,F)
#define R3(a3,q0,F,V2) q1.a3.sample(q0,F,level(V2))
#define Q3(a3,q0,F,W2,X2) q1.a3.sample(q0,F,gradient2d(W2,X2))
#define z3 ,ja q1,ia M1
#define h3 ,q1,M1
#define w1(a,V,A,k,O) __attribute__((visibility("default")))f0 vertex a(uint k[[vertex_id]],uint O[[instance_id]],constant XB&r[[buffer(r3)]],constant V*A[[buffer(0)]]z3){f0 K;
#define k5(a,V,A,k,O) __attribute__((visibility("default")))f0 vertex a(uint k[[vertex_id]],uint O[[instance_id]],constant XB&r[[buffer(r3)]],constant YB&I[[buffer(v3)]],constant V*A[[buffer(0)]]z3){f0 K;
#define o4(a,X1,Y1,l2,m2,k) __attribute__((visibility("default")))f0 vertex a(uint k[[vertex_id]],constant XB&r[[buffer(r3)]],constant YB&I[[buffer(v3)]],constant X1*Y1[[buffer(0)]],constant l2*m2[[buffer(1)]]){f0 K;
#define f1(i6) K.p1=i6;}return K;
#define r2(j4,a) j4 __attribute__((visibility("default")))fragment a(f0 K[[stage_in]]){
#define v2(o) return o;}
#define w4 ,d o0,Z2 q1,Y2 M1
#define L2 ,o0,q1,M1
#ifdef PLS_IMPL_DEVICE_BUFFER
#define E1 struct J0{
#ifdef PLS_IMPL_DEVICE_BUFFER_RASTER_ORDERED
#define E0(c,a) device uint*a[[buffer(c+w3),raster_order_group(0)]]
#define z0(c,a) device uint*a[[buffer(c+w3),raster_order_group(0)]]
#define K3(c,a) device atomic_uint*a[[buffer(c+w3),raster_order_group(0)]]
#else
#define E0(c,a) device uint*a[[buffer(c+w3)]]
#define z0(c,a) device uint*a[[buffer(c+w3)]]
#define K3(c,a) device atomic_uint*a[[buffer(c+w3)]]
#endif
#define F1 };
#define H2 ,J0 X,uint N1
#define j1 ,X,N1
#define O0(e) unpackUnorm4x8(X.e[N1])
#define G0(e) X.e[N1]
#define m3(e) atomic_load_explicit(&X.e[N1],memory_order::memory_order_relaxed)
#define H0(e,o) X.e[N1]=packUnorm4x8(o)
#define R0(e,o) X.e[N1]=(o)
#define n3(e,o) atomic_store_explicit(&X.e[N1],o,memory_order::memory_order_relaxed)
#define d0(e)
#define D4(e)
#define B4(e,T0) atomic_fetch_max_explicit(&X.e[N1],T0,memory_order::memory_order_relaxed)
#define C4(e,T0) atomic_fetch_add_explicit(&X.e[N1],T0,memory_order::memory_order_relaxed)
#define m1
#define n1
#define k4(a) __attribute__((visibility("default")))fragment a(J0 X,constant XB&r[[buffer(r3)]],f0 K[[stage_in]],Z2 q1,Y2 M1){d o0=K.p1.xy;A0 J=A0(metal::floor(o0));uint N1=J.y*r.g7+J.x;
#define d8(a) __attribute__((visibility("default")))fragment a(J0 X,constant XB&r[[buffer(r3)]],constant YB&I[[buffer(v3)]],f0 K[[stage_in]],Z2 q1,Y2 M1){d o0=K.p1.xy;A0 J=A0(metal::floor(o0));uint N1=J.y*r.g7+J.x;
#define R1(a) void k4(a)
#define U3(a) void d8(a)
#define c2 }
#define l3(a) i k4(a){i Q0;
#define z4(a) i d8(a){i Q0;
#define V3 }return Q0;c2
#else
#define E1 struct J0{
#define E0(c,a) [[color(c)]]i a
#define z0(c,a) [[color(c)]]uint a
#define K3 z0
#define F1 };
#define H2 ,thread J0&f2,thread J0&X
#define j1 ,f2,X
#define O0(e) f2.e
#define G0(e) f2.e
#define m3(e) G0
#define H0(e,o) X.e=(o)
#define R0(e,o) X.e=(o)
#define n3(e) R0
#define d0(e) X.e=f2.e
#define D4(e) f2.e=X.e
p uint f6(thread uint&m,uint x){uint L1=m;m=metal::max(L1,x);return L1;}
#define B4(e,T0) f6(X.e,T0)
p uint h6(thread uint&m,uint x){uint L1=m;m=L1+x;return L1;}
#define C4(e,T0) h6(X.e,T0)
#define m1
#define n1
#define k4(a,...) J0 __attribute__((visibility("default")))fragment a(__VA_ARGS__){d o0[[maybe_unused]]=K.p1.xy;J0 X;
#define R1(a,...) k4(a,J0 f2,f0 K[[stage_in]],Z2 q1,Y2 M1)
#define U3(a) k4(a,J0 f2,f0 K[[stage_in]],Z2 q1,Y2 M1,constant YB&I[[buffer(v3)]])
#define c2 }return X;
#define e8(a,...) struct ka{i la[[f(0)]];J0 X;};ka __attribute__((visibility("default")))fragment a(__VA_ARGS__){d o0[[maybe_unused]]=K.p1.xy;i Q0;J0 X;
#define l3(a) e8(a,J0 f2,f0 K[[stage_in]],Z2 q1,Y2 M1)
#define z4(a) e8(a,J0 f2,f0 K[[stage_in]],Z2 q1,Y2 M1,__VA_ARGS__ constant YB&I[[buffer(v3)]])
#define V3 }return{.la=Q0,.X=X};
#endif
using namespace metal;template<int z1>p vec<uint,z1>floatBitsToUint(vec<float,z1>x){return as_type<vec<uint,z1>>(x);}template<int z1>p vec<int,z1>floatBitsToInt(vec<float,z1>x){return as_type<vec<int,z1>>(x);}p uint floatBitsToUint(float x){return as_type<uint>(x);}p int floatBitsToInt(float x){return as_type<int>(x);}template<int z1>p vec<float,z1>uintBitsToFloat(vec<uint,z1>x){return as_type<vec<float,z1>>(x);}p float uintBitsToFloat(uint x){return as_type<float>(x);}p m0 unpackHalf2x16(uint x){return as_type<m0>(x);}p uint packHalf2x16(m0 x){return as_type<uint>(x);}p i unpackUnorm4x8(uint x){return unpack_unorm4x8_to_half(x);}p uint packUnorm4x8(i x){return pack_half_to_unorm4x8(x);}p v inverse(v d1){v j6=v(d1[1][1],-d1[0][1],-d1[1][0],d1[0][0]);float ma=(j6[0][0]*d1[0][0])+(j6[0][1]*d1[1][0]);return j6*(1/ma);}p N mix(N l,N b,Z7 g0){N f8;for(int j=0;j<3;++j)f8[j]=g0[j]?b[j]:l[j];return f8;}p e6 j5(N l,h b,N g0,h na,N k6,h T){return e6(l.x,l.y,l.z,b,g0.x,g0.y,g0.z,na,k6.x,k6.y,k6.z,T);}