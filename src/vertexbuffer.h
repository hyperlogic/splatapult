#ifndef VERTEXBUFFER_H
#define VERTEXBUFFER_H

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

class VertexArrayObject;

class BufferObject
{
	friend class VertexArrayObject;
public:
    BufferObject(int targetIn);
    ~BufferObject();

	void Bind() const;
	void Unbind() const;
	void Store(const std::vector<float>& data);
	void Store(const std::vector<glm::vec2>& data);
	void Store(const std::vector<glm::vec3>& data);
	void Store(const std::vector<glm::vec4>& data);
	void Store(const std::vector<uint32_t>& data);

	// AJT: TEMP HELPER FOR DEBUGGG
	void BufferStorage(int target, size_t numBytes, void* data, int flags);

	// AJT: TEMP FOR DEBUGGIGN
	void Check(const uint8_t* data);

protected:
	int target;
    uint32_t obj;
	int elementSize;  // vec2 = 2, vec3 = 3 etc.
	int numElements;  // number of vec2, vec3 in buffer

	// AJT TODO REMOVE in an effort to debug this fucking thing!
	std::vector<uint8_t> dataVec;
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
	void Draw() const;

	// AJT: FUCKING DEBUG
	void CheckArrays(const std::vector<glm::vec3>& posVec,
					 const std::vector<glm::vec2>& uvVec,
					 const std::vector<glm::vec4>& colorVec,
					 const std::vector<uint32_t> indexVec);

protected:
	uint32_t obj;

	// AJT: TODO REMOVE
	std::vector<int> locVec;
	
	std::vector<std::shared_ptr<BufferObject>> attribBufferVec;
	std::shared_ptr<BufferObject> elementBuffer;
};

#endif
