
//
// WIP splat vertex shader
//

#version 120

uniform mat4 viewMat;
uniform mat4 projMat;
uniform vec4 projParams;  // x = HEIGHT / tan(FOVY / 2), y = Z_NEAR, z = X_FAR
uniform vec4 viewport;  // x, y, WIDTH, HEIGHT

attribute vec3 position;
attribute vec2 uv;
attribute vec4 color;
attribute vec3 cov3_col0; // mat3
attribute vec3 cov3_col1;
attribute vec3 cov3_col2;

varying vec4 frag_color;
varying vec4 cov2Dinv4;
varying vec2 p;

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

void main(void)
{
    vec4 t = viewMat * vec4(position, 1.0f);

    float SS = projParams.x;
    float Z_NEAR = projParams.y;
    float Z_FAR = projParams.z;
    float tzSq = t.z * t.z;
    float jsx = -SS / (2.0f * t.z);
    float jsy = -SS / (2.0f * t.z);
    float jtx = (SS * t.x) / (2.0f * tzSq);
    float jty = (SS * t.y) / (2.0f * tzSq);
    float jtz = -(Z_FAR * Z_NEAR) / tzSq;
    mat3 J = mat3(vec3(jsx, 0.0f, 0.0f),
                  vec3(0.0f, jsy, 0.0f),
                  vec3(jtx, jty, jtz));
    mat3 W = mat3(viewMat);
    mat3 V = mat3(cov3_col0, cov3_col1, cov3_col2);
    mat3 JW = J * W;
    mat3 V_prime = JW * V * transpose(JW);
    mat2 cov2D = mat2(V_prime);
    cov2D[0] += vec2(0.3f, 0.3f); // low pass filter for anti-aliasing
    cov2D[1] += vec2(0.3f, 0.3f);

    vec4 p4 = projMat * viewMat * vec4(position, 1.0f);
    p = vec2(p4.x / p4.w, p4.y / p4.w);

    float WIDTH_2 = viewport.z / 2.0f;
    float HEIGHT_2 = viewport.w / 2.0f;
    p.x = p.x * WIDTH_2 + viewport.x + WIDTH_2;
    p.y = p.y * HEIGHT_2 + viewport.y + HEIGHT_2;

    frag_color = color;
    mat2 cov2Dinv = inverseMat2(cov2D);
    cov2Dinv4 = vec4(cov2Dinv[0], cov2Dinv[1]);

    // transform position into clip coordinates.
    vec4 viewPos = viewMat * vec4(position, 1);
    gl_Position = projMat * viewPos;

    // in viewport coords use cov2 to compute an offset vector for this vertex using the uv parameter.
    float FUDGE = 2.0f;
    vec2 offset = (cov2D * (uv - vec2(0.5f, 0.5f))) * FUDGE;

    // transform that offset back into clip space
    offset.x *= (2.0f / viewport.z) * gl_Position.z;
    offset.y *= (2.0f / viewport.w) * gl_Position.z;
    gl_Position.xy += offset;
}
