#pragma once

#include <string>
#include "Dependencies\glew.h"

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized() const;
	void SetViewport(int windowSizeX, int windowSizeY);
	void BeginScene(float r, float g, float b, float a);
	void EndScene(float elapsedTime);
	void SetLight(int index, float x, float y, float radius);
	void DrawQuad(const float* points, float r, float g, float b, float a);

	void DrawSolidRect(float x, float y, float z, float width, float height,
		float r, float g, float b, float a);
	void DrawDiamond(float x, float y, float z, float width, float height,
		float r, float g, float b, float a);
	void DrawCircle(float x, float y, float z, float width, float height,
		float r, float g, float b, float a);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(const char* filename, std::string* target);
	bool AddShader(GLuint shaderProgram, const char* shaderText, GLenum shaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void CreateFramebuffer();
	void DestroyFramebuffer();
	void DrawBuffer(GLuint buffer, int vertexCount, float x, float y,
		float width, float height, float r, float g, float b, float a);
	void GetGLPosition(float x, float y, float* newX, float* newY) const;

	bool m_Initialized = false;
	bool m_FramebufferReady = false;
	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	GLuint m_VBORect = 0;
	GLuint m_VBODiamond = 0;
	GLuint m_VBOCircle = 0;
	GLuint m_QuadBuffer = 0;
	float m_Lights[16 * 3] = { 0 };
	GLuint m_SolidShader = 0;
	GLuint m_PostShader = 0;
	GLuint m_Framebuffer = 0;
	GLuint m_SceneTexture = 0;

	GLint m_SolidPositionAttribute = -1;
	GLint m_SolidPositionUniform = -1;
	GLint m_SolidSizeUniform = -1;
	GLint m_SolidColorUniform = -1;
	GLint m_PostPositionAttribute = -1;
};
