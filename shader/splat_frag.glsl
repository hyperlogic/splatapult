//
// 3d gaussian splat fragment shader
//

#version 460

in vec4 frag_color;  // radiance of splat
in vec4 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
in vec2 frag_p;  // 2D screen space center of the guassian

out vec4 out_color;

void main()
{
    vec2 d = gl_FragCoord.xy - frag_p;

    // TODO: Use texture for gaussian evaluation
    // evaluate the gaussian
    mat2 cov2Dinv = mat2(frag_cov2inv.xy, frag_cov2inv.zw);
    float g = exp(-0.5f * dot(d, cov2Dinv * d));

    // The SIBR reference renderer uses sRGB throughout.
    // the splat colors are sRGB, the gaussian is sRGB and alpha-blending occurs in sRGB.
    // However, our shader output needs to be in linear space. (in order for openxr color conversion to work)
    // So, we convert the splat color to linear (in vertex shader),
    // but the guassian and alpha-blending are now in linear space.
    // This leads to results that don't quite match the SIBR reference.

    // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // glEnable(GL_FRAMEBUFFER_SRGB);
    out_color.rgb = frag_color.a * g * frag_color.rgb;
    out_color.a = frag_color.a * g;

    /*
    // sRGB blending
    // assuming frag_color is in sRGB
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glDisable(GL_FRAMEBUFFER_SRGB);
    out_color.rgb = frag_color.rgb;
    out_color.a = frag_color.a * g;
    */

    /*
    // pre-multiplied sRGB blending
    // assuming frag_color is in sRGB
    // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // glDisable(GL_FRAMEBUFFER_SRGB);
    out_color.rgb = frag_color.a * g * frag_color.rgb;
    out_color.a = frag_color.a * g;
    */
}
