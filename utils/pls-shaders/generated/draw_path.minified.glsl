#ifdef VERTEX
V0(V)
#ifdef DRAW_INTERIOR_TRIANGLES
r0(0,H3,KB);
#else
r0(0,g,TB);r0(1,g,UB);
#endif
W0
#endif
A1 l0 H(0,g,p0);
#ifndef USING_DEPTH_STENCIL
#ifdef DRAW_INTERIOR_TRIANGLES
OPTIONALLY_FLAT H(1,h,C1);
#else
l0 H(2,m0,L0);
#endif
OPTIONALLY_FLAT H(3,h,a0);
#ifdef ENABLE_CLIPPING
OPTIONALLY_FLAT H(4,h,I0);
#endif
#ifdef ENABLE_CLIP_RECT
l0 H(5,g,j0);
#endif
#endif
#ifdef ENABLE_ADVANCED_BLEND
OPTIONALLY_FLAT H(6,h,x2);
#endif
B1
#ifdef VERTEX
w1(OB,V,A,k,O){
#ifdef DRAW_INTERIOR_TRIANGLES
x0(k,A,KB,D1);
#else
x0(k,A,TB,g);x0(k,A,UB,g);
#endif
P(p0,g);
#ifndef Sa
#ifdef DRAW_INTERIOR_TRIANGLES
P(C1,h);
#else
P(L0,m0);
#endif
P(a0,h);
#ifdef ENABLE_CLIPPING
P(I0,h);
#endif
#ifdef ENABLE_CLIP_RECT
P(j0,g);
#endif
#endif
#ifdef ENABLE_ADVANCED_BLEND
P(x2,h);
#endif
bool z7=false;L i1;d Q;
#ifdef USING_DEPTH_STENCIL
L A7;
#endif
#ifdef DRAW_INTERIOR_TRIANGLES
Q=F6(KB,i1,C1 h3);
#else
z7=!E6(TB,UB,O,i1,Q
#ifndef USING_DEPTH_STENCIL
,L0
#else
,A7
#endif
h3);
#endif
A0 E=q2(PB,i1);
#ifndef USING_DEPTH_STENCIL
a0=F4(i1,r.q3);if((E.x&L6)!=0u)a0=-a0;
#endif
uint H1=E.x&0xfu;
#ifdef ENABLE_CLIPPING
uint w9=(H1==y4?E.y:E.x)>>16;I0=F4(w9,r.q3);if(H1==y4)I0=-I0;
#endif
#ifdef ENABLE_ADVANCED_BLEND
x2=float((E.x>>4)&0xfu);
#endif
d a4=Q;
#ifdef x9
a4.y=float(r.l9)-a4.y;
#endif
#ifdef ENABLE_CLIP_RECT
v Y0=X0(B0(HB,i1*4u+2u));g g1=B0(HB,i1*4u+3u);
#ifndef USING_DEPTH_STENCIL
j0=n4(Y0,g1.xy,a4);
#else
h7(Y0,g1.xy,a4);
#endif
#endif
if(H1==M6){i f=unpackUnorm4x8(E.y);p0=g(f);}
#ifdef ENABLE_CLIPPING
else if(H1==y4){h L4=F4(E.x>>16,r.q3);p0=g(L4,0,0,0);}
#endif
else{v y9=X0(B0(HB,i1*4u));g M5=B0(HB,i1*4u+1u);d o2=i0(y9,a4)+M5.xy;if(H1==x4||H1==N6){p0.w=-uintBitsToFloat(E.y);if(M5.z>.9){p0.z=2.;}else{p0.z=M5.w;}if(H1==x4){p0.y=.0;p0.x=o2.x;}else{p0.z=-p0.z;p0.xy=o2.xy;}}else{float J2=uintBitsToFloat(E.y);p0=g(o2.x,o2.y,J2,-2.);}}g B;if(!z7){B=i2(Q);
#ifdef USING_DEPTH_STENCIL
B.z=J5(A7);
#endif
}else{B=g(r.j2,r.j2,r.j2,r.j2);}R(p0);
#ifndef USING_DEPTH_STENCIL
#ifdef DRAW_INTERIOR_TRIANGLES
R(C1);
#else
R(L0);
#endif
R(a0);
#ifdef ENABLE_CLIPPING
R(I0);
#endif
#ifdef ENABLE_CLIP_RECT
R(j0);
#endif
#endif
#ifdef ENABLE_ADVANCED_BLEND
R(x2);
#endif
f1(B);}
#endif
#ifdef FRAGMENT
F2 x1(q4,DC);x1(i3,MB);
#ifdef USING_DEPTH_STENCIL
#ifdef ENABLE_ADVANCED_BLEND
x1(x7,EC);
#endif
#endif
G2 J3(q4,r5)j3(i3,n2)L3 O3 p i B7(g J1
#ifdef TARGET_VULKAN
,d N5,d O5
#endif
w4){if(J1.w>=.0){return N0(J1);}else if(J1.w>-1.){float t=J1.z>.0?J1.x:length(J1.xy);t=clamp(t,.0,1.);float C7=abs(J1.z);float x=C7>1.?(1.-1./K5)*t+(.5/K5):(1./K5)*t+C7;float z9=-J1.w;return N0(R3(DC,r5,d(x,z9),.0));}else{i f;
#ifdef TARGET_VULKAN
f=Q3(MB,n2,J1.xy,N5,O5);
#else
f=M2(MB,n2,J1.xy);
#endif
f.w*=J1.z;return f;}}
#ifndef USING_DEPTH_STENCIL
E1 E0(v5,W);z0(w5,y0);z0(x5,v0);E0(r7,w2);F1 R1(IB){M(p0,g);
#ifdef DRAW_INTERIOR_TRIANGLES
M(C1,h);
#else
M(L0,m0);
#endif
M(a0,h);
#ifdef ENABLE_CLIPPING
M(I0,h);
#endif
#ifdef ENABLE_CLIP_RECT
M(j0,g);
#endif
#ifdef ENABLE_ADVANCED_BLEND
M(x2,h);
#endif
#ifdef TARGET_VULKAN
d N5=dFdx(p0.xy);d O5=dFdy(p0.xy);
#endif
#ifndef DRAW_INTERIOR_TRIANGLES
m1;
#endif
m0 D7=unpackHalf2x16(G0(y0));h E7=D7.y;h Z0=E7==a0?D7.x:F0(0);
#ifdef DRAW_INTERIOR_TRIANGLES
Z0+=C1;
#else
if(L0.y>=.0)Z0=max(min(L0.x,L0.y),Z0);else Z0+=L0.x;R0(y0,packHalf2x16(Z1(Z0,a0)));
#endif
h C=abs(Z0);
#ifdef ENABLE_EVEN_ODD
if(a0<.0)C=1.-abs(fract(C*.5)*2.+-1.);
#endif
C=min(C,F0(1));
#ifdef ENABLE_CLIPPING
if(I0<.0){h G1=-I0;
#ifdef ENABLE_NESTED_CLIPPING
h L4=p0.x;if(L4!=.0){m0 a1=unpackHalf2x16(G0(v0));h x3=a1.y;h M4;if(x3!=G1){M4=x3==L4?a1.x:.0;
#ifndef DRAW_INTERIOR_TRIANGLES
H0(w2,N0(M4,0,0,0));
#endif
}else{M4=O0(w2).x;
#ifndef DRAW_INTERIOR_TRIANGLES
d0(w2);
#endif
}C=min(C,M4);}
#endif
R0(v0,packHalf2x16(Z1(C,G1)));d0(W);}else
#endif
{
#ifdef ENABLE_CLIPPING
if(I0!=.0){m0 a1=unpackHalf2x16(G0(v0));h x3=a1.y;h I2=x3==I0?a1.x:F0(0);C=min(C,I2);}
#endif
#ifdef ENABLE_CLIP_RECT
h K2=B5(N0(j0));C=clamp(K2,F0(0),C);
#endif
d0(v0);i f=B7(p0
#ifdef TARGET_VULKAN
,N5,O5
#endif
L2);f.w*=C;i y1;if(E7!=a0){y1=O0(W);
#ifndef DRAW_INTERIOR_TRIANGLES
H0(w2,y1);
#endif
}else{y1=O0(w2);
#ifndef DRAW_INTERIOR_TRIANGLES
d0(w2);
#endif
}
#ifdef ENABLE_ADVANCED_BLEND
if(x2!=F0(T6)){
#ifdef ENABLE_HSL_BLEND_MODES
f=E3(
#else
f=F3(
#endif
f,S3(y1),P0(x2));}else
#endif
{
#ifndef PLS_IMPL_NONE
f.xyz*=f.w;f=f+y1*(1.-f.w);
#endif
}H0(W,f);}
#ifndef DRAW_INTERIOR_TRIANGLES
n1;
#endif
c2;}
#else
r2(i,IB){M(p0,g);
#ifdef ENABLE_ADVANCED_BLEND
M(x2,h);
#endif
i f=B7(p0);
#ifdef ENABLE_ADVANCED_BLEND
i y1=I1(EC,n0(floor(o0.xy)));
#ifdef ENABLE_HSL_BLEND_MODES
f=E3(
#else
f=F3(
#endif
f,S3(y1),P0(x2));
#else
f=p2(f);
#endif
v2(f);}
#endif
#endif
