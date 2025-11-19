#version 140

#define FILL_TYPE_SOLID 0
#define FILL_TYPE_LINEAR 1
#define FILL_TYPE_RADIAL 2
#define FILL_TYPE_TEXTURED 3
#define MAX_COLORS 16
#define MAX_STOPS 4

in highp vec2 var_texcoord0;
in highp vec2 var_gradient_uv;

uniform fs_uniforms {
    lowp vec4 colors[MAX_COLORS];
    mediump vec4 stops[MAX_STOPS];
    highp vec4 gradientLimits;
    highp vec4 properties; // x: filltype, y: stopcount
    lowp vec4 id_color;
};

uniform mediump sampler2D texture_sampler;

out lowp vec4 out_color;

void main() {
    vec2 gradientStart = gradientLimits.xy;
    vec2 gradientStop = gradientLimits.zw;
    int iStopCount = int(properties.y);
    int iFillType = int(properties.x);
    float alpha = 0.0;

    if (iFillType == FILL_TYPE_SOLID) {
        alpha = colors[0].a;
    } else if (iFillType == FILL_TYPE_TEXTURED) {
        alpha = texture(texture_sampler, var_texcoord0).a * colors[0].a;
    } else {
        int stop_cur = 1;
        int stop_next = stop_cur + 1;
        float f = iFillType == FILL_TYPE_LINEAR ? var_gradient_uv.x : length(var_gradient_uv);
        float t = smoothstep(stops[0][0], stops[0][1], f);
        alpha = mix(colors[0].a, colors[1].a, t);

        for (int i = 1; i < 15; ++i) {
            if (i >= iStopCount - 1) {
                break;
            }

            t = smoothstep(stops[i / 4][stop_cur], stops[(i + 1) / 4][stop_next], f);
            alpha = mix(alpha, colors[i + 1].a, t);
            stop_cur = stop_next;
            stop_next = stop_next + 1;

            if (stop_next >= 4) {
                stop_next = stop_next - 4;
            }
        }
    }

    if (alpha > 0.05) {
        out_color = id_color;
    } else {
        out_color = vec4(0.0);
    }
}
