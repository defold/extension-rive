#define FILL_TYPE_SOLID    0
#define FILL_TYPE_LINEAR   1
#define FILL_TYPE_RADIAL   2
#define FILL_TYPE_TEXTURED 3

#define MAX_COLORS       16
#define MAX_STOPS        4

uniform vec4  colors[MAX_COLORS];
uniform vec4  stops[MAX_STOPS];

varying vec2 var_texcoord0;
varying vec2 gradient_uv;

uniform highp vec4  gradientLimits;
uniform highp vec4  properties; // x: filltype, y: stopcount

uniform sampler2D texture_sampler;

void main()
{
    vec2 gradientStart = gradientLimits.xy;
    vec2 gradientStop = gradientLimits.zw;
    int iStopCount = int(properties.y);
    int iFillType  = int(properties.x);

    vec4 fragColor = vec4(vec3(0.0), 1.0);

    if (iFillType == FILL_TYPE_SOLID)
    {
        fragColor = colors[0];
    }
    else if (iFillType == FILL_TYPE_TEXTURED)
    {
        fragColor      = texture2D(texture_sampler, var_texcoord0);
        //fragColor.rgb *= colors[0].a;
        fragColor.a  *= colors[0].a;
    }
    else
    {
        float f = iFillType == 1 ? gradient_uv.x : length(gradient_uv);
        vec4 color =
        mix(colors[0], colors[1], smoothstep(stops[0][0], stops[0][1], f));
        int stop_cur = 1;
        int stop_next = stop_cur+1;
        for (int i = 1; i < 15; ++i)
        {
            if (i >= iStopCount - 1)
            {
                break;
            }

            color = mix(color,
                colors[i + 1],
                smoothstep(stops[i/4][stop_cur], stops[(i+1)/4][stop_next], f));

            stop_cur = stop_next;
            stop_next = stop_next + 1;
            if (stop_next >= 4)
            {
                stop_next = stop_next - 4;
            }
        }
        fragColor = color;
    }

    gl_FragColor = fragColor;
}
