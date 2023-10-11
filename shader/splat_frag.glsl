//
// WIP splat fragment shader
//

#version 120

uniform mat4 viewMat;
uniform mat4 projMat;
uniform vec4 projParams;  // x = HEIGHT / tan(FOVY / 2), y = Z_NEAR, z = X_FAR

varying vec4 frag_color;
varying vec4 cov2Dinv4;
varying vec2 p;

void main()
{
    vec2 d = gl_FragCoord.xy - p;
    mat2 cov2Dinv = mat2(cov2Dinv4.xy, cov2Dinv4.zw);
    float g = exp(-0.5f * dot(d, cov2Dinv * d));

    if (d.x * d.x + d.y * d.y < 0.0001f)
    {
        g = 1.0f;
    }

    // premultiplied alpha blending
    gl_FragColor.rgb = g * frag_color.a * frag_color.rgb;
    gl_FragColor.a = g * frag_color.a;
}
