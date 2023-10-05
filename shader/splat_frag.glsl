//
// WIP splat fragment shader
//

uniform float k;
uniform vec2 p;
uniform mat2 rhoInvMat;

varying vec4 frag_color;

void main()
{
    vec2 d = gl_FragCoord.xy - p;
    float g = k * exp(-0.5f * (d * rhoInvMat * d));

    // premultiplied alpha blending
    gl_FragColor.rgb = g * frag_color.a * frag_color.rgb;
    gl_FragColor.a = g * frag_color.a;

    //float d_len = sqrt(d.x * d.x + d.y * d.y);
    //const float WIDTH = 1024.0;
    //const float HEIGHT = 768.0;
    gl_FragColor.rgba = vec4(p.x / 1024.0f, p.y / 768.0, 1.0, 1.0);
}
