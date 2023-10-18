
//
// 3d gaussian splat vertex shader
//

#version 460

uniform mat4 viewMat;  // used to project position into view coordinates.
uniform mat4 projMat;  // used to project view coordinates into clip coordinates.
uniform vec4 projParams;  // x = HEIGHT / tan(FOVY / 2), y = Z_NEAR, z = Z_FAR
uniform vec4 viewport;  // x, y, WIDTH, HEIGHT

in vec3 position;  // center of the gaussian in object coordinates.
in vec4 color;  // radiance of the splat
in vec3 cov3_col0;  // 3x3 covariance matrix of the splat in object coordinates.
in vec3 cov3_col1;
in vec3 cov3_col2;

out vec4 geom_color;  // radiance of splat
out vec4 geom_cov2;  // 2D screen space covariance matrix of the gaussian
out vec2 geom_p;  // the 2D screen space center of the gaussian

void main(void)
{
    // t is in view coordinates
    vec4 t = viewMat * vec4(position, 1.0f);

    // J is the jacobian of the projection and viewport transformations.
    // this is an affine approximation of the real projection.
    // because gaussians are closed under affine transforms.
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

    // combine the affine transforms of W (viewMat) and J (approx of viewportMat * projMat)
    // using the fact that the new transformed covariance matrix V_Prime = JW * V * (JW)^T
    mat3 W = mat3(viewMat);
    mat3 V = mat3(cov3_col0, cov3_col1, cov3_col2);
    mat3 JW = J * W;
    mat3 V_prime = JW * V * transpose(JW);

    // now we can 'project' the 3D covariance matrix onto the xy plane by just dropping the last column and row.
    mat2 cov2D = mat2(V_prime);

    // use the fact that the convolution of a gaussian with another gaussian is the sum
    // of their covariance matrices to apply a low-pass filter to anti-alias the splats
    cov2D[0] += vec2(1.0f, 1.0f);
    cov2D[1] += vec2(1.0f, 1.0f);
    geom_cov2 = vec4(cov2D[0], cov2D[1]); // cram it into a vec4

    float X0 = viewport.x;
    float Y0 = viewport.y;
    float WIDTH = viewport.z;
    float HEIGHT = viewport.w;

    // geom_p is the gaussian center transformed into screen space
    vec4 p4 = projMat * viewMat * vec4(position, 1.0f);
    geom_p = vec2(p4.x / p4.w, p4.y / p4.w);
    geom_p.x = 0.5f * (WIDTH + (geom_p.x * WIDTH) + (2.0f * X0));
    geom_p.y = 0.5f * (HEIGHT + (geom_p.y * HEIGHT) + (2.0f * Y0));

    geom_color = color;

    // transform position into clip coordinates.
    vec4 viewPos = viewMat * vec4(position, 1);
    gl_Position = projMat * viewPos;

    // discard splats that end up outside of a guard band
    vec3 ndcP = p4.xyz / p4.w;
    if (ndcP.z < 0.25f ||
        ndcP.x > 2.0f || ndcP.x < -2.0f ||
        ndcP.y > 2.0f || ndcP.y < -2.0f)
    {
        // discard this point
        gl_Position = vec4(0.0f, 0.0f, -2.0f, 1.0f); // Place vertex behind the camera
    }
}
