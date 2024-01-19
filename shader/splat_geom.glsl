/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

/*%%HEADER%%*/

uniform vec4 viewport;  // x, y, WIDTH, HEIGHT

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 geom_color[];  // radiance of splat
in vec4 geom_cov2[];  // 2D screen space covariance matrix of the gaussian
in vec2 geom_p[];  // the 2D screen space center of the gaussian

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

    mat2 cov2D = mat2(geom_cov2[0].xy, geom_cov2[0].zw);

    // we pass the inverse of the 2d covariance matrix to the pixel shader, to avoid doing a matrix inverse per pixel.
    mat2 cov2Dinv = inverseMat2(cov2D);
    vec4 cov2Dinv4 = vec4(cov2Dinv[0], cov2Dinv[1]); // cram it into a vec4

    // discard splats that end up outside of a guard band
    vec4 p4 = gl_in[0].gl_Position;
    vec3 ndcP = p4.xyz / p4.w;
    if (ndcP.z < 0.25f ||
        ndcP.x > 2.0f || ndcP.x < -2.0f ||
        ndcP.y > 2.0f || ndcP.y < -2.0f)
    {
        // discard this point
        return;
    }

    // compute 2d extents for the splat, using covariance matrix ellipse
    // see https://cookierobotics.com/007/
    float k = 3.5f;
    float a = cov2D[0][0];
    float b = cov2D[0][1];
    float c = cov2D[1][1];
    float apco2 = (a + c) / 2.0f;
    float amco2 = (a - c) / 2.0f;
    float term = sqrt(amco2 * amco2 + b * b);
    float maj = apco2 + term;
    float min = apco2 - term;

    float theta;
    if (b == 0.0f)
    {
        theta = (a >= c) ? 0.0f : radians(90.0f);
    }
    else
    {
        theta = atan(maj - a, b);
    }

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
    float w = gl_in[0].gl_Position.w;
    for (int i = 0; i < 4; i++)
    {
        // transform offset back into clip space, and apply it to gl_Position.
        offset = offsets[i];
        offset.x *= (2.0f / WIDTH) * w;
        offset.y *= (2.0f / HEIGHT) * w;

        gl_Position = gl_in[0].gl_Position + vec4(offset.x, offset.y, 0.0, 0.0);
        frag_color = geom_color[0];
        frag_cov2inv = cov2Dinv4;
        frag_p = geom_p[0];

        EmitVertex();
    }

    EndPrimitive();
}
