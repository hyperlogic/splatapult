#version 460

layout(local_size_x = 32) in;

uniform vec3 forward;
uniform vec3 eye;

layout(std430, binding = 0) readonly buffer PosBuffer
{
    vec4 positions[];
};

layout(std430, binding = 1) writeonly buffer OutputBuffer
{
    uint quantizedZs[];
};

void main()
{
    uint idx = gl_GlobalInvocationID.x;

    if (idx >= positions.length()) return;

    float depth = dot(vec3(positions[idx]) - eye, forward);
    uint fixedPointZ = 0xffffffff - uint(clamp(depth, 0.0f, 65535.0f) * 65536.0f);

    quantizedZs[idx] = fixedPointZ;
}
