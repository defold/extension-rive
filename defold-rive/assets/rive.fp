#define FILL_TYPE_NONE   0
#define FILL_TYPE_SOLID  1
#define FILL_TYPE_LINEAR 2
#define FILL_TYPE_RADIAL 3
#define MAX_STOPS        16

uniform vec4  stops[MAX_STOPS];
uniform vec4  colors[MAX_STOPS];
uniform vec4  gradientLimits;
uniform vec4  properties; // x: filltype, y: stopcount
varying vec2 vx_position;

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
    else if (iFillType == FILL_TYPE_LINEAR)
    {
        vec2 toEnd          = gradientStop - gradientStart;
        float lengthSquared = toEnd.x * toEnd.x + toEnd.y * toEnd.y;
        float f             = dot(vx_position - gradientStart, toEnd) / lengthSquared;
        vec4 color          = mix(colors[0], colors[1], smoothstep(stops[0].x, stops[1].x, f));

        for (int i=1; i < MAX_STOPS; ++i)
        {
            if(i >= iStopCount-1)
            {
                break;
            }
            color = mix(color, colors[i+1], smoothstep( stops[i].x, stops[i+1].x, f ));
        }

        fragColor = vec4(color.xyz * color.w, color.w);
    }
    else if (iFillType == FILL_TYPE_RADIAL)
    {
        float f    = distance(gradientStart, vx_position)/distance(gradientStart, gradientStop);
        vec4 color = mix(colors[0], colors[1], smoothstep(stops[0].x, stops[1].x, f));

        for (int i=1; i < MAX_STOPS; ++i)
        {
            if(i >= iStopCount-1)
            {
                break;
            }
            color = mix(color, colors[i+1], smoothstep( stops[i].x, stops[i+1].x, f ));
        }

        fragColor = vec4(color.xyz * color.w, color.w);
    }

    gl_FragColor = fragColor;
}
