#include <glm/glm.hpp>

#define uniform extern
#define in extern
#define out extern
#define main debug_splat_geom
using namespace glm;
extern vec4 gl_Position;
extern vec4 gl_Position2;
extern void EmitVertex();
extern void EndPrimitive();

uniform vec4 viewport;  // x, y, WIDTH, HEIGHT
uniform vec4 projParams; // x = HEIGHT / tan(FOVY / 2), y = Z_NEAR, z = Z_FAR
//layout(points) in;
//layout(triangle_strip, max_vertices = 4) out;

/*
in vec4 geom_color[];  // radiance of splat
in vec4 geom_cov2[];  // 2D screen space covariance matrix of the gaussian
in vec2 geom_p[];  // the 2D screen space center of the gaussian
*/
in vec4 geom_color;  // radiance of splat
in vec4 geom_cov2;  // 2D screen space covariance matrix of the gaussian
in vec2 geom_p;  // the 2D screen space center of the gaussian

out vec4 frag_color;  // radiance of splat
out vec4 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
out vec2 frag_p;  // the 2D screen space center of the gaussian

// used to invert the 2D screen space covariance matrix
mat2 inverseMat2(mat2 m)
{
    float det = m[0][0] * m[1][1] - m[0][1] * m[1][0];
    mat2 inv;
    inv[0][0] =  m[1][1] / det;
    inv[0][1] = -m[0][1] / det;
    inv[1][0] = -m[1][0] / det;
    inv[1][1] =  m[0][0] / det;

    return inv;
}

void main()
{
    float WIDTH = viewport.z;
    float HEIGHT = viewport.w;

    //mat2 cov2D = mat2(geom_cov2[0].xy, geom_cov2[0].zw);
    mat2 cov2D = mat2(vec2(geom_cov2.x, geom_cov2.y), vec2(geom_cov2.z, geom_cov2.w));

    // we pass the inverse of the 2d covariance matrix to the pixel shader, to avoid doing a matrix inverse per pixel.
    mat2 cov2Dinv = inverseMat2(cov2D);
    vec4 cov2Dinv4 = vec4(cov2Dinv[0], cov2Dinv[1]); // cram it into a vec4

    // discard splats that end up outside of a guard band
    vec4 p4 = gl_Position;
    vec3 ndcP = vec3(p4) / p4.w;
    if (ndcP.z < 0.25f ||
        ndcP.x > 2.0f || ndcP.x < -2.0f ||
        ndcP.y > 2.0f || ndcP.y < -2.0f)
    {
        // discard this point
        return;
    }

    vec2 offset;
    float OFFSET = 10.5f;
    float w = gl_Position.w;

    vec2 scale = vec2(length(cov2D[0]), length(cov2D[1]));
    cov2D[0] = cov2D[0] / scale.x;
    cov2D[1] = cov2D[1] / scale.y;
    scale = sqrt(scale);
    cov2D[0] = cov2D[0] * scale;
    cov2D[1] = cov2D[1] * scale;

    //cov2D = transpose(inverseMat2(cov2D));

    /*
    // counter out square factor in cov matrix
    cov2D[0][0] = sign(cov2D[0][0]) * sqrt(abs(cov2D[0][0]));
    cov2D[0][1] = sign(cov2D[0][1]) * sqrt(abs(cov2D[0][1]));
    cov2D[1][0] = sign(cov2D[1][0]) * sqrt(abs(cov2D[1][0]));
    cov2D[1][1] = sign(cov2D[1][1]) * sqrt(abs(cov2D[1][1]));
    */

    vec2 offsets[4];
    offsets[0] = vec2(-OFFSET, -OFFSET);
    offsets[1] = vec2(OFFSET, -OFFSET);
    offsets[2] = vec2(-OFFSET, OFFSET);
    offsets[3] = vec2(OFFSET, OFFSET);
    for (int i = 0; i < 4; i++)
    {
        offset = cov2D * offsets[i];

        // transform that offset back into clip space, and apply it to gl_Position.
        offset.x *= (2.0f / WIDTH) * w;
        offset.y *= (2.0f / HEIGHT) * w;

        gl_Position2 = gl_Position + vec4(offset.x, offset.y, 0.0, 0.0);
        frag_color = geom_color;
        frag_cov2inv = cov2Dinv4;
        frag_p = geom_p;

        EmitVertex();
    }

    EndPrimitive();
}
