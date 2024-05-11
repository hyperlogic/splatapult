/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "camerapathrenderer.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#endif

#include "core/log.h"
#include "core/util.h"

#include "camerasconfig.h"

CameraPathRenderer::CameraPathRenderer()
{
}

CameraPathRenderer::~CameraPathRenderer()
{
}

bool CameraPathRenderer::Init(const std::vector<Camera>& cameraVec)
{
	ddProg = std::make_shared<Program>();
    if (!ddProg->LoadVertFrag("shader/debugdraw_vert.glsl", "shader/debugdraw_frag.glsl"))
    {
        Log::E("Error loading CameraPathRenderer shader!\n");
        return false;
    }

	BuildCamerasVao(cameraVec);
	BuildPathVao(cameraVec);

	return true;
}

// viewport = (x, y, width, height)
void CameraPathRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
								const glm::vec4& viewport, const glm::vec2& nearFar)
{
	if (!showCameras && !showPath)
	{
		return;
	}

	GL_ERROR_CHECK("CameraPathRenderer::Render() begin");

	glm::mat4 modelViewProjMat = projMat * glm::inverse(cameraMat);

	ddProg->Bind();
	ddProg->SetUniform("modelViewProjMat", modelViewProjMat);

	if (showCameras)
	{
		camerasVao->Bind();
		glDrawElements(GL_LINES, numCameraVerts, GL_UNSIGNED_INT, nullptr);
		camerasVao->Unbind();
	}

	if (showPath)
	{
		pathVao->Bind();
		glDrawElements(GL_LINES, numPathVerts, GL_UNSIGNED_INT, nullptr);
		pathVao->Unbind();
	}

	GL_ERROR_CHECK("CameraPathRenderer::Render() draw");
}

void CameraPathRenderer::BuildCamerasVao(const std::vector<Camera>& cameraVec)
{
	camerasVao = std::make_shared<VertexArrayObject>();

	const uint32_t NUM_LINES = 8;
	const float FRUSTUM_LEN = 0.1f;
	const glm::vec4 FRUSTUM_COLOR(0.0f, 1.0f, 0.0f, 1.0f);

	numCameraVerts = (uint32_t)cameraVec.size() * NUM_LINES * 2;

	std::vector<glm::vec3> posVec;
	posVec.reserve(numCameraVerts);
	std::vector<glm::vec4> colVec;
	colVec.reserve(numCameraVerts);
	std::vector<uint32_t> indexVec;
	indexVec.reserve(numCameraVerts);

	// build lines for each frustum
	for (auto& c : cameraVec)
	{
		float xRadius = FRUSTUM_LEN / cosf(c.fov.x / 2.0f);
		float xOffset = xRadius * sinf(c.fov.x / 2.0f);
		float yRadius = FRUSTUM_LEN / cosf(c.fov.y / 2.0f);
		float yOffset = yRadius * sinf(c.fov.y / 2.0f);

		const uint32_t NUM_FRUSTUM_VERTS = 5;
		glm::vec3 verts[NUM_FRUSTUM_VERTS] = {
		    glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(xOffset, yOffset, -FRUSTUM_LEN),
			glm::vec3(-xOffset, yOffset, -FRUSTUM_LEN),
			glm::vec3(-xOffset, -yOffset, -FRUSTUM_LEN),
			glm::vec3(xOffset, -yOffset, -FRUSTUM_LEN)
		};
		for (int i = 0; i < NUM_FRUSTUM_VERTS; i++)
		{
			verts[i] = XformPoint(c.mat, verts[i]);
		}

		posVec.push_back(verts[0]); posVec.push_back(verts[1]);
		posVec.push_back(verts[0]); posVec.push_back(verts[2]);
		posVec.push_back(verts[0]); posVec.push_back(verts[3]);
		posVec.push_back(verts[0]); posVec.push_back(verts[4]);

		posVec.push_back(verts[1]); posVec.push_back(verts[2]);
		posVec.push_back(verts[2]); posVec.push_back(verts[3]);
		posVec.push_back(verts[3]); posVec.push_back(verts[4]);
		posVec.push_back(verts[4]); posVec.push_back(verts[1]);

		for (int i = 0; i < NUM_LINES; i++)
		{
			colVec.push_back(FRUSTUM_COLOR); colVec.push_back(FRUSTUM_COLOR);
		}
	}

    auto posBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
    auto colBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colVec);

	// build element array
    assert(numCameraVerts <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < numCameraVerts; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

	// setup vertex array object with buffers
    camerasVao->SetAttribBuffer(ddProg->GetAttribLoc("position"), posBuffer);
    camerasVao->SetAttribBuffer(ddProg->GetAttribLoc("color"), colBuffer);
    camerasVao->SetElementBuffer(indexBuffer);
}

void CameraPathRenderer::BuildPathVao(const std::vector<Camera>& cameraVec)
{
	pathVao = std::make_shared<VertexArrayObject>();

	const uint32_t NUM_LINES = 1;
	const glm::vec4 PATH_COLOR(0.0f, 1.0f, 1.0f, 1.0f);

	numPathVerts = (uint32_t)cameraVec.size() * NUM_LINES * 2;

	std::vector<glm::vec3> posVec;
	posVec.reserve(numPathVerts);
	std::vector<glm::vec4> colVec;
	colVec.reserve(numPathVerts);
	std::vector<uint32_t> indexVec;
	indexVec.reserve(numPathVerts);

	// build lines for each path segment
	if (cameraVec.size() > 1)
	{
		const Camera* prev = &cameraVec[0];
		for (size_t i = 1; i < cameraVec.size(); i++)
		{
			const Camera* curr = cameraVec.data() + i;
			glm::vec3 prevPos = glm::vec3(prev->mat[3]);
			glm::vec3 currPos = glm::vec3(curr->mat[3]);
			posVec.push_back(prevPos);
			posVec.push_back(currPos);
			colVec.push_back(PATH_COLOR);
			colVec.push_back(PATH_COLOR);
			prev = curr;
		}
	}
	else
	{
		posVec.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
		posVec.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
		colVec.push_back(PATH_COLOR);
		colVec.push_back(PATH_COLOR);
	}

    auto posBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
    auto colBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colVec);

	// build element array
    assert(numPathVerts <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < numPathVerts; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

	// setup vertex array object with buffers
    pathVao->SetAttribBuffer(ddProg->GetAttribLoc("position"), posBuffer);
    pathVao->SetAttribBuffer(ddProg->GetAttribLoc("color"), colBuffer);
    pathVao->SetElementBuffer(indexBuffer);
}
