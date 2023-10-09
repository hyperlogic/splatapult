//
// WIP splat fragment shader
//

uniform mat4 projMat;
uniform float k;
uniform vec2 p;
uniform mat2 rhoInvMat;

varying vec4 frag_color;

vec2 toNDC(vec2 p)
{
    float w = 1024.0;
    float h = 768.0;
    return vec2((p.x * (2.0 / h)) - (w / h), (p.y * (2.0 / h)) - 1.0);
}

void main()
{
    vec2 x = toNDC(gl_FragCoord.xy);
    vec2 d = x - p;
    float g = k * exp(-0.5f * dot(d, rhoInvMat * d));

    if (d.x * d.x + d.y * d.y < 0.0001f)
    {
        g = 1.0;
    }

    // premultiplied alpha blending
    gl_FragColor.rgb = g * frag_color.a * frag_color.rgb;
    gl_FragColor.a = g * frag_color.a;

    float sigma = 100.0f;
    mat2 V;
    V[0] = vec2(1.0 / (sigma * sigma), 0.0);
    V[1] = vec2(0.0, 1.0 / (sigma * sigma));
    float ggg = exp(-0.5f * dot(d, V * d));

    gl_FragColor.rgba = vec4(g, g, g, 1.0);
}
