#include <glm/glm.hpp>

#define in extern
#define out extern
#define main debug_splat_frag
using namespace glm;
extern vec2 gl_FragCoord;

//
// 3d gaussian splat fragment shader
//

//#version 460

in vec4 frag_color;  // radiance of splat
in vec4 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
in vec2 frag_p;  // 2D screen space center of the guassian

out vec4 out_color;

void main()
{
    vec2 d = vec2(gl_FragCoord) - frag_p;

    // evaluate the gaussian
    //mat2 cov2Dinv = mat2(frag_cov2inv.xy, frag_cov2inv.zw);
    mat2 cov2Dinv = mat2(vec2(frag_cov2inv.x, frag_cov2inv.y), vec2(frag_cov2inv.z, frag_cov2inv.w));
    float g = exp(-0.5f * dot(d, cov2Dinv * d));

    // premultiplied alpha blending
    vec3 color = g * frag_color.a * vec3(frag_color);
    float alpha = g * frag_color.a;
    out_color = vec4(color, alpha);
}
