
//
// 3d gaussian splat vertex shader
//

#version 460

uniform mat4 viewMat;  // used to project position into view coordinates.
uniform mat4 projMat;  // used to project view coordinates into clip coordinates.
uniform vec4 projParams;  // x = HEIGHT / tan(FOVY / 2), y = Z_NEAR, z = Z_FAR
uniform vec4 viewport;  // x, y, WIDTH, HEIGHT
uniform vec3 eye;

in vec4 position;  // center of the gaussian in object coordinates, (with alpha crammed in to w)
in vec4 sh0;  // spherical harmonics coeff for radiance of the splat
in vec4 sh1;
in vec4 sh2;
in vec3 cov3_col0;  // 3x3 covariance matrix of the splat in object coordinates.
in vec3 cov3_col1;
in vec3 cov3_col2;

out vec4 geom_color;  // radiance of splat
out vec4 geom_cov2;  // 2D screen space covariance matrix of the gaussian
out vec2 geom_p;  // the 2D screen space center of the gaussian, (z is alpha)


// from Efficient Spherical Harmonic Evaluation, Peter-Pike Sloan (2013)
/*
void SHNewEval3(const float fX, const float fY, const float fZ, float* __restrict pSH) {
    float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
    float fZ2 = fZ * fZ;
    pSH[0] = 0.2820947917738781f;
    pSH[2] = 0.4886025119029199f * fZ;
    pSH[6] = 0.9461746957575601f * fZ2 + -0.3153915652525201f;
    fC0 = fX;
    fS0 = fY;
    fTmpA = -0.48860251190292f;
    pSH[3] = fTmpA * fC0;
    pSH[1] = fTmpA * fS0;
    fTmpB = -1.092548430592079f * fZ;
    pSH[7] = fTmpB * fC0;
    pSH[5] = fTmpB * fS0;
    fC1 = fX*fC0 - fY*fS0;
    fS1 = fX*fS0 + fY*fC0;
    fTmpC = 0.5462742152960395f;
    pSH[8] = fTmpC * fC1;
    pSH[4] = fTmpC * fS1;
}
*/
vec3 ComputeRadianceFromSH(const vec3 v)
{
    float pSH[9];
    float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
    float vz2 = v.z * v.z;
    pSH[0] = 0.2820947917738781f;
    pSH[2] = 0.4886025119029199f * v.z;
    pSH[6] = 0.9461746957575601f * vz2 + -0.3153915652525201f;
    fC0 = v.x;
    fS0 = v.y;
    fTmpA = -0.48860251190292f;
    pSH[3] = fTmpA * fC0;
    pSH[1] = fTmpA * fS0;
    fTmpB = -1.092548430592079f * v.z;
    pSH[7] = fTmpB * fC0;
    pSH[5] = fTmpB * fS0;
    fC1 = v.x * fC0 - v.y * fS0;
    fS1 = v.x * fS0 + v.y * fC0;
    fTmpC = 0.5462742152960395f;
    pSH[8] = fTmpC * fC1;
    pSH[4] = fTmpC * fS1;

    return vec3(0.5f + pSH[0] * sh0.x + pSH[1] * sh0.w + pSH[2] * sh1.z + pSH[3] * sh2.y,
                0.5f + pSH[0] * sh0.y + pSH[1] * sh1.x + pSH[2] * sh1.w + pSH[3] * sh2.z,
                0.5f + pSH[0] * sh0.z + pSH[1] * sh1.y + pSH[2] * sh2.x + pSH[3] * sh2.w);
}

void main(void)
{
    // t is in view coordinates
    float alpha = position.w;
    vec4 t = viewMat * vec4(position.xyz, 1.0f);

    //float X0 = viewport.x;
    float X0 = viewport.x * (0.00001f * projParams.y);  // one weird hack to prevent projParams from being compiled away
    float Y0 = viewport.y;
    float WIDTH = viewport.z;
    float HEIGHT = viewport.w;
    float Z_NEAR = projParams.y;
    float Z_FAR = projParams.z;

    // J is the jacobian of the projection and viewport transformations.
    // this is an affine approximation of the real projection.
    // because gaussians are closed under affine transforms.
    float SX = projMat[0][0];
    float SY = projMat[1][1];
    float WZ =  projMat[3][2];
    float tzSq = t.z * t.z;
    float jsx = -(SX * WIDTH) / (2.0f * t.z);
    float jsy = -(SY * HEIGHT) / (2.0f * t.z);
    float jtx = (SX * t.x * WIDTH) / (2.0f * tzSq);
    float jty = (SY * t.y * HEIGHT) / (2.0f * tzSq);
    float jtz = ((Z_FAR - Z_NEAR) * WZ) / (2.0f * tzSq);
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

    // geom_p is the gaussian center transformed into screen space
    vec4 p4 = projMat * t;
    geom_p = vec2(p4.x / p4.w, p4.y / p4.w);
    geom_p.x = 0.5f * (WIDTH + (geom_p.x * WIDTH) + (2.0f * X0));
    geom_p.y = 0.5f * (HEIGHT + (geom_p.y * HEIGHT) + (2.0f * Y0));

    // compute radiance from sh
    /*
    float SH_K0 = 0.28209479177387814f; // 1 / (2 sqrt(pi))
    float SH_K1 = 0.4886025119029199f;  // sqrt(3) / (2 sqrt(pi))
    vec3 v = normalize(eye - position.xyz);
    vec3 SH_C1 = vec3(-SH_K1 * v.y, SH_K1 * v.z, -SH_K1 * v.x);
    vec3 radiance = vec3(0.5f + SH_K0 * sh0.x + SH_C1.x * sh0.w + SH_C1.x * sh1.z + SH_C1.x * sh2.y,
                         0.5f + SH_K0 * sh0.y + SH_C1.y * sh1.x + SH_C1.y * sh1.w + SH_C1.y * sh2.z,
                         0.5f + SH_K0 * sh0.z + SH_C1.z * sh1.y + SH_C1.z * sh2.x + SH_C1.z * sh2.w);
    geom_color = vec4(radiance, alpha);
    */
    vec3 v = normalize(eye - position.xyz);
    geom_color = vec4(ComputeRadianceFromSH(v), alpha);

    // gl_Position is in clip coordinates.
    gl_Position = p4;

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
