#define E4 float(3.141592653589793238)
#ifndef USING_DEPTH_STENCIL
#define k2 float(.5)
#else
#define k2 float(.0)
#endif
p uint Y6(uint D){return(D&h9)-1u;}p d p3(d l,d b,float t){return(b-l)*t+l;}p h F4(uint Z6,uint q3){return Z6==0u?.0:unpackHalf2x16((Z6+i9)*q3).x;}p float a7(d S0){float c7=.0;if(abs(S0.x)>abs(S0.y)){S0=d(S0.y,-S0.x);c7=E4/2.;}return atan(S0.y,S0.x)+c7;}p i p2(i f){return N0(f.xyz*f.w,f.w);}p i S3(i f){if(f.w!=.0)f.xyz*=1.0/f.w;return f;}p v X0(g T){return v(T.xy,T.zw);}p h B5(i d7){m0 e7=min(d7.xy,d7.zw);h j9=min(e7.x,e7.y);return j9;}p float o5(d x){return abs(x.x)+abs(x.y);}
#ifdef VERTEX
X3(r3,XB)float E5;float F5;float k9;float f7;uint g7;uint l9;uint d9;uint e9;G5 p4;uint q3;float j2;G4(r)
#define i2(F) g((F).x*r.k9-1.,(F).y*-r.f7+sign(r.f7),.0,1.)
#ifndef USING_DEPTH_STENCIL
p g n4(v Y0,d g1,d H5){d I5=abs(Y0[0])+abs(Y0[1]);if(I5.x!=.0&&I5.y!=.0){d Z=1./I5;d O2=i0(Y0,H5)+g1;const float m9=.5;return g(O2,-O2)*Z.xyxy+Z.xyxy+m9;}else{return g1.xyxy;}}
#else
p float J5(uint Y3){return 1.-float(Y3)*(2./32768.);}
#ifdef ENABLE_CLIP_RECT
p void h7(v Y0,d g1,d H5){if(Y0!=v(0)){d O2=i0(Y0,H5)+g1.xy;gl_ClipDistance[0]=O2.x+1.;gl_ClipDistance[1]=O2.y+1.;gl_ClipDistance[2]=1.-O2.x;gl_ClipDistance[3]=1.-O2.y;}else{gl_ClipDistance[0]=gl_ClipDistance[1]=gl_ClipDistance[2]=gl_ClipDistance[3]=g1.x-.5;}}
#endif
#endif
#endif
#ifdef DRAW_IMAGE
X3(v3,YB)g m5;d M0;float J2;float Ha;g Y0;d g1;uint G1;uint a2;uint Y3;G4(I)
#endif
