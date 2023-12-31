#include "Graphics.h"
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <glew/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <exception>

#ifdef _WIN32
#include <Windows.h>
// Prefer the high-performance GPU on switchable GPU systems
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace Turso3D
{
	static const unsigned glPrimitiveTypes[] =
	{
		GL_LINES,
		GL_TRIANGLES
	};

	static const unsigned glCompareFuncs[] =
	{
		GL_NEVER,
		GL_LESS,
		GL_EQUAL,
		GL_LEQUAL,
		GL_GREATER,
		GL_NOTEQUAL,
		GL_GEQUAL,
		GL_ALWAYS,
	};

	static const unsigned glSrcBlend[] =
	{
		GL_ONE,
		GL_ONE,
		GL_DST_COLOR,
		GL_SRC_ALPHA,
		GL_SRC_ALPHA,
		GL_ONE,
		GL_ONE_MINUS_DST_ALPHA,
		GL_ONE,
		GL_SRC_ALPHA
	};

	static const unsigned glDestBlend[] =
	{
		GL_ZERO,
		GL_ONE,
		GL_ZERO,
		GL_ONE_MINUS_SRC_ALPHA,
		GL_ONE,
		GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA,
		GL_ONE,
		GL_ONE
	};

	static const unsigned glBlendOp[] =
	{
		GL_FUNC_ADD,
		GL_FUNC_ADD,
		GL_FUNC_ADD,
		GL_FUNC_ADD,
		GL_FUNC_ADD,
		GL_FUNC_ADD,
		GL_FUNC_ADD,
		GL_FUNC_REVERSE_SUBTRACT,
		GL_FUNC_REVERSE_SUBTRACT
	};

	unsigned occlusionQueryType = GL_SAMPLES_PASSED;

#ifdef _DEBUG
	void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
	{
		LOG_RAW("[GL DEBUG] [source: {:d}] [type: {:d}] [id: {:d}] [severity: {:d}] {:s}", source, type, id, severity, std::string(message, length));
	}
#endif

	// ==========================================================================================
	Graphics::Marker::Marker(const char* name)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
	}
	Graphics::Marker::~Marker()
	{
		glPopDebugGroup();
	}

	// ==========================================================================================
	Graphics::Graphics() :
		window(nullptr),
		context(nullptr),
		lastBlendMode(MAX_BLEND_MODES),
		lastCullMode(MAX_CULL_MODES),
		lastDepthTest(MAX_COMPARE_MODES),
		lastColorWrite(true),
		lastDepthWrite(true),
		lastDepthBias(false),
		vsync(false),
		hasInstancing(false),
		instancingEnabled(false)
	{
	}

	Graphics::~Graphics()
	{
		if (context) {
			context = nullptr;
		}

		if (window) {
			glfwDestroyWindow(window);
			window = nullptr;
		}

		glfwTerminate();
	}

	bool Graphics::Initialize(const char* windowTitle, const IntVector2& windowSize)
	{
		if (context) {
			return true;
		}

		if (!glfwInit()) {
			throw std::exception("Failed to initialize GLFW.");
		}

#ifdef _DEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

		glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		lastWindowPos = IntVector2::ZERO;
		lastWindowSize = windowSize;

		window = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle, nullptr, nullptr);
		if (!window) {
			throw std::exception("Failed to create GLFW window.");
		}

		glfwMakeContextCurrent(window);
		context = glfwGetCurrentContext();
		if (!context) {
			LOG_ERROR("Could not create OpenGL context");
			return false;
		}

		GLenum err = glewInit();
		if (err != GLEW_OK || !GLEW_VERSION_3_3) {
			LOG_ERROR("Could not initialize OpenGL 3.3");
			return false;
		}

		// "Any samples passed" is potentially faster if supported
		if (GLEW_VERSION_3_3) {
			occlusionQueryType = GL_ANY_SAMPLES_PASSED;
		}

#if 0 //def _DEBUG
		glEnable(GL_DEBUG_OUTPUT);

		if (GLEW_KHR_debug)
		{
			LOG_DEBUG("KHR_debug extension found");
			GLDEBUGPROC p = &(GLDebugCallback);
			glDebugMessageCallback(p, nullptr);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			LOG_DEBUG("debug callback enabled.");
		}
		else if (GL_ARB_debug_output)
		{
			LOG_DEBUG("GL_ARB_debug_output extension found.");
			GLDEBUGPROC p = &(GLDebugCallback);
			glDebugMessageCallbackARB(p, nullptr);
			LOG_DEBUG("debug callback enabled.");
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		}
		else
		{
			LOG_DEBUG("KHR_debug extension NOT found.");
		}
#endif

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_FRAMEBUFFER_SRGB);
		glClearDepth(1.0f);
		glDepthRange(0.0f, 1.0f);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);

		GLuint defaultVao;
		glGenVertexArrays(1, &defaultVao);
		glBindVertexArray(defaultVao);

		// Use texcoords 3-5 for instancing if supported
		if (glVertexAttribDivisorARB) {
			hasInstancing = true;

			glVertexAttribDivisorARB(ATTR_TEXCOORD3, 1);
			glVertexAttribDivisorARB(ATTR_TEXCOORD4, 1);
			glVertexAttribDivisorARB(ATTR_TEXCOORD5, 1);
		}

		DefineQuadVertexBuffer();
		SetVSync(vsync);

		return true;
	}

	void Graphics::Resize(const IntVector2& size)
	{
		glfwSetWindowSize(window, size.x, size.y);
	}

	void Graphics::SetFullscreen(bool enable)
	{
		GLFWmonitor* monitor = glfwGetWindowMonitor(window);
		if (enable) {
			if (monitor) {
				return; // Already fullscreen mode
			}
			monitor = glfwGetPrimaryMonitor();

			glfwGetWindowPos(window, &lastWindowPos.x, &lastWindowPos.y);
			glfwGetWindowSize(window, &lastWindowSize.x, &lastWindowSize.y);

			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

		} else {
			if (!monitor) {
				return; // Already windowed mode
			}
			glfwSetWindowMonitor(window, nullptr, lastWindowPos.x, lastWindowPos.y, lastWindowSize.x, lastWindowSize.y, GLFW_DONT_CARE);
		}
	}

	void Graphics::SetVSync(bool enable)
	{
		if (IsInitialized()) {
			glfwSwapInterval(enable ? 1 : 0);
			vsync = enable;
		}
	}

	void Graphics::Present()
	{
		glfwSwapBuffers(window);
	}

	void Graphics::SetFrameBuffer(FrameBuffer* buffer)
	{
		if (buffer) {
			buffer->Bind();
		} else {
			FrameBuffer::Unbind();
		}
	}

	void Graphics::SetViewport(const IntRect& viewRect)
	{
		glViewport(viewRect.left, viewRect.top, viewRect.right - viewRect.left, viewRect.bottom - viewRect.top);
	}

	ShaderProgram* Graphics::SetProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines)
	{
		std::shared_ptr<ShaderProgram> program = CreateProgram(shaderName, vsDefines, fsDefines);
		if (program && program->Bind()) {
			return program.get();
		}
		return nullptr;
	}

	std::shared_ptr<ShaderProgram> Graphics::CreateProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines)
	{
		ResourceCache* cache = ResourceCache::Instance();
		std::shared_ptr<Shader> shader = cache->LoadResource<Shader>(shaderName);
		if (shader) {
			return shader->CreateProgram(vsDefines, fsDefines);
		}
		return {};
	}

	void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, float value)
	{
		if (program) {
			int location = program->Uniform(uniform);
			if (location >= 0) {
				glUniform1f(location, value);
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector2& value)
	{
		if (program) {
			int location = program->Uniform(uniform);
			if (location >= 0) {
				glUniform2fv(location, 1, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector3& value)
	{
		if (program) {
			int location = program->Uniform(uniform);
			if (location >= 0) {
				glUniform3fv(location, 1, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector4& value)
	{
		if (program) {
			int location = program->Uniform(uniform);
			if (location >= 0) {
				glUniform4fv(location, 1, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Matrix3x4& value)
	{
		if (program) {
			int location = program->Uniform(uniform);
			if (location >= 0) {
				glUniformMatrix3x4fv(location, 1, GL_FALSE, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Matrix4& value)
	{
		if (program) {
			int location = program->Uniform(uniform);
			if (location >= 0) {
				glUniformMatrix4fv(location, 1, GL_FALSE, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, const char* name, float value)
	{
		if (program) {
			int location = program->Uniform(name);
			if (location >= 0) {
				glUniform1f(location, value);
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, const char* name, const Vector2& value)
	{
		if (program) {
			int location = program->Uniform(name);
			if (location >= 0) {
				glUniform2fv(location, 1, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, const char* name, const Vector3& value)
	{
		if (program) {
			int location = program->Uniform(name);
			if (location >= 0) {
				glUniform3fv(location, 1, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, const char* name, const Vector4& value)
	{
		if (program) {
			int location = program->Uniform(name);
			if (location >= 0) {
				glUniform4fv(location, 1, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, const char* name, const Matrix3x4& value)
	{
		if (program) {
			int location = program->Uniform(name);
			if (location >= 0) {
				glUniformMatrix3x4fv(location, 1, GL_FALSE, value.Data());
			}
		}
	}

	void Graphics::SetUniform(ShaderProgram* program, const char* name, const Matrix4& value)
	{
		if (program) {
			int location = program->Uniform(name);
			if (location >= 0) {
				glUniformMatrix4fv(location, 1, GL_FALSE, value.Data());
			}
		}
	}

	void Graphics::SetUniformBuffer(size_t index, UniformBuffer* buffer)
	{
		if (buffer) {
			buffer->Bind(index);
		} else {
			UniformBuffer::Unbind(index);
		}
	}

	void Graphics::SetTexture(size_t index, Texture* texture)
	{
		if (texture) {
			texture->Bind(index);
		} else {
			Texture::Unbind(index);
		}
	}

	void Graphics::SetVertexBuffer(VertexBuffer* buffer, ShaderProgram* program)
	{
		if (buffer && program) {
			buffer->Bind(program->Attributes());
		}
	}

	void Graphics::SetIndexBuffer(IndexBuffer* buffer)
	{
		if (buffer) {
			buffer->Bind();
		}
	}

	void Graphics::SetRenderState(BlendMode blendMode, CullMode cullMode, CompareMode depthTest, bool colorWrite, bool depthWrite)
	{
		if (blendMode != lastBlendMode) {
			if (blendMode == BLEND_REPLACE) {
				glDisable(GL_BLEND);
			} else {
				glEnable(GL_BLEND);
				glBlendFunc(glSrcBlend[blendMode], glDestBlend[blendMode]);
				glBlendEquation(glBlendOp[blendMode]);
			}
			lastBlendMode = blendMode;
		}

		if (cullMode != lastCullMode) {
			if (cullMode == CULL_NONE) {
				glDisable(GL_CULL_FACE);
			} else {
				// Use Direct3D convention, ie. clockwise vertices define a front face
				glEnable(GL_CULL_FACE);
				glCullFace(cullMode == CULL_BACK ? GL_FRONT : GL_BACK);
			}
			lastCullMode = cullMode;
		}

		if (depthTest != lastDepthTest) {
			glDepthFunc(glCompareFuncs[depthTest]);
			lastDepthTest = depthTest;
		}

		if (colorWrite != lastColorWrite) {
			GLboolean newColorWrite = colorWrite ? GL_TRUE : GL_FALSE;
			glColorMask(newColorWrite, newColorWrite, newColorWrite, newColorWrite);
			lastColorWrite = colorWrite;
		}

		if (depthWrite != lastDepthWrite) {
			GLboolean newDepthWrite = depthWrite ? GL_TRUE : GL_FALSE;
			glDepthMask(newDepthWrite);
			lastDepthWrite = depthWrite;
		}
	}

	void Graphics::SetDepthBias(float constantBias, float slopeScaleBias)
	{
		if (constantBias <= 0.0f && slopeScaleBias <= 0.0f) {
			if (lastDepthBias) {
				glDisable(GL_POLYGON_OFFSET_FILL);
				lastDepthBias = false;
			}
		} else {
			if (!lastDepthBias) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				lastDepthBias = true;
			}
			glPolygonOffset(slopeScaleBias, constantBias);
		}
	}

#if 0
	void Graphics::SetScissor(const IntRect& scissor)
	{
		 if (scissor == IntRect::ZERO) {
			 if (lastScissor) {
				glDisable(GL_SCISSOR_TEST);
				lastScissor = false;
			 }
		 } else {
			 if (!lastScissor) {
				 glEnable(GL_SCISSOR_TEST);
				 lastScissor = true;
			 }
			 glScissor(scissor.left, scissor.top, scissor.Width(), scissor.Height());
		 }
	}
#endif

	void Graphics::Clear(bool clearColor, bool clearDepth, const IntRect& clearRect, const Color& backgroundColor)
	{
		if (clearColor) {
			glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			lastColorWrite = true;
		}
		if (clearDepth) {
			glDepthMask(GL_TRUE);
			lastDepthWrite = true;
		}

		GLenum glClearBits = 0;
		if (clearColor) {
			glClearBits |= GL_COLOR_BUFFER_BIT;
		}
		if (clearDepth) {
			glClearBits |= GL_DEPTH_BUFFER_BIT;
		}

		if (clearRect == IntRect::ZERO) {
			glClear(glClearBits);
		} else {
			glEnable(GL_SCISSOR_TEST);
			glScissor(clearRect.left, clearRect.top, clearRect.right - clearRect.left, clearRect.bottom - clearRect.top);
			glClear(glClearBits);
			glDisable(GL_SCISSOR_TEST);
		}
	}

	void Graphics::Blit(FrameBuffer* dest, const IntRect& destRect, FrameBuffer* src, const IntRect& srcRect, bool blitColor, bool blitDepth, TextureFilterMode filter)
	{
		FrameBuffer::Bind(dest, src);

		GLenum glBlitBits = 0;
		if (blitColor) {
			glBlitBits |= GL_COLOR_BUFFER_BIT;
		}
		if (blitDepth) {
			glBlitBits |= GL_DEPTH_BUFFER_BIT;
		}

		glBlitFramebuffer(srcRect.left, srcRect.top, srcRect.right, srcRect.bottom, destRect.left, destRect.top, destRect.right, destRect.bottom, glBlitBits, filter == FILTER_POINT ? GL_NEAREST : GL_LINEAR);
	}

	void Graphics::Draw(PrimitiveType type, size_t drawStart, size_t drawCount)
	{
		if (instancingEnabled) {
			glDisableVertexAttribArray(ATTR_TEXCOORD3);
			glDisableVertexAttribArray(ATTR_TEXCOORD4);
			glDisableVertexAttribArray(ATTR_TEXCOORD5);
			instancingEnabled = false;
		}

		glDrawArrays(glPrimitiveTypes[type], (GLsizei)drawStart, (GLsizei)drawCount);
	}

	void Graphics::DrawIndexed(PrimitiveType type, size_t drawStart, size_t drawCount)
	{
		if (instancingEnabled) {
			glDisableVertexAttribArray(ATTR_TEXCOORD3);
			glDisableVertexAttribArray(ATTR_TEXCOORD4);
			glDisableVertexAttribArray(ATTR_TEXCOORD5);
			instancingEnabled = false;
		}

		unsigned indexSize = (unsigned)IndexBuffer::BoundIndexSize();
		if (indexSize) {
			glDrawElements(glPrimitiveTypes[type], (GLsizei)drawCount, indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(drawStart * indexSize));
		}
	}

	void Graphics::DrawInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount)
	{
		if (!hasInstancing || !instanceVertexBuffer) {
			return;
		}

		if (!instancingEnabled) {
			glEnableVertexAttribArray(ATTR_TEXCOORD3);
			glEnableVertexAttribArray(ATTR_TEXCOORD4);
			glEnableVertexAttribArray(ATTR_TEXCOORD5);
			instancingEnabled = true;
		}

		unsigned instanceVertexSize = (unsigned)instanceVertexBuffer->VertexSize();

		instanceVertexBuffer->Bind(0);
		glVertexAttribPointer(ATTR_TEXCOORD3, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize));
		glVertexAttribPointer(ATTR_TEXCOORD4, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + sizeof(Vector4)));
		glVertexAttribPointer(ATTR_TEXCOORD5, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + 2 * sizeof(Vector4)));
		glDrawArraysInstanced(glPrimitiveTypes[type], (GLint)drawStart, (GLsizei)drawCount, (GLsizei)instanceCount);
	}

	void Graphics::DrawIndexedInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount)
	{
		unsigned indexSize = (unsigned)IndexBuffer::BoundIndexSize();

		if (!hasInstancing || !instanceVertexBuffer || !indexSize) {
			return;
		}

		if (!instancingEnabled) {
			glEnableVertexAttribArray(ATTR_TEXCOORD3);
			glEnableVertexAttribArray(ATTR_TEXCOORD4);
			glEnableVertexAttribArray(ATTR_TEXCOORD5);
			instancingEnabled = true;
		}

		unsigned instanceVertexSize = (unsigned)instanceVertexBuffer->VertexSize();

		instanceVertexBuffer->Bind(0);
		glVertexAttribPointer(ATTR_TEXCOORD3, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize));
		glVertexAttribPointer(ATTR_TEXCOORD4, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + sizeof(Vector4)));
		glVertexAttribPointer(ATTR_TEXCOORD5, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + 2 * sizeof(Vector4)));
		glDrawElementsInstanced(glPrimitiveTypes[type], (GLsizei)drawCount, indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(drawStart * indexSize), (GLsizei)instanceCount);
	}

	void Graphics::DrawQuad()
	{
		quadVertexBuffer->Bind(MASK_POSITION | MASK_TEXCOORD);
		Draw(PT_TRIANGLE_LIST, 0, 6);
	}

	unsigned Graphics::BeginOcclusionQuery(void* object)
	{
		GLuint queryId;

		if (freeQueries.size()) {
			queryId = freeQueries.back();
			freeQueries.pop_back();
		} else {
			glGenQueries(1, &queryId);
		}

		glBeginQuery(occlusionQueryType, queryId);
		pendingQueries.push_back(std::make_pair(queryId, object));

		return queryId;
	}

	void Graphics::EndOcclusionQuery()
	{
		glEndQuery(occlusionQueryType);
	}

	void Graphics::FreeOcclusionQuery(unsigned queryId)
	{
		if (!queryId) {
			return;
		}

		for (auto it = pendingQueries.begin(); it != pendingQueries.end(); ++it) {
			if (it->first == queryId) {
				pendingQueries.erase(it);
				break;
			}
		}

		glDeleteQueries(1, &queryId);
	}

	void Graphics::CheckOcclusionQueryResults(std::vector<OcclusionQueryResult>& result, bool isHighFrameRate)
	{
		GLuint available = 0;

		if (!vsync && isHighFrameRate) {
			// Vsync off and high framerate: check for query result availability to avoid stalling.
			// To save API calls, go through queries in reverse order
			// and assume that if a later query has its result available, then all earlier queries will have too
			GLuint available = 0;

			for (size_t i = pendingQueries.size() - 1; i < pendingQueries.size(); --i) {
				GLuint queryId = pendingQueries[i].first;

				if (!available) {
					glGetQueryObjectuiv(queryId, GL_QUERY_RESULT_AVAILABLE, &available);
				}

				if (available) {
					GLuint passed = 0;
					glGetQueryObjectuiv(queryId, GL_QUERY_RESULT, &passed);

					OcclusionQueryResult newResult;
					newResult.id = queryId;
					newResult.object = pendingQueries[i].second;
					newResult.visible = passed > 0;
					result.push_back(newResult);

					freeQueries.push_back(queryId);
					pendingQueries.erase(pendingQueries.begin() + i);
				}
			}
		} else {
			// Vsync on or low frame rate: check all query results, potentially stalling, to avoid stutter and large false occlusion errors
			for (auto it = pendingQueries.begin(); it != pendingQueries.end(); ++it) {
				GLuint queryId = it->first;
				GLuint passed = 0;
				glGetQueryObjectuiv(queryId, GL_QUERY_RESULT, &passed);

				OcclusionQueryResult newResult;
				newResult.id = queryId;
				newResult.object = it->second;
				newResult.visible = passed > 0;
				result.push_back(newResult);

				freeQueries.push_back(queryId);
			}

			pendingQueries.clear();
		}
	}

	IntVector2 Graphics::Size() const
	{
		IntVector2 size;
		glfwGetWindowSize(window, &size.x, &size.y);
		return size;
	}

	IntVector2 Graphics::RenderSize() const
	{
		IntVector2 size;
		glfwGetFramebufferSize(window, &size.x, &size.y);
		return size;
	}

	bool Graphics::IsFullscreen() const
	{
		GLFWmonitor* monitor = glfwGetWindowMonitor(window);
		return (monitor != nullptr);
	}

	int Graphics::FullscreenRefreshRate() const
	{
		GLFWmonitor* monitor = glfwGetWindowMonitor(window);
		if (monitor) {
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			return mode->refreshRate;
		}
		return 0;
	}

	void Graphics::DefineQuadVertexBuffer()
	{
		float quadVertexData[] = {
			// Position         // UV
			-1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
			1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
			1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
			1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 1.0f
		};

		std::vector<VertexElement> vertexDeclaration;
		vertexDeclaration.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
		vertexDeclaration.push_back(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
		quadVertexBuffer = std::make_unique<VertexBuffer>();
		quadVertexBuffer->Define(USAGE_DEFAULT, 6, vertexDeclaration, quadVertexData);
	}
}
