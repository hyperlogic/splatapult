#version 460

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

    // k is the desired value we want to bound.
    // where k = dot(d, conv2Dinv * d);
    // or k = x (a*x + c*y) + y (b*x + d*y)
    // We solve for the x that will give us one y solution.
    // and solve for the y that will give us one x solution.
    // we use these x & y's to build an axis aligned bounding box
    // that is guarrenteed to enclose k = 8.
    float k = 8.0f;
    float a = cov2Dinv4.x;
    float b = cov2Dinv4.z;
    float c = cov2Dinv4.y;
    float d = cov2Dinv4.w;
    float denom = sqrt(-(b * b) - 2 * b * c - (c * c) + 4 * a * d);
    float sqrtk = sqrt(k);
    float xx = 2.0f * sqrt(d) * sqrtk / denom;
    float yy = 2.0f * sqrt(a) * sqrtk / denom;
    vec2 o[4];
    o[0] = vec2(xx, (-(b * xx) - (c * xx)) / (2.0f * d));
    o[1] = vec2(((-b * yy) - (c * yy)) / (2.0f * a), yy);
    o[2] = -o[0];
    o[3] = -o[1];

    vec2 offsets[4];
    vec2 omax = max(max(o[0], o[1]), max(o[2], o[3]));
    vec2 omin = min(min(o[0], o[1]), min(o[2], o[3]));
    offsets[0] = vec2(omax.x, omax.y);
    offsets[1] = vec2(omin.x, omax.y);
    offsets[3] = vec2(omin.x, omin.y);
    offsets[2] = vec2(omax.x, omin.y);

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
