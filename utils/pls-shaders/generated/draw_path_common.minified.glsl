#ifdef VERTEX
O1 P2(r9,AC);P1 V1 Q2(v7,A9,RB);M3(G6,V8,PB);N3(H6,W8,HB);Q2(w7,B9,FC);W1
#ifdef DRAW_PATH
p n0 N4(int F7){return n0(F7&((1<<i7)-1),F7>>i7);}p float G7(v c0,d C9){d S0=i0(c0,C9);return(abs(S0.x)+abs(S0.y))*(1./dot(S0,S0));}p bool E6(g c4,g P5,int O,y2(L)R2,y2(d)D9
#ifndef USING_DEPTH_STENCIL
,y2(m0)S2
#else
,y2(L)E9
#endif
z3){int O4=int(c4.x);float S1=c4.y;float P4=c4.z;int H7=floatBitsToInt(c4.w)>>2;int Q5=floatBitsToInt(c4.w)&3;int R5=min(O4,H7-1);int Q4=O*H7+R5;S A3=I1(AC,N4(Q4));uint D=A3.w;S S5=B0(FC,Y6(D));d I7=uintBitsToFloat(S5.xy);R2=P0(S5.z&0xffffu);uint F9=S5.w;v c0=X0(uintBitsToFloat(B0(RB,R2*2u)));S d4=B0(RB,R2*2u+1u);d M0=uintBitsToFloat(d4.xy);float K1=uintBitsToFloat(d4.z);
#ifdef USING_DEPTH_STENCIL
E9=P0(d4.w);
#endif
uint J7=D&I4;if(J7!=0u){O4=int(P5.x);S1=P5.y;P4=P5.z;}if(O4!=R5){Q4+=O4-R5;S K7=I1(AC,N4(Q4));if((K7.w&0xffffu)!=(D&0xffffu)){bool G9=K1==.0||I7.x!=.0;if(G9){A3=I1(AC,N4(int(F9)));}}else{A3=K7;}D=A3.w|J7;}float o1=uintBitsToFloat(A3.z);d T2=d(sin(o1),-cos(o1));d L7=uintBitsToFloat(A3.xy);d T5;if(K1!=.0){S1*=sign(determinant(c0));if((D&J4)!=0u)S1=min(S1,.0);if((D&n7)!=0u)S1=max(S1,.0);float z2=G7(c0,T2)*k2;h M7=1.;if(z2>K1){M7=F0(K1)/F0(z2);K1=z2;}d U2=i0(T2,K1+z2);
#ifndef USING_DEPTH_STENCIL
float x=S1*(K1+z2);S2=Z1((1./(z2*2.))*(d(x,-x)+K1)+.5);
#endif
uint U5=D&Z3;if(U5!=0u){int e4=2;if((D&L5)==0u)e4=-e4;if((D&I4)!=0u)e4=-e4;n0 H9=N4(Q4+e4);S I9=I1(AC,H9);float J9=uintBitsToFloat(I9.z);float f4=abs(J9-o1);if(f4>E4)f4=2.*E4-f4;bool R4=(D&L5)!=0u;bool K9=(D&J4)!=0u;float N7=f4*(R4==K9?-.5:.5)+o1;d S4=d(sin(N7),-cos(N7));float V5=G7(c0,S4);float g4=cos(f4*.5);float W5;if((U5==p9)||(U5==q9&&g4>=.25)){float L9=(D&H4)!=0u?1.:.25;W5=K1*(1./max(g4,L9));}else{W5=K1*g4+V5*.5;}float X5=W5+V5*k2;if((D&m7)!=0u){float O7=K1+z2;float M9=z2*.125;if(O7<=X5*g4+M9){float N9=O7*(1./g4);U2=S4*N9;}else{d Y5=S4*X5;d O9=d(dot(U2,U2),dot(Y5,Y5));U2=i0(O9,inverse(v(U2,Y5)));}}d P9=abs(S1)*U2;float P7=(X5-dot(P9,S4))/(V5*(k2*2.));
#ifndef USING_DEPTH_STENCIL
if((D&J4)!=0u)S2.y=F0(P7);else S2.x=F0(P7);
#endif
}
#ifndef USING_DEPTH_STENCIL
S2*=M7;S2.y=max(S2.y,F0(1e-4));
#endif
T5=i0(c0,S1*U2);if(Q5!=o7)return false;}else{if(Q5==q7)L7=I7;T5=sign(i0(S1*T2,inverse(c0)))*k2;if((D&I4)!=0u)P4=-P4;
#ifndef USING_DEPTH_STENCIL
S2=Z1(P4,-1);
#endif
if((D&l7)!=0u&&Q5!=p7)return false;}D9=i0(c0,L7)+T5+M0;return true;}
#endif
#ifdef DRAW_INTERIOR_TRIANGLES
p d F6(D1 Z5,y2(L)R2,y2(h)Q9 z3){R2=P0(floatBitsToUint(Z5.z)&0xffffu);v c0=X0(uintBitsToFloat(B0(RB,R2*2u)));S d4=B0(RB,R2*2u+1u);d M0=uintBitsToFloat(d4.xy);Q9=float(floatBitsToInt(Z5.z)>>16)*sign(determinant(c0));return i0(c0,Z5.xy)+M0;}
#endif
#endif
