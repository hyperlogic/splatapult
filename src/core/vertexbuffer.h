/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

class VertexArrayObject;

#ifdef __ANDROID__
// AJT: ANDROID: TODO: HACK TO WORK AROUND glBufferStorage
#define GL_DYNAMIC_STORAGE_BIT            0x0100
#define GL_MAP_READ_BIT                   0x0001
#endif

class BufferObject
{
	friend class VertexArrayObject;
public:

	// targetIn should be one of the following.
	//     GL_ARRAY_BUFFER, GL_ATOMIC_COUNTER_BUFFER, GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER
	//     GL_DISPATCH_INDIRECT_BUFFER, GL_DRAW_INDIRECT_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
	//     GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_QUERY_BUFFER, GL_SHADER_STORAGE_BUFFER,
	//     GL_TEXTURE_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER
	// flags can one of the following bitfields.
	//     GL_DYNAMIC_STORAGE_BIT, GL_MAP_READ_BIT, GL_MAP_WRITE_BIT, GL_MAP_PERSISTENT_BIT
	//     GL_MAP_COHERENT_BIT, GL_CLIENT_STORAGE_BIT
	BufferObject(int targetIn, void* data, size_t size, unsigned int flags = 0);
    BufferObject(int targetIn, const std::vector<float>& data, unsigned int flags = 0);
	BufferObject(int targetIn, const std::vector<glm::vec2>& data, unsigned int flags = 0);
	BufferObject(int targetIn, const std::vector<glm::vec3>& data, unsigned int flags = 0);
	BufferObject(int targetIn, const std::vector<glm::vec4>& data, unsigned int flags = 0);
	BufferObject(int targetIn, const std::vector<uint32_t>& data, unsigned int flags = 0);
	BufferObject(const BufferObject& orig) = delete;
    ~BufferObject();

	void Bind() const;
	void Unbind() const;

	void Update(const std::vector<float>& data);
	void Update(const std::vector<glm::vec2>& data);
	void Update(const std::vector<glm::vec3>& data);
	void Update(const std::vector<glm::vec4>& data);
	void Update(const std::vector<uint32_t>& data);

	void Read(std::vector<uint32_t>& data);

	uint32_t GetObj() const { return obj; }

protected:
	int target;
    uint32_t obj;
	int elementSize;  // vec2 = 2, vec3 = 3 etc.
	int numElements;  // number of vec2, vec3 in buffer
};

class VertexArrayObject
{
public:
	VertexArrayObject();
	~VertexArrayObject();

	void Bind() const;
	void Unbind() const;

	void SetAttribBuffer(int loc, std::shared_ptr<BufferObject> attribBufferIn);
	void SetElementBuffer(std::shared_ptr<BufferObject> elementBufferIn);
	std::shared_ptr<BufferObject> GetElementBuffer() const { return elementBuffer; }
	void DrawElements(int mode) const;

protected:
	uint32_t obj;
	std::vector<std::shared_ptr<BufferObject>> attribBufferVec;
	std::shared_ptr<BufferObject> elementBuffer;
};
