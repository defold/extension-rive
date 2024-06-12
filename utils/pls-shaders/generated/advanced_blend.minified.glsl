#ifdef ENABLE_KHR_BLEND
layout(
#ifdef ENABLE_HSL_BLEND_MODES
blend_support_all_equations
#else
blend_support_multiply,blend_support_screen,blend_support_overlay,blend_support_darken,blend_support_lighten,blend_support_colordodge,blend_support_colorburn,blend_support_hardlight,blend_support_softlight,blend_support_difference,blend_support_exclusion
#endif
)out;
#endif
#ifdef ENABLE_ADVANCED_BLEND
#ifdef ENABLE_HSL_BLEND_MODES
h e5(N g0){return min(min(g0.x,g0.y),g0.z);}h x6(N g0){return max(max(g0.x,g0.y),g0.z);}h f5(N g0){return dot(g0,k0(.30,.59,.11));}h y6(N g0){return x6(g0)-e5(g0);}N A8(N f){h h2=f5(f);h z6=e5(f);h A6=x6(f);if(z6<.0)f=h2+((f-h2)*h2)/(h2-z6);if(A6>1.)f=h2+((f-h2)*(1.-h2))/(A6-h2);return f;}N g5(N g3,N h5){h B8=f5(g3);h C8=f5(h5);h i5=C8-B8;N f=g3+k0(i5,i5,i5);return A8(f);}N B6(N g3,N D8,N h5){h E8=e5(g3);h C6=y6(g3);h F8=y6(D8);N f;if(C6>.0){f=(g3-E8)*F8/C6;}else{f=k0(0,0,0);}return g5(f,h5);}
#endif
#ifdef ENABLE_HSL_BLEND_MODES
i E3(i q,i m,L D6)
#else
i F3(i q,i m,L D6)
#endif
{N T=k0(0,0,0);switch(D6){case G8:T=q.xyz*m.xyz;break;case H8:T=q.xyz+m.xyz-q.xyz*m.xyz;break;case I8:{for(int j=0;j<3;++j){if(m[j]<=.5)T[j]=2.*q[j]*m[j];else T[j]=1.-2.*(1.-q[j])*(1.-m[j]);}break;}case J8:T=min(q.xyz,m.xyz);break;case K8:T=max(q.xyz,m.xyz);break;case L8:T=mix(min(m.xyz/(1.-q.xyz),k0(1,1,1)),k0(0,0,0),lessThanEqual(m.xyz,k0(0,0,0)));break;case M8:T=mix(1.-min((1.-m.xyz)/q.xyz,1.),k0(1,1,1),greaterThanEqual(m.xyz,k0(1,1,1)));break;case N8:{for(int j=0;j<3;++j){if(q[j]<=.5)T[j]=2.*q[j]*m[j];else T[j]=1.-2.*(1.-q[j])*(1.-m[j]);}break;}case O8:{for(int j=0;j<3;++j){if(q[j]<=0.5)T[j]=m[j]-(1.-2.*q[j])*m[j]*(1.-m[j]);else if(m[j]<=.25)T[j]=m[j]+(2.*q[j]-1.)*m[j]*((16.*m[j]-12.)*m[j]+3.);else T[j]=m[j]+(2.*q[j]-1.)*(sqrt(m[j])-m[j]);}break;}case P8:T=abs(m.xyz-q.xyz);break;case Q8:T=q.xyz+m.xyz-2.*q.xyz*m.xyz;break;
#ifdef ENABLE_HSL_BLEND_MODES
case R8:q.xyz=clamp(q.xyz,k0(0,0,0),k0(1,1,1));T=B6(q.xyz,m.xyz,m.xyz);break;case S8:q.xyz=clamp(q.xyz,k0(0,0,0),k0(1,1,1));T=B6(m.xyz,q.xyz,m.xyz);break;case T8:q.xyz=clamp(q.xyz,k0(0,0,0),k0(1,1,1));T=g5(q.xyz,m.xyz);break;case U8:q.xyz=clamp(q.xyz,k0(0,0,0),k0(1,1,1));T=g5(m.xyz,q.xyz);break;
#endif
}N G3=k0(q.w*m.w,q.w*(1.-m.w),(1.-q.w)*m.w);return i0(j5(T,1,q.xyz,1,m.xyz,1),G3);}
#endif
