/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// 3d gaussian splat vertex shader
//

/*%%HEADER%%*/

/*%%DEFINES%%*/

uniform mat4 viewMat;  // used to project position into view coordinates.
uniform mat4 projMat;  // used to project view coordinates into clip coordinates.
uniform vec4 projParams;  // x = HEIGHT / tan(FOVY / 2), y = Z_NEAR, z = Z_FAR
uniform vec4 viewport;  // x, y, WIDTH, HEIGHT
uniform vec3 eye;

in vec4 position;  // center of the gaussian in object coordinates, (with alpha crammed in to w)

// spherical harmonics coeff for radiance of the splat
in vec4 r_sh0;  // sh coeff for red channel (up to third-order)
#ifdef FULL_SH
in vec4 r_sh1;
in vec4 r_sh2;
in vec4 r_sh3;
#endif
in vec4 g_sh0;  // sh coeff for green channel
#ifdef FULL_SH
in vec4 g_sh1;
in vec4 g_sh2;
in vec4 g_sh3;
#endif
in vec4 b_sh0;  // sh coeff for blue channel
#ifdef FULL_SH
in vec4 b_sh1;
in vec4 b_sh2;
in vec4 b_sh3;
#endif

// 3x3 covariance matrix of the splat in object coordinates.
in vec3 cov3_col0;
in vec3 cov3_col1;
in vec3 cov3_col2;

out vec4 geom_color;  // radiance of splat
out vec4 geom_cov2;  // 2D screen space covariance matrix of the gaussian
out vec2 geom_p;  // the 2D screen space center of the gaussian, (z is alpha)

vec3 ComputeRadianceFromSH(const vec3 v)
{
#ifdef FULL_SH
    float b[16];
#else
    float b[4];
#endif

    float vx2 = v.x * v.x;
    float vy2 = v.y * v.y;
    float vz2 = v.z * v.z;

    // zeroth order
    // (/ 1.0 (* 2.0 (sqrt pi)))
    b[0] = 0.28209479177387814f;

    // first order
    // (/ (sqrt 3.0) (* 2 (sqrt pi)))
    float k1 = 0.4886025119029199f;
    b[1] = -k1 * v.y;
    b[2] = k1 * v.z;
    b[3] = -k1 * v.x;

#ifdef FULL_SH
    // second order
    // (/ (sqrt 15.0) (* 2 (sqrt pi)))
    float k2 = 1.0925484305920792f;
    // (/ (sqrt 5.0) (* 4 (sqrt  pi)))
    float k3 = 0.31539156525252005f;
    // (/ (sqrt 15.0) (* 4 (sqrt pi)))
    float k4 = 0.5462742152960396f;
    b[4] = k2 * v.y * v.x;
    b[5] = -k2 * v.y * v.z;
    b[6] = k3 * (3.0f * vz2 - 1.0f);
    b[7] = -k2 * v.x * v.z;
    b[8] = k4 * (vx2 - vy2);

    // third order
    // (/ (* (sqrt 2) (sqrt 35)) (* 8 (sqrt pi)))
    float k5 = 0.5900435899266435f;
    // (/ (sqrt 105) (* 2 (sqrt pi)))
    float k6 = 2.8906114426405543f;
    // (/ (* (sqrt 2) (sqrt 21)) (* 8 (sqrt pi)))
    float k7 = 0.4570457994644658f;
    // (/ (sqrt 7) (* 4 (sqrt pi)))
    float k8 = 0.37317633259011546f;
    // (/ (sqrt 105) (* 4 (sqrt pi)))
    float k9 = 1.4453057213202771f;
    b[9] = -k5 * v.y * (3.0f * vx2 - vy2);
    b[10] = k6 * v.y * v.x * v.z;
    b[11] = -k7 * v.y * (5.0f * vz2 - 1.0f);
    b[12] = k8 * v.z * (5.0f * vz2 - 3.0f);
    b[13] = -k7 * v.x * (5.0f * vz2 - 1.0f);
    b[14] = k9 * v.z * (vx2 - vy2);
    b[15] = -k5 * v.x * (vx2 - 3.0f * vy2);

    float re = (b[0] * r_sh0.x + b[1] * r_sh0.y + b[2] * r_sh0.z + b[3] * r_sh0.w +
                b[4] * r_sh1.x + b[5] * r_sh1.y + b[6] * r_sh1.z + b[7] * r_sh1.w +
                b[8] * r_sh2.x + b[9] * r_sh2.y + b[10]* r_sh2.z + b[11]* r_sh2.w +
                b[12]* r_sh3.x + b[13]* r_sh3.y + b[14]* r_sh3.z + b[15]* r_sh3.w);

    float gr = (b[0] * g_sh0.x + b[1] * g_sh0.y + b[2] * g_sh0.z + b[3] * g_sh0.w +
                b[4] * g_sh1.x + b[5] * g_sh1.y + b[6] * g_sh1.z + b[7] * g_sh1.w +
                b[8] * g_sh2.x + b[9] * g_sh2.y + b[10]* g_sh2.z + b[11]* g_sh2.w +
                b[12]* g_sh3.x + b[13]* g_sh3.y + b[14]* g_sh3.z + b[15]* g_sh3.w);

    float bl = (b[0] * b_sh0.x + b[1] * b_sh0.y + b[2] * b_sh0.z + b[3] * b_sh0.w +
                b[4] * b_sh1.x + b[5] * b_sh1.y + b[6] * b_sh1.z + b[7] * b_sh1.w +
                b[8] * b_sh2.x + b[9] * b_sh2.y + b[10]* b_sh2.z + b[11]* b_sh2.w +
                b[12]* b_sh3.x + b[13]* b_sh3.y + b[14]* b_sh3.z + b[15]* b_sh3.w);
#else
    float re = (b[0] * r_sh0.x + b[1] * r_sh0.y + b[2] * r_sh0.z + b[3] * r_sh0.w);
    float gr = (b[0] * g_sh0.x + b[1] * g_sh0.y + b[2] * g_sh0.z + b[3] * g_sh0.w);
    float bl = (b[0] * b_sh0.x + b[1] * b_sh0.y + b[2] * b_sh0.z + b[3] * b_sh0.w);
#endif
    return vec3(0.5f, 0.5f, 0.5f) + vec3(re, gr, bl);
}

#ifdef FRAMEBUFFER_SRGB
float SRGBToLinearF(float srgb)
{
    if (srgb <= 0.04045f)
    {
        return srgb / 12.92f;
    }
    else
    {
        return pow((srgb + 0.055f) / 1.055f, 2.4f);
    }
}

vec3 SRGBToLinear(const vec3 srgbColor)
{
    vec3 linearColor;
    for (int i = 0; i < 3; ++i) // Convert RGB, leave A unchanged
    {
        linearColor[i] = SRGBToLinearF(srgbColor[i]);
    }
    return linearColor;
}
#endif

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
    cov2D[0][0] += 0.3f;
    cov2D[1][1] += 0.3f;
    geom_cov2 = vec4(cov2D[0], cov2D[1]); // cram it into a vec4

    // geom_p is the gaussian center transformed into screen space
    vec4 p4 = projMat * t;
    geom_p = vec2(p4.x / p4.w, p4.y / p4.w);
    geom_p.x = 0.5f * (WIDTH + (geom_p.x * WIDTH) + (2.0f * X0));
    geom_p.y = 0.5f * (HEIGHT + (geom_p.y * HEIGHT) + (2.0f * Y0));

    // compute radiance from sh
    vec3 v = normalize(position.xyz - eye);
    geom_color = vec4(ComputeRadianceFromSH(v), alpha);

#ifdef FRAMEBUFFER_SRGB
    // The SIBR reference renderer uses sRGB throughout,
    // i.e. the splat colors are sRGB, the gaussian and alpha-blending occurs in sRGB space.
    // However, in vr our shader output must be in linear space,
    // in order for openxr color conversion to work.
    // So, we convert the splat color to linear,
    // but the guassian and alpha-blending occur in linear space.
    // This leads to results that don't quite match the SIBR reference.
    geom_color.rgb = SRGBToLinear(geom_color.rgb);
#endif

    // gl_Position is in clip coordinates.
    gl_Position = p4;
}
