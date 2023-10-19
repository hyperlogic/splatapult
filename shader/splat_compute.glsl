#version 460

layout(local_size_x = 256) in;

uniform vec3 forward;
uniform vec3 eye;

layout(std430, binding = 0) readonly buffer PosBuffer
{
    vec3 positions[];
};

layout(std430, binding = 1) writeonly buffer OutputBuffer
{
    uint quantizedZs[];
};

void main()
{
    uint idx = gl_GlobalInvocationID.x;

    if (idx >= positions.length()) return;

    float depth = dot(positions[idx] - eye, forward);

    uint fixedPointZ = uint(clamp(depth * 65536.0, float(0x00000000), float(0xFFFFFFFF)));

    quantizedZs[idx] = fixedPointZ;
}
