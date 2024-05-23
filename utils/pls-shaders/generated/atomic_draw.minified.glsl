#ifdef DRAW_PATH
#ifdef VERTEX
V0(V)r0(0,g,TB);r0(1,g,UB);W0
#endif
A1 l0 H(0,m0,L0);OPTIONALLY_FLAT H(1,L,a0);B1
#ifdef VERTEX
w1(OB,V,A,k,O){x0(k,A,TB,g);x0(k,A,UB,g);P(L0,m0);P(a0,L);g B;d Q;if(E6(TB,UB,O,a0,Q,L0 h3)){B=i2(Q);}else{B=g(r.j2,r.j2,r.j2,r.j2);}R(L0);R(a0);f1(B);}
#endif
#endif
#ifdef DRAW_INTERIOR_TRIANGLES
#ifdef VERTEX
V0(V)r0(0,H3,KB);W0
#endif
A1 OPTIONALLY_FLAT H(0,h,C1);OPTIONALLY_FLAT H(1,L,a0);B1
#ifdef VERTEX
w1(OB,V,A,k,O){x0(k,A,KB,D1);P(C1,h);P(a0,L);d Q=F6(KB,a0,C1 h3);g B=i2(Q);R(C1);R(a0);f1(B);}
#endif
#endif
#ifdef DRAW_IMAGE
#ifdef DRAW_IMAGE_RECT
#ifdef VERTEX
V0(V)r0(0,g,LB);W0
#endif
A1 l0 H(0,d,D0);l0 H(1,h,D2);
#ifdef ENABLE_CLIP_RECT
l0 H(2,g,j0);
#endif
B1
#ifdef VERTEX
O1 P1 V1 W1 k5(OB,V,A,k,O){x0(k,A,LB,g);P(D0,d);P(D2,h);
#ifdef ENABLE_CLIP_RECT
P(j0,g);
#endif
bool l5=LB.z==.0||LB.w==.0;D2=l5?.0:1.;d Q=LB.xy;v c0=X0(I.m5);v I3=transpose(inverse(c0));if(!l5){float n5=k2*o5(I3[1])/dot(c0[1],I3[1]);if(n5>=.5){Q.x=.5;D2*=.5/n5;}else{Q.x+=n5*LB.z;}float p5=k2*o5(I3[0])/dot(c0[0],I3[0]);if(p5>=.5){Q.y=.5;D2*=.5/p5;}else{Q.y+=p5*LB.w;}}D0=Q;Q=i0(c0,Q)+I.M0;if(l5){d E2=i0(I3,LB.zw);E2*=o5(E2)/dot(E2,E2);Q+=k2*E2;}
#ifdef ENABLE_CLIP_RECT
j0=n4(X0(I.Y0),I.g1,Q);
#endif
g B=i2(Q);R(D0);R(D2);
#ifdef ENABLE_CLIP_RECT
R(j0);
#endif
f1(B);}
#endif
#else
#ifdef VERTEX
V0(X1)r0(0,d,VB);W0 V0(l2)r0(1,d,WB);W0
#endif
A1 l0 H(0,d,D0);
#ifdef ENABLE_CLIP_RECT
l0 H(1,g,j0);
#endif
B1
#ifdef VERTEX
o4(OB,X1,Y1,l2,m2,k){x0(k,Y1,VB,d);x0(k,m2,WB,d);P(D0,d);
#ifdef ENABLE_CLIP_RECT
P(j0,g);
#endif
v c0=X0(I.m5);d Q=i0(c0,VB)+I.M0;D0=WB;
#ifdef ENABLE_CLIP_RECT
j0=n4(X0(I.Y0),I.g1,Q);
#endif
g B=i2(Q);R(D0);
#ifdef ENABLE_CLIP_RECT
R(j0);
#endif
f1(B);}
#endif
#endif
#endif
#ifdef DRAW_RENDER_TARGET_UPDATE_BOUNDS
#ifdef VERTEX
V0(V)W0
#endif
A1 B1
#ifdef VERTEX
O1 P1 V1 W1 w1(OB,V,A,k,O){n0 h1;h1.x=(k&1)==0?r.p4.x:r.p4.z;h1.y=(k&2)==0?r.p4.y:r.p4.w;g B=i2(d(h1));f1(B);}
#endif
#endif
#ifdef ENABLE_BINDLESS_TEXTURES
#define q5
#endif
#ifdef DRAW_IMAGE
#define q5
#endif
#ifdef FRAGMENT
F2 x1(q4,DC);
#ifdef q5
x1(i3,MB);
#endif
G2 J3(q4,r5)
#ifdef q5
j3(i3,n2)
#endif
E1
#ifdef ENABLE_ADVANCED_BLEND
#ifdef FRAMEBUFFER_PLANE_IDX_OVERRIDE
E0(NC,W);
#else
E0(v5,W);
#endif
#endif
K3(w5,y0);
#ifdef ENABLE_CLIPPING
z0(x5,v0);
#endif
F1 L3 M3(G6,V8,PB);N3(H6,W8,HB);O3 uint I6(float x){return uint(x*J6+P3);}float r4(uint x){return float(x)*K6+(-P3*K6);}i v4(h Z0,A0 E,L i1 w4 H2){h C=abs(Z0);
#ifdef ENABLE_EVEN_ODD
if((E.x&L6)!=0u)C=1.-abs(fract(C*.5)*2.+-1.);
#endif
C=min(C,F0(1));
#ifdef ENABLE_CLIPPING
uint G1=E.x>>16u;if(G1!=0u){uint a1=G0(v0);h I2=G1==(a1>>16u)?unpackHalf2x16(a1).x:.0;C=min(C,I2);}
#endif
i f=N0(0,0,0,0);uint H1=E.x&0xfu;switch(H1){case M6:f=unpackUnorm4x8(E.y);break;case x4:case N6:
#ifdef ENABLE_BINDLESS_TEXTURES
case O6:
#endif
{v c0=X0(B0(HB,i1*4u));g M0=B0(HB,i1*4u+1u);d o2=i0(c0,o0)+M0.xy;
#ifdef ENABLE_BINDLESS_TEXTURES
if(H1==O6){f=Q3(sampler2D(floatBitsToUint(M0.zw)),n2,o2,c0[0],c0[1]);float J2=uintBitsToFloat(E.y);f.w*=J2;}else
#endif
{float t=H1==x4?o2.x:length(o2);t=clamp(t,.0,1.);float x=t*M0.z+M0.w;float y=uintBitsToFloat(E.y);f=N0(R3(DC,r5,d(x,y),.0));}break;}
#ifdef ENABLE_CLIPPING
case y4:R0(v0,E.y|packHalf2x16(Z1(C,0)));break;
#endif
}
#ifdef ENABLE_CLIP_RECT
if((E.x&X8)!=0u){v c0=X0(B0(HB,i1*4u+2u));g M0=B0(HB,i1*4u+3u);d Y8=i0(c0,o0)+M0.xy;m0 P6=Z1(abs(Y8)*M0.zw-M0.zw);h K2=clamp(min(P6.x,P6.y)+.5,.0,1.);C=min(C,K2);}
#endif
f.w*=C;return f;}i Q6(i R6,i Q1){return R6+Q1*(1.-R6.w);}
#ifdef ENABLE_ADVANCED_BLEND
i y5(i S6,i Q1,L a2){if(a2!=T6){
#ifdef ENABLE_HSL_BLEND_MODES
return E3(
#else
return F3(
#endif
S6,S3(Q1),a2);}else{return Q6(p2(S6),Q1);}}i U6(i f,A0 E H2){i Q1=O0(W);L a2=P0((E.x>>4)&0xfu);return y5(f,Q1,a2);}void z5(i f,A0 E H2){if(f.w!=.0){i Z8=U6(f,E j1);H0(W,Z8);}}
#endif
#ifdef ENABLE_ADVANCED_BLEND
#define T3 R1
#define V6 U3
#define k3 c2
#else
#define T3 l3
#define V6 z4
#define k3 V3
#endif
#ifdef DRAW_PATH
T3(IB){M(L0,m0);M(a0,L);d0(v0);d0(y0);
#ifdef ENABLE_ADVANCED_BLEND
d0(W);
#else
Q0=N0(0,0,0,0);
#endif
h C=min(min(L0.x,abs(L0.y)),F0(1));uint A4=I6(C);uint A5=(uint(a0)<<16)|A4;uint k1=B4(y0,A5);L l1=P0(k1>>16);if(l1!=a0){h Z0=r4(k1&0xffffu);A0 E=q2(PB,l1);i f=v4(Z0,E,l1 L2 j1);
#ifdef ENABLE_ADVANCED_BLEND
z5(f,E j1);
#else
Q0=p2(f);
#endif
}else if(L0.y<.0){if(k1<A5){A4+=k1-A5;}A4-=uint(P3);C4(y0,A4);}k3}
#endif
#ifdef DRAW_INTERIOR_TRIANGLES
T3(IB){M(C1,h);M(a0,L);d0(v0);d0(y0);
#ifdef ENABLE_ADVANCED_BLEND
d0(W);
#else
Q0=N0(0,0,0,0);
#endif
h C=C1;uint k1=m3(y0);L l1=P0(k1>>16);h W6=r4(k1&0xffffu);if(l1!=a0){A0 E=q2(PB,l1);i f=v4(W6,E,l1 L2 j1);
#ifdef ENABLE_ADVANCED_BLEND
z5(f,E j1);
#else
Q0=p2(f);
#endif
}else{C+=W6;}n3(y0,(uint(a0)<<16)|I6(C));k3}
#endif
#ifdef DRAW_IMAGE
V6(IB){M(D0,d);
#ifdef DRAW_IMAGE_RECT
M(D2,h);
#endif
#ifdef ENABLE_CLIP_RECT
M(j0,g);
#endif
d0(v0);d0(y0);
#ifdef ENABLE_ADVANCED_BLEND
d0(W);
#endif
i o3=M2(MB,n2,D0);h N2=1.;
#ifdef DRAW_IMAGE_RECT
N2=min(D2,N2);
#endif
#ifdef ENABLE_CLIP_RECT
h K2=B5(N0(j0));N2=clamp(K2,F0(0),N2);
#endif
#ifdef DRAW_IMAGE_MESH
m1;
#endif
uint k1=m3(y0);h Z0=r4(k1&0xffffu);L l1=P0(k1>>16);A0 X6=q2(PB,l1);i C5=v4(Z0,X6,l1 L2 j1);
#ifdef ENABLE_CLIPPING
if(I.G1!=0u){D4(v0);uint a1=G0(v0);uint G1=a1>>16;h I2=G1==I.G1?unpackHalf2x16(a1).x:0;N2=min(N2,I2);}
#endif
o3.w*=N2*I.J2;
#ifdef ENABLE_ADVANCED_BLEND
if(C5.w!=.0||o3.w!=0){i Q1=O0(W);L a9=P0((X6.x>>4)&0xfu);L c9=P0(I.a2);Q1=y5(C5,Q1,a9);o3=y5(o3,Q1,c9);H0(W,o3);}
#else
Q0=Q6(p2(o3),p2(C5));
#endif
n3(y0,uint(P3));
#ifdef DRAW_IMAGE_MESH
n1;
#endif
k3}
#endif
#ifdef INITIALIZE_PLS
T3(IB){
#ifndef ENABLE_ADVANCED_BLEND
Q0=N0(0,0,0,0);
#endif
#ifdef STORE_COLOR_CLEAR
H0(W,unpackUnorm4x8(r.d9));
#endif
#ifdef SWIZZLE_COLOR_BGRA_TO_RGBA
i f=O0(W);H0(W,f.zyxw);
#endif
n3(y0,r.e9);
#ifdef ENABLE_CLIPPING
R0(v0,0u);
#endif
k3}
#endif
#ifdef RESOLVE_PLS
#ifdef COALESCED_PLS_RESOLVE_AND_TRANSFER
l3(IB)
#else
T3(IB)
#endif
{
#ifdef ENABLE_ADVANCED_BLEND
d0(W);
#endif
uint k1=m3(y0);h Z0=r4(k1&0xffffu);L l1=P0(k1>>16);A0 E=q2(PB,l1);i f=v4(Z0,E,l1 L2 j1);
#ifdef COALESCED_PLS_RESOLVE_AND_TRANSFER
Q0=U6(f,E j1);V3
#else
#ifdef ENABLE_ADVANCED_BLEND
z5(f,E j1);
#else
Q0=p2(f);
#endif
k3
#endif
}
#endif
#endif
