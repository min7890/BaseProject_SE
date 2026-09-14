#include "stdafx.h"
#include "Renderer.h"

#include <climits>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}

Renderer::~Renderer()
{
	DestroyFramebuffer();
	if (m_QuadBuffer) glDeleteBuffers(1, &m_QuadBuffer);
	if (m_VBORect != 0) glDeleteBuffers(1, &m_VBORect);
	if (m_VBODiamond != 0) glDeleteBuffers(1, &m_VBODiamond);
	if (m_VBOCircle != 0) glDeleteBuffers(1, &m_VBOCircle);
	if (m_SolidShader != 0) glDeleteProgram(m_SolidShader);
	if (m_PostShader != 0) glDeleteProgram(m_PostShader);
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	m_WindowSizeX = windowSizeX > 0 ? windowSizeX : 1;
	m_WindowSizeY = windowSizeY > 0 ? windowSizeY : 1;
	glViewport(0, 0, m_WindowSizeX, m_WindowSizeY);

	m_SolidShader = CompileShaders("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	m_PostShader = CompileShaders("./Shaders/PostProcess.vs", "./Shaders/PostProcess.fs");
	CreateVertexBufferObjects();

	if (m_SolidShader != 0)
	{
		m_SolidPositionAttribute = glGetAttribLocation(m_SolidShader, "a_Position");
		m_SolidPositionUniform = glGetUniformLocation(m_SolidShader, "u_Position");
		m_SolidSizeUniform = glGetUniformLocation(m_SolidShader, "u_Size");
		m_SolidColorUniform = glGetUniformLocation(m_SolidShader, "u_Color");
	}
	if (m_PostShader != 0)
		m_PostPositionAttribute = glGetAttribLocation(m_PostShader, "a_Position");

	CreateFramebuffer();
	m_Initialized = (m_SolidShader != 0 && m_PostShader != 0 &&
		m_VBORect != 0 && m_VBODiamond != 0 && m_VBOCircle != 0);
}

bool Renderer::IsInitialized() const
{
	return m_Initialized;
}

void Renderer::SetViewport(int windowSizeX, int windowSizeY)
{
	m_WindowSizeX = windowSizeX > 0 ? windowSizeX : 1;
	m_WindowSizeY = windowSizeY > 0 ? windowSizeY : 1;
	glViewport(0, 0, m_WindowSizeX, m_WindowSizeY);
	if (m_SolidShader != 0)
		CreateFramebuffer();
}

void Renderer::CreateFramebuffer()
{
	DestroyFramebuffer();
	glGenFramebuffers(1, &m_Framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, m_Framebuffer);

	glGenTextures(1, &m_SceneTexture);
	glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_WindowSizeX, m_WindowSizeY,
		0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, m_SceneTexture, 0);

	m_FramebufferReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	if (!m_FramebufferReady)
		std::cerr << "Post-process framebuffer creation failed." << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::DestroyFramebuffer()
{
	if (m_SceneTexture != 0) glDeleteTextures(1, &m_SceneTexture);
	if (m_Framebuffer != 0) glDeleteFramebuffers(1, &m_Framebuffer);
	m_SceneTexture = 0;
	m_Framebuffer = 0;
	m_FramebufferReady = false;
}

void Renderer::BeginScene(float r, float g, float b, float a)
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferReady ? m_Framebuffer : 0);
	glViewport(0, 0, m_WindowSizeX, m_WindowSizeY);
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::EndScene(float elapsedTime)
{
	if (!m_FramebufferReady)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_WindowSizeX, m_WindowSizeY);
	glDisable(GL_BLEND);
	glUseProgram(m_PostShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
	glUniform1i(glGetUniformLocation(m_PostShader, "u_Scene"), 0);
	glUniform1f(glGetUniformLocation(m_PostShader, "u_Time"), elapsedTime);
	glUniform3fv(glGetUniformLocation(m_PostShader, "u_Lights"), 16, m_Lights);
	glUniform2f(glGetUniformLocation(m_PostShader, "u_Resolution"),
		static_cast<float>(m_WindowSizeX), static_cast<float>(m_WindowSizeY));

	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glEnableVertexAttribArray(m_PostPositionAttribute);
	glVertexAttribPointer(m_PostPositionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(m_PostPositionAttribute);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glEnable(GL_BLEND);
}

void Renderer::CreateVertexBufferObjects()
{
	glGenBuffers(1, &m_QuadBuffer);
	const float rect[] =
	{
		-0.5f, -0.5f, 0.0f, -0.5f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f,
		-0.5f, -0.5f, 0.0f,  0.5f, 0.5f, 0.0f, 0.5f, -0.5f, 0.0f
	};
	const float diamond[] =
	{
		0.0f, 0.5f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f,
		0.0f, 0.5f, 0.0f,  0.0f, -0.5f, 0.0f, 0.5f, 0.0f, 0.0f
	};

	glGenBuffers(1, &m_VBORect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);
	glGenBuffers(1, &m_VBODiamond);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBODiamond);
	glBufferData(GL_ARRAY_BUFFER, sizeof(diamond), diamond, GL_STATIC_DRAW);

	const int segments = 28;
	const float pi = 3.1415926535f;
	std::vector<float> circle;
	circle.reserve(segments * 9);
	for (int i = 0; i < segments; ++i)
	{
		const float a0 = pi * 2.0f * i / segments;
		const float a1 = pi * 2.0f * (i + 1) / segments;
		circle.push_back(0.0f); circle.push_back(0.0f); circle.push_back(0.0f);
		circle.push_back(std::cos(a0) * 0.5f); circle.push_back(std::sin(a0) * 0.5f); circle.push_back(0.0f);
		circle.push_back(std::cos(a1) * 0.5f); circle.push_back(std::sin(a1) * 0.5f); circle.push_back(0.0f);
	}
	glGenBuffers(1, &m_VBOCircle);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOCircle);
	glBufferData(GL_ARRAY_BUFFER, circle.size() * sizeof(float), &circle[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Renderer::AddShader(GLuint shaderProgram, const char* shaderText, GLenum shaderType)
{
	GLuint shaderObject = glCreateShader(shaderType);
	if (shaderObject == 0) return false;
	const GLchar* source[] = { shaderText };
	const size_t sourceLength = std::strlen(shaderText);
	if (sourceLength > INT_MAX)
	{
		glDeleteShader(shaderObject);
		return false;
	}
	const GLint length[] = { static_cast<GLint>(sourceLength) };
	glShaderSource(shaderObject, 1, source, length);
	glCompileShader(shaderObject);
	GLint success = GL_FALSE;
	glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE)
	{
		GLchar infoLog[1024] = { 0 };
		glGetShaderInfoLog(shaderObject, sizeof(infoLog), NULL, infoLog);
		std::cerr << "Shader compilation failed: " << infoLog << std::endl;
		glDeleteShader(shaderObject);
		return false;
	}
	glAttachShader(shaderProgram, shaderObject);
	glDeleteShader(shaderObject);
	return true;
}

bool Renderer::ReadFile(const char* filename, std::string* target)
{
	std::ifstream file(filename);
	if (!file)
	{
		std::cerr << filename << " file loading failed." << std::endl;
		return false;
	}
	std::string line;
	while (std::getline(file, line))
	{
		target->append(line);
		target->append("\n");
	}
	return true;
}

GLuint Renderer::CompileShaders(const char* filenameVS, const char* filenameFS)
{
	std::string vertexSource, fragmentSource;
	if (!ReadFile(filenameVS, &vertexSource) || !ReadFile(filenameFS, &fragmentSource)) return 0;
	GLuint shaderProgram = glCreateProgram();
	if (shaderProgram == 0) return 0;
	if (!AddShader(shaderProgram, vertexSource.c_str(), GL_VERTEX_SHADER) ||
		!AddShader(shaderProgram, fragmentSource.c_str(), GL_FRAGMENT_SHADER))
	{
		glDeleteProgram(shaderProgram);
		return 0;
	}
	glLinkProgram(shaderProgram);
	GLint success = GL_FALSE;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (success == GL_FALSE)
	{
		GLchar errorLog[1024] = { 0 };
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cerr << "Shader linking failed: " << errorLog << std::endl;
		glDeleteProgram(shaderProgram);
		return 0;
	}
	return shaderProgram;
}

void Renderer::DrawBuffer(GLuint buffer, int vertexCount, float x, float y,
	float width, float height, float r, float g, float b, float a)
{
	if (!m_Initialized) return;
	float glX = 0.0f, glY = 0.0f;
	GetGLPosition(x, y, &glX, &glY);
	const float glWidth = width * 2.0f / static_cast<float>(m_WindowSizeX);
	const float glHeight = height * 2.0f / static_cast<float>(m_WindowSizeY);
	glUseProgram(m_SolidShader);
	glUniform2f(m_SolidPositionUniform, glX, glY);
	glUniform2f(m_SolidSizeUniform, glWidth, glHeight);
	glUniform4f(m_SolidColorUniform, r, g, b, a);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glEnableVertexAttribArray(m_SolidPositionAttribute);
	glVertexAttribPointer(m_SolidPositionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
	glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	glDisableVertexAttribArray(m_SolidPositionAttribute);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::DrawSolidRect(float x, float y, float, float width, float height,
	float r, float g, float b, float a)
{
	DrawBuffer(m_VBORect, 6, x, y, width, height, r, g, b, a);
}

void Renderer::DrawDiamond(float x, float y, float, float width, float height,
	float r, float g, float b, float a)
{
	DrawBuffer(m_VBODiamond, 6, x, y, width, height, r, g, b, a);
}

void Renderer::DrawCircle(float x, float y, float, float width, float height,
	float r, float g, float b, float a)
{
	DrawBuffer(m_VBOCircle, 84, x, y, width, height, r, g, b, a);
}

void Renderer::GetGLPosition(float x, float y, float* newX, float* newY) const
{
	*newX = x * 2.0f / static_cast<float>(m_WindowSizeX);
	*newY = y * 2.0f / static_cast<float>(m_WindowSizeY);
}
void Renderer::SetLight(int index, float x, float y, float radius)
{
	if (index < 0 || index >= 16) return;
	m_Lights[index * 3] = x + m_WindowSizeX * 0.5f;
	m_Lights[index * 3 + 1] = y + m_WindowSizeY * 0.5f;
	m_Lights[index * 3 + 2] = radius;
}

void Renderer::DrawQuad(const float* points, float r, float g, float b, float a)
{
	if (!m_Initialized) return;
	const int indices[] = { 0, 1, 2, 0, 2, 3 };
	float vertices[18];
	for (int i = 0; i < 6; ++i) {
		vertices[i * 3] = points[indices[i] * 2];
		vertices[i * 3 + 1] = points[indices[i] * 2 + 1];
		vertices[i * 3 + 2] = 0.0f;
	}
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	DrawBuffer(m_QuadBuffer, 6, 0, 0, 1, 1, r, g, b, a);
}
