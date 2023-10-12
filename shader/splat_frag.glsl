//
// WIP splat fragment shader
//

#version 120

varying vec4 frag_color;  // radiance of splat
varying vec4 cov2Dinv4;  // inverse of the 2D screen space covariance matrix of the guassian
varying vec2 p;  // 2D screen space center of the guassian

void main()
{
    vec2 d = gl_FragCoord.xy - p;

    // evaluate the gaussian
    mat2 cov2Dinv = mat2(cov2Dinv4.xy, cov2Dinv4.zw);
    float g = exp(-0.5f * dot(d, cov2Dinv * d));

    // premultiplied alpha blending
    gl_FragColor.rgb = g * frag_color.a * frag_color.rgb;
    gl_FragColor.a = g * frag_color.a;
}
