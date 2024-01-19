/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

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

    // see https://cookierobotics.com/007/
    float k = 3.0f;
    float a = cov2D[0][0];
    float b = cov2D[0][1];
    float c = cov2D[1][1];
    float apco2 = (a + c) / 2.0f;
    float amco2 = (a - c) / 2.0f;
    float term = sqrt(amco2 * amco2 + b * b);
    float maj = apco2 + term;
    float min = apco2 - term;
    float theta = atan2(maj - a, b);
    float r1 = k * sqrt(maj);
    float r2 = k * sqrt(min);
    vec2 majAxis = vec2(r1 * cos(theta), r1 * sin(theta));
    vec2 minAxis = vec2(r2 * cos(theta + radians(90.0f)), r2 * sin(theta + radians(90.0f)));

    vec2 offsets[4];
    offsets[0] = majAxis + minAxis;
    offsets[1] = -majAxis + minAxis;
    offsets[3] = -majAxis - minAxis;
    offsets[2] = majAxis - minAxis;

    vec2 offset;
    float w = gl_Position.w;
    for (int i = 0; i < 4; i++)
    {
        offset = offsets[i];

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
