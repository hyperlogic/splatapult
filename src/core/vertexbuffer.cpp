/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "vertexbuffer.h"

#include <cassert>
#include <string.h>

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif

#include "util.h"

#ifdef __ANDROID__
static void glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags)
{
	GLenum usage = 0;
	if (flags & GL_DYNAMIC_STORAGE_BIT)
	{
		if (flags & GL_MAP_READ_BIT)
		{
			usage = GL_DYNAMIC_READ;
		}
		else
		{
			usage = GL_DYNAMIC_DRAW;
		}
	}
	else
	{
		if (flags & GL_MAP_READ_BIT)
		{
			usage = GL_STATIC_READ;
		}
		else
		{
			usage = GL_STATIC_DRAW;
		}
	}
	glBufferData(target, size, data, usage);
}
#endif

BufferObject::BufferObject(int targetIn, void* data, size_t size, unsigned int flags)
{
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, size, data, flags);
	Unbind();
	elementSize = 0;
	numElements = 0;
}

BufferObject::BufferObject(int targetIn, const std::vector<float>& data, unsigned int flags)
{
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(float) * data.size(), (void*)data.data(), flags);
	Unbind();
	elementSize = 1;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<glm::vec2>& data, unsigned int flags)
{
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(glm::vec2) * data.size(), (void*)data.data(), flags);
	Unbind();
	elementSize = 2;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<glm::vec3>& data, unsigned int flags)
{
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(glm::vec3) * data.size(), (void*)data.data(), flags);
	Unbind();
	elementSize = 3;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<glm::vec4>& data, unsigned int flags)
{
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(glm::vec4) * data.size(), (void*)data.data(), flags);
	Unbind();
	elementSize = 4;
	numElements = (int)data.size();
}

BufferObject::BufferObject(int targetIn, const std::vector<uint32_t>& data, unsigned int flags)
{
	target = targetIn;
    glGenBuffers(1, &obj);
	Bind();
    glBufferStorage(target, sizeof(uint32_t) * data.size(), (void*)data.data(), flags);
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

void BufferObject::Read(std::vector<uint32_t>& data)
{
	Bind();
	size_t bufferSize = sizeof(uint32_t) * data.size();
	assert(bufferSize == (elementSize * sizeof(uint32_t) * numElements));
	//void* rawBuffer = glMapBuffer(target, GL_READ_ONLY);
	void* rawBuffer = glMapBufferRange(target, 0, bufferSize, GL_MAP_READ_BIT);
	if (rawBuffer)
	{
		memcpy((void*)data.data(), rawBuffer, bufferSize);
	}
	glUnmapBuffer(target);
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

void VertexArrayObject::DrawElements(int mode) const
{
	Bind();
	glDrawElements((GLenum)mode, elementBuffer->numElements, GL_UNSIGNED_INT, nullptr);
	Unbind();
}

