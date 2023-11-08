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

	vec4 p = modelViewProj * positions[idx];
	float depth = p.w;
	float xx = p.x / depth;
	float yy = p.y / depth;
	float zz = p.z / depth;

	if (depth > 0.0f && xx < 1.0f && xx > -1.0f && yy < 1.0f && yy > -1.0f)
	{
		uint count = atomicCounterIncrement(output_count);
		// 16.16 fixed point

		// convert -1, 1 range to 0, 1
		//float d = ((0.5f * zz) + 0.5f);

		uint fixedPointZ = 0xffffffff - uint(clamp(depth, 0.0f, 65535.0f) * 65536.0f);
		quantizedZs[count] = fixedPointZ;
		indices[count] = idx;
	}

	/*
	vec3 d = vec3(positions[idx]) - eye;
	float len = length(d);
    float depth = dot(d, forward);
	float cosTheta = depth / len;

	// TODO: cull against actual frustum. (cos (ajt-deg-to-rad 30))
	if (depth > 0.0f && cosTheta > 0.8660254037844387f)
	{
		uint count = atomicCounterIncrement(output_count);
		uint fixedPointZ = 0xffffffff - uint(clamp(depth, 0.0f, 65535.0f) * 65536.0f);
		quantizedZs[count] = fixedPointZ;
		indices[count] = idx;
	}
	*/
}
