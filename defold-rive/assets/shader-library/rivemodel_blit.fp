varying vec2 var_texcoord0;

uniform sampler2D texture_sampler;

void main()
{
    vec4 color = texture2D(texture_sampler, var_texcoord0);
    gl_FragColor = color;
}
