#include "vertexbuffer.h"

#include <cassert>
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#include "util.h"

BufferObject::BufferObject(int targetIn)
{
	// limit the types for safety... these are the only one I actually use.
	assert(targetIn == GL_ARRAY_BUFFER || targetIn == GL_ELEMENT_ARRAY_BUFFER);
	target = targetIn;
    glGenBuffers(1, &obj);
	elementSize = 0;
	numElements = 0;
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

// AJT: REMOVE
void BufferObject::BufferStorage(int target, size_t numBytes, void* data, int flags)
{
	dataVec.reserve(numBytes);
	uint8_t* p = (uint8_t*)data;
	for (int i = 0; i < numBytes; i++)
	{
		dataVec.push_back(p[i]);
	}
}

void BufferObject::Store(const std::vector<float>& data)
{
	Bind();
	GL_ERROR_CHECK("before BufferObject::Store<float>");
    glBufferStorage(target, sizeof(float) * data.size(), (void*)data.data(), 0);
	GL_ERROR_CHECK("BufferObject::Store<float>");
	Unbind();
	elementSize = 1;
	numElements = (int)data.size();
}

void BufferObject::Store(const std::vector<glm::vec2>& data)
{
	Bind();
	GL_ERROR_CHECK("before BufferObject::Store<vec2>");
    glBufferStorage(target, sizeof(glm::vec2) * data.size(), (void*)data.data(), 0);
	GL_ERROR_CHECK("BufferObject::Store<vec2>");
	Unbind();
	elementSize = 2;
	numElements = (int)data.size();
}

void BufferObject::Store(const std::vector<glm::vec3>& data)
{
	Bind();
	GL_ERROR_CHECK("before BufferObject::Store<vec3>");
    glBufferStorage(target, sizeof(glm::vec3) * data.size(), (void*)data.data(), 0);
	GL_ERROR_CHECK("BufferObject::Store<vec3>");
	Unbind();
	elementSize = 3;
	numElements = (int)data.size();
}

void BufferObject::Store(const std::vector<glm::vec4>& data)
{
	Bind();
	GL_ERROR_CHECK("before BufferObject::Store<vec4>");
    glBufferStorage(target, sizeof(glm::vec4) * data.size(), (void*)data.data(), 0);
	GL_ERROR_CHECK("BufferObject::Store<vec4>");
	Unbind();
	elementSize = 4;
	numElements = (int)data.size();
}

void BufferObject::Store(const std::vector<uint32_t>& data)
{
	Bind();
	GL_ERROR_CHECK("before BufferObject::Store<uint32>");
    glBufferStorage(target, sizeof(uint32_t) * data.size(), (void*)data.data(), 0);
	GL_ERROR_CHECK("BufferObject::Store<uint32>");
	Unbind();
	elementSize = 1;
	numElements = (int)data.size();
}

void BufferObject::Check(const uint8_t* data)
{
	for (size_t i = 0; i < dataVec.size(); i++)
	{
		assert(data[i] == dataVec[i]);
	}
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

	/*
	locVec.push_back(loc);
	attribBufferVec.push_back(attribBuffer);
	*/
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

	//AJT
	/*
	for (size_t i = 0; i < locVec.size(); i++)
	{
		attribBufferVec[i]->Bind();
		glVertexAttribPointer(locVec[i], attribBufferVec[i]->elementSize, GL_FLOAT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(locVec[i]);
	}
	elementBuffer->Bind();
	glDrawElements(GL_TRIANGLES, elementBuffer->numElements, GL_UNSIGNED_INT, nullptr);
	*/

	//AJT
	/*
	for (size_t i = 0; i < locVec.size(); i++)
	{
		glVertexAttribPointer(locVec[i], attribBufferVec[i]->elementSize, GL_FLOAT, GL_FALSE, 0, (void*)attribBufferVec[i]->dataVec.data());
		glEnableVertexAttribArray(locVec[i]);
	}
	glDrawElements(GL_TRIANGLES, elementBuffer->numElements, GL_UNSIGNED_INT, (void*)elementBuffer->dataVec.data());
	*/
}

void VertexArrayObject::CheckArrays(const std::vector<glm::vec3>& posVec,
									const std::vector<glm::vec2>& uvVec,
									const std::vector<glm::vec4>& colorVec,
									const std::vector<uint32_t> indexVec)
{
	// compare arrays byte by fucking byte, they should be identical!
	for (auto&& buffer : attribBufferVec)
	{
		//
	}

	elementBuffer->Check((uint8_t*)indexVec.data());
}
