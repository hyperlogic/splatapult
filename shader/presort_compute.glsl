#version 460

layout(local_size_x = 256) in;

uniform mat4 modelViewProj;

layout(binding = 4, offset = 0) uniform atomic_uint output_count;

layout(std430, binding = 0) readonly buffer PosBuffer
{
    vec4 positions[];
};

layout(std430, binding = 1) writeonly buffer OutputBuffer
{
    uint quantizedZs[];
};

layout(std430, binding = 2) writeonly buffer OutputBuffer2
{
    uint indices[];
};

void main()
{
    uint idx = gl_GlobalInvocationID.x;

    if (idx >= positions.length())
	{
		return;
	}

	vec4 p = modelViewProj * vec4(positions[idx].xyz, 1.0f);
	float depth = p.w;
	float xx = p.x / depth;
	float yy = p.y / depth;

	const float CLIP = 1.5f;
	if (depth > 0.0f && xx < CLIP && xx > -CLIP && yy < CLIP && yy > -CLIP)
	{
		uint count = atomicCounterIncrement(output_count);
		// 16.16 fixed point
		uint fixedPointZ = 0xffffffff - uint(clamp(depth, 0.0f, 65535.0f) * 65536.0f);
		quantizedZs[count] = fixedPointZ;
		indices[count] = idx;
	}
}
