#version 460

uniform vec4 viewport;  // x, y, WIDTH, HEIGHT

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 geom_sh0[];  // sh coeff for radiance of splat
in vec4 geom_sh1[];
in vec4 geom_sh2[];
in vec4 geom_cov2[];  // 2D screen space covariance matrix of the gaussian
in vec3 geom_p[];  // the 2D screen space center of the gaussian, (alpha in z)

out vec4 frag_sh0;  // sh coeff for radiance of splat
out vec4 frag_sh1;
out vec4 frag_sh2;
out vec4 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
out vec3 frag_p;  // the 2D screen space center of the gaussian (alpha in z)

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

    float FUDGE = 1.0f;  // I bet there's an optimal value for this...
    vec2 uv, offset;

    //
	// bottom-left vertex
    //
    uv = vec2(0.0, 0.0);

    // in viewport coords use cov2 to compute an offset vector for this vertex using the uv parameter.
    // this is used to embiggen the splat to fit the actual gaussian
    offset = (cov2D * (uv - vec2(0.5f, 0.5f))) * FUDGE;

    // transform that offset back into clip space, and apply it to gl_Position.
    float w = gl_in[0].gl_Position.w;
    offset.x *= (2.0f / WIDTH) * w;
    offset.y *= (2.0f / HEIGHT) * w;

    gl_Position = gl_in[0].gl_Position + vec4(offset.x, offset.y, 0.0, 0.0);
    frag_sh0 = geom_sh0[0];
    frag_sh1 = geom_sh1[0];
    frag_sh2 = geom_sh2[0];
    frag_cov2inv = cov2Dinv4;
    frag_p = geom_p[0];

    EmitVertex();

    //
    // bottom-right vertex
    //
    uv = vec2(1.0, 0.0);

    // in viewport coords use cov2 to compute an offset vector for this vertex using the uv parameter.
    // this is used to embiggen the splat to fit the actual gaussian
    offset = (cov2D * (uv - vec2(0.5f, 0.5f))) * FUDGE;

    // transform that offset back into clip space, and apply it to gl_Position.
    offset.x *= (2.0f / WIDTH) * gl_Position.w;
    offset.y *= (2.0f / HEIGHT) * gl_Position.w;

    gl_Position = gl_in[0].gl_Position + vec4(offset.x, offset.y, 0.0, 0.0);
    frag_sh0 = geom_sh0[0];
    frag_sh1 = geom_sh1[0];
    frag_sh2 = geom_sh2[0];
    frag_cov2inv = cov2Dinv4;
    frag_p = geom_p[0];

    EmitVertex();

    //
    // top-left vertex
    //
    uv = vec2(0.0, 1.0);

    // in viewport coords use cov2 to compute an offset vector for this vertex using the uv parameter.
    // this is used to embiggen the splat to fit the actual gaussian
    offset = (cov2D * (uv - vec2(0.5f, 0.5f))) * FUDGE;

    // transform that offset back into clip space, and apply it to gl_Position.
    offset.x *= (2.0f / WIDTH) * gl_Position.w;
    offset.y *= (2.0f / HEIGHT) * gl_Position.w;

    gl_Position = gl_in[0].gl_Position + vec4(offset.x, offset.y, 0.0, 0.0);
    frag_sh0 = geom_sh0[0];
    frag_sh1 = geom_sh1[0];
    frag_sh2 = geom_sh2[0];
    frag_cov2inv = cov2Dinv4;
    frag_p = geom_p[0];

    EmitVertex();

    // top-right vertex
    uv = vec2(1.0, 1.0);

    // in viewport coords use cov2 to compute an offset vector for this vertex using the uv parameter.
    // this is used to embiggen the splat to fit the actual gaussian
    offset = (cov2D * (uv - vec2(0.5f, 0.5f))) * FUDGE;

    // transform that offset back into clip space, and apply it to gl_Position.
    offset.x *= (2.0f / WIDTH) * gl_Position.w;
    offset.y *= (2.0f / HEIGHT) * gl_Position.w;

    gl_Position = gl_in[0].gl_Position + vec4(offset.x, offset.y, 0.0, 0.0);
    frag_sh0 = geom_sh0[0];
    frag_sh1 = geom_sh1[0];
    frag_sh2 = geom_sh2[0];
    frag_cov2inv = cov2Dinv4;
    frag_p = geom_p[0];

    EmitVertex();

    EndPrimitive();
}
