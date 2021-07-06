uniform lowp vec4 color;
uniform lowp vec4 tint;

void main()
{
    gl_FragColor = vec4(color.rgb * color.a * tint.rgb, color.a * tint.a);
}
