uniform lowp vec4 color;

void main()
{
    gl_FragColor = vec4(color.rgb * color.a, color.a);
}
