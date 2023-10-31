//
// 3d gaussian splat fragment shader
//

#version 460

in vec4 frag_sh0;  // sh coeff for radiance of splat
in vec4 frag_sh1;
in vec4 frag_sh2;
in vec4 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
in vec3 frag_p;  // 2D screen space center of the guassian (alpha in z)

out vec4 out_color;

void main()
{
    vec2 d = gl_FragCoord.xy - frag_p.xy;

    // evaluate the gaussian
    mat2 cov2Dinv = mat2(frag_cov2inv.xy, frag_cov2inv.zw);
    float g = exp(-0.5f * dot(d, cov2Dinv * d));

    // compute radiance from sh
    float SH_C0 = 0.28209479177387814f; // 1 / (2 sqrt(pi))

    // AJT: TODO compute proper values for polynomials
    vec3 SH_C1 = vec3(0.0001f, 0.0001f, 0.0001f);

    vec3 radiance = vec3(0.5f + SH_C0 * frag_sh0.x + SH_C1.x * frag_sh0.w + SH_C1.x * frag_sh1.z + SH_C1.x * frag_sh2.y,
                         0.5f + SH_C0 * frag_sh0.y + SH_C1.y * frag_sh1.x + SH_C1.y * frag_sh1.w + SH_C1.y * frag_sh2.z,
                         0.5f + SH_C0 * frag_sh0.z + SH_C1.z * frag_sh1.y + SH_C1.z * frag_sh2.x + SH_C1.z * frag_sh2.w);
    float alpha = frag_p.z;

    // premultiplied alpha blending
    //float alpha = frag_p.z;
    out_color.rgb = g * alpha * radiance;
    out_color.a = g * alpha;
}
