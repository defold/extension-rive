#define FILL_TYPE_SOLID  0
#define FILL_TYPE_LINEAR 1
#define FILL_TYPE_RADIAL 2
#define MAX_COLORS       16
#define MAX_STOPS        4

uniform vec4  colors[MAX_COLORS];
uniform vec4  stops[MAX_STOPS];
uniform vec4  gradientLimits;
uniform vec4  properties; // x: filltype, y: stopcount
varying vec2 vx_position;
varying vec2 gradient_uv;

void main()
{
    vec2 gradientStart = gradientLimits.xy;
    vec2 gradientStop = gradientLimits.zw;
    int iStopCount = int(properties.y);
    int iFillType  = int(properties.x);

    vec4 fragColor = vec4(vec3(0.0), 1.0);

    if (iFillType == FILL_TYPE_SOLID)
    {
        fragColor = vec4(colors[0].rgb * colors[0].a, colors[0].a);
    }
    else
    {
        float f = iFillType == 1 ? gradient_uv.x : length(gradient_uv);
        vec4 color =
        mix(colors[0], colors[1], smoothstep(stops[0][0], stops[0][1], f));
        for (int i = 1; i < 15; ++i)
        {
            if (i >= iStopCount - 1)
            {
                break;
            }

            color = mix(color,
                colors[i + 1],
                smoothstep(stops[i/4][i%4], stops[(i+1)/4][(i+1)%4], f));
        }
        fragColor = color;    
    }

    gl_FragColor = fragColor;
}
