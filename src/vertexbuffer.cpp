#include "vertexbuffer.h"

#include <cassert>
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#include "util.h"

BufferObject::BufferObject(int targetIn, const std::vector<float>& data, bool isDynamic)
{
	// limit the types for safety... these are the only one I actually use.
	assert(targetIn == GL_ARRAY_BUFFER || targetIn == GL_ELEMENT_ARRAY_BUFFER);
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(float) * data.size(), (void*)data.data(), isDynamic ? GL_DYNAMIC_STORAGE_BIT : 0);
	Unbind();
	elementSize = 1;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<glm::vec2>& data, bool isDynamic)
{
	// limit the types for safety... these are the only one I actually use.
	assert(targetIn == GL_ARRAY_BUFFER || targetIn == GL_ELEMENT_ARRAY_BUFFER);
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(glm::vec2) * data.size(), (void*)data.data(), isDynamic ? GL_DYNAMIC_STORAGE_BIT : 0);
	Unbind();
	elementSize = 2;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<glm::vec3>& data, bool isDynamic)
{
	// limit the types for safety... these are the only one I actually use.
	assert(targetIn == GL_ARRAY_BUFFER || targetIn == GL_ELEMENT_ARRAY_BUFFER);
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(glm::vec3) * data.size(), (void*)data.data(), isDynamic ? GL_DYNAMIC_STORAGE_BIT : 0);
	Unbind();
	elementSize = 3;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<glm::vec4>& data, bool isDynamic)
{
	// limit the types for safety... these are the only one I actually use.
	assert(targetIn == GL_ARRAY_BUFFER || targetIn == GL_ELEMENT_ARRAY_BUFFER);
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(glm::vec4) * data.size(), (void*)data.data(), isDynamic ? GL_DYNAMIC_STORAGE_BIT : 0);
	Unbind();
	elementSize = 4;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<uint32_t>& data, bool isDynamic)
{
	// limit the types for safety... these are the only one I actually use.
	assert(targetIn == GL_ARRAY_BUFFER || targetIn == GL_ELEMENT_ARRAY_BUFFER);
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(uint32_t) * data.size(), (void*)data.data(), isDynamic ? GL_DYNAMIC_STORAGE_BIT : 0);
	Unbind();
	elementSize = 1;
	numElements = (int)data.size();
}

BufferObject::~BufferObject()
{
    glDeleteBuffers(1, &obj);
}

void BufferObject::Bind() const
{
	glBindBuffer(target, obj);
}

void BufferObject::Unbind() const
{
	glBindBuffer(target, 0);
}

void BufferObject::Update(const std::vector<float>& data)
{
	Bind();
    glBufferSubData(target, 0, sizeof(float) * data.size(), (void*)data.data());
	Unbind();
}

void BufferObject::Update(const std::vector<glm::vec2>& data)
{
	Bind();
    glBufferSubData(target, 0, sizeof(glm::vec2) * data.size(), (void*)data.data());
	Unbind();
}

void BufferObject::Update(const std::vector<glm::vec3>& data)
{
	Bind();
    glBufferSubData(target, 0, sizeof(glm::vec3) * data.size(), (void*)data.data());
	Unbind();
}

void BufferObject::Update(const std::vector<glm::vec4>& data)
{
	Bind();
    glBufferSubData(target, 0, sizeof(glm::vec4) * data.size(), (void*)data.data());
	Unbind();
}

void BufferObject::Update(const std::vector<uint32_t>& data)
{
	Bind();
    glBufferSubData(target, 0, sizeof(uint32_t) * data.size(), (void*)data.data());
	Unbind();
}


VertexArrayObject::VertexArrayObject()
{
	glGenVertexArrays(1, &obj);
}

VertexArrayObject::~VertexArrayObject()
{
	glDeleteVertexArrays(1, &obj);
}

void VertexArrayObject::Bind() const
{
	glBindVertexArray(obj);
}

void VertexArrayObject::Unbind() const
{
	glBindVertexArray(0);
}

void VertexArrayObject::SetAttribBuffer(int loc, std::shared_ptr<BufferObject> attribBuffer)
{
	assert(attribBuffer->target == GL_ARRAY_BUFFER);

	Bind();
	attribBuffer->Bind();
	glVertexAttribPointer(loc, attribBuffer->elementSize, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(loc);
	attribBuffer->Unbind();
	attribBufferVec.push_back(attribBuffer);
	Unbind();
}

void VertexArrayObject::SetElementBuffer(std::shared_ptr<BufferObject> elementBufferIn)
{
	assert(elementBufferIn->target == GL_ELEMENT_ARRAY_BUFFER);
	elementBuffer = elementBufferIn;

	Bind();
	elementBufferIn->Bind();
	Unbind();
}

void VertexArrayObject::Draw() const
{
	Bind();
	glDrawElements(GL_TRIANGLES, elementBuffer->numElements, GL_UNSIGNED_INT, nullptr);
	Unbind();
}
