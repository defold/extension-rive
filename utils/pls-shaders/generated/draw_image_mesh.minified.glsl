#ifdef VERTEX
V0(X1)r0(0,d,VB);W0 V0(l2)r0(1,d,WB);W0
#endif
A1 l0 H(0,d,D0);
#ifdef ENABLE_CLIPPING
OPTIONALLY_FLAT H(1,h,I0);
#endif
#ifdef ENABLE_CLIP_RECT
l0 H(2,g,j0);
#endif
B1
#ifdef VERTEX
O1 P1 o4(OB,X1,Y1,l2,m2,k){x0(k,Y1,VB,d);x0(k,m2,WB,d);P(D0,d);
#ifdef ENABLE_CLIPPING
P(I0,h);
#endif
#ifdef ENABLE_CLIP_RECT
P(j0,g);
#endif
d Q=i0(X0(I.m5),VB)+I.M0;D0=WB;
#ifdef ENABLE_CLIPPING
I0=F4(I.G1,r.q3);
#endif
#ifdef ENABLE_CLIP_RECT
#ifndef USING_DEPTH_STENCIL
j0=n4(X0(I.Y0),I.g1,Q);
#else
h7(X0(I.Y0),I.g1,Q);
#endif
#endif
g B=i2(Q);
#ifdef USING_DEPTH_STENCIL
B.z=J5(I.Y3);
#endif
R(D0);
#ifdef ENABLE_CLIPPING
R(I0);
#endif
#ifdef ENABLE_CLIP_RECT
R(j0);
#endif
f1(B);}
#endif
#ifdef FRAGMENT
F2 x1(i3,MB);
#ifdef USING_DEPTH_STENCIL
#ifdef ENABLE_ADVANCED_BLEND
x1(x7,EC);
#endif
#endif
G2 j3(i3,n2)L3 O3
#ifndef USING_DEPTH_STENCIL
E1 E0(v5,W);z0(w5,y0);z0(x5,v0);E0(r7,w2);F1 U3(IB){M(D0,d);
#ifdef ENABLE_CLIPPING
M(I0,h);
#endif
#ifdef ENABLE_CLIP_RECT
M(j0,g);
#endif
i f=M2(MB,n2,D0);h C=1.;
#ifdef ENABLE_CLIP_RECT
h K2=B5(N0(j0));C=clamp(K2,F0(0),C);
#endif
m1;
#ifdef ENABLE_CLIPPING
if(I0!=.0){m0 a1=unpackHalf2x16(G0(v0));h x3=a1.y;h I2=x3==I0?a1.x:F0(0);C=min(C,I2);}
#endif
f.w*=I.J2*C;i y1=O0(W);
#ifdef ENABLE_ADVANCED_BLEND
if(I.a2!=0u){
#ifdef ENABLE_HSL_BLEND_MODES
f=E3(
#else
f=F3(
#endif
f,S3(y1),P0(I.a2));}else
#endif
{f.xyz*=f.w;f=f+y1*(1.-f.w);}H0(W,f);d0(y0);d0(v0);n1;c2;}
#else
r2(i,IB){M(D0,d);i f=M2(MB,n2,D0);f.w*=I.J2;
#ifdef ENABLE_ADVANCED_BLEND
i y1=I1(EC,n0(floor(o0.xy)));
#ifdef ENABLE_HSL_BLEND_MODES
f=E3(
#else
f=F3(
#endif
f,S3(y1),P0(I.a2));
#else
f=p2(f);
#endif
v2(f);}
#endif
#endif
