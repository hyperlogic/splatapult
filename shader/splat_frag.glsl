//
// 3d gaussian splat fragment shader
//

#version 460

in vec4 frag_color;  // radiance of splat
in vec4 cov2Dinv4;  // inverse of the 2D screen space covariance matrix of the guassian
in vec2 p;  // 2D screen space center of the guassian

out vec4 out_color;

void main()
{
    vec2 d = gl_FragCoord.xy - p;

    // evaluate the gaussian
    mat2 cov2Dinv = mat2(cov2Dinv4.xy, cov2Dinv4.zw);
    float g = exp(-0.5f * dot(d, cov2Dinv * d));

    // premultiplied alpha blending
    out_color.rgb = g * frag_color.a * frag_color.rgb;
    out_color.a = g * frag_color.a;
}
