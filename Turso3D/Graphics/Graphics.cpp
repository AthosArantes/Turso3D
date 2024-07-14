#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Utils/ShaderPermutation.h>
#include <glew/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <exception>

// Prefer the high-performance GPU on switchable GPU systems
extern "C" {
	__declspec(dllexport) int NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace
{
	using namespace Turso3D;

	static const unsigned glPrimitiveTypes[] = {
		GL_LINES,
		GL_TRIANGLES
	};

	static const unsigned glCompareFuncs[] = {
		GL_NEVER,
		GL_LESS,
		GL_EQUAL,
		GL_LEQUAL,
		GL_GREATER,
		GL_NOTEQUAL,
		GL_GEQUAL,
		GL_ALWAYS,
	};

	static const unsigned glSrcBlend[] = {
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

	static const unsigned glDestBlend[] = {
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

	static const unsigned glBlendOp[] = {
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

	static const unsigned glCullMode[] = {
		0,
		GL_FRONT,
		GL_BACK
	};

	static const unsigned glVertexElementSizes[] = {
		1,
		1,
		2,
		3,
		4,
		4
	};

	static const unsigned glVertexElementTypes[] = {
		GL_INT,
		GL_FLOAT,
		GL_FLOAT,
		GL_FLOAT,
		GL_FLOAT,
		GL_UNSIGNED_BYTE,
	};

	// Store Vertex Array Object state
	struct VAO
	{
		// A combined hash of the vertex and instance elements.
		size_t hash;
		// Opengl object.
		unsigned vao;

		// Vertex buffer for binding point 0.
		VertexBuffer* vertexBuffer[MAX_VERTEX_BINDING_POINTS];
		// Vertex buffer offset.
		size_t vertexStart[MAX_VERTEX_BINDING_POINTS];

		// Index buffer
		IndexBuffer* indexBuffer;
	};

	struct GraphicsState
	{
		// OS-level rendering window.
		GLFWwindow* window;
		// OpenGL context.
		void* context;

		// Vertical sync flag.
		bool vsync;
		// The window position before going full screen.
		IntVector2 lastWindowPos;
		// The window size before going full screen.
		IntVector2 lastWindowSize;

		FrameBuffer* boundDrawBuffer;
		FrameBuffer* boundReadBuffer;
		ShaderProgram* boundProgram;
		UniformBuffer* boundUniformBuffers[MAX_CONSTANT_BUFFER_SLOTS];

		unsigned activeTargets[MAX_TEXTURE_UNITS];
		Texture* boundTextures[MAX_TEXTURE_UNITS];
		size_t activeTextureUnit;

		// Last blend mode.
		BlendMode lastBlendMode;
		// Last cull mode.
		CullMode lastCullMode;
		// Last depth test.
		CompareMode lastDepthTest;
		// Last color write.
		bool lastColorWrite;
		// Last depth write.
		bool lastDepthWrite;
		// Last depth bias enabled.
		bool lastDepthBias;

		// Pending occlusion queries.
		std::vector<std::pair<unsigned, void*>> pendingQueries;
		// Free occlusion queries.
		std::vector<unsigned> freeQueries;

		// Quad vertex buffer.
		VertexBuffer quadVertexBuffer;

		// Default VAO.
		unsigned defaultVAO;
		// Currently bound VAO, nullptr if using the default.
		VAO* boundVAO;
		// Cache map for VAOs.
		std::vector<VAO> vaoCache;
	};

#ifdef _DEBUG
	const char* GLSeverity(GLenum severity)
	{
		switch (severity) {
			case GL_DEBUG_SEVERITY_NOTIFICATION: return "Notification";
			case GL_DEBUG_SEVERITY_HIGH: return "High";
			case GL_DEBUG_SEVERITY_MEDIUM: return "Medium";
			case GL_DEBUG_SEVERITY_LOW: return "Low";
		}
		return nullptr;
	}

	void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
	{
		if (id) {
			LOG_RAW("[GL DEBUG] [source: {:d}] [type: {:d}] [id: {:d}] [severity: {:d}] {:s}", source, type, id, severity, std::string(message, length));
		}
	}
#endif

	static GraphicsState State;
	static bool StateInitialized = false;
}

// ==========================================================================================

namespace Turso3D
{
#ifdef _DEBUG
	GraphicsMarker::GraphicsMarker(const char* name)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
	}

	GraphicsMarker::~GraphicsMarker()
	{
		glPopDebugGroup();
	}
#endif

	// ==========================================================================================
	bool Graphics::Initialize(const char* windowTitle, int width, int height)
	{
		if (StateInitialized) {
			return true;
		}

		if (!glfwInit()) {
			LOG_ERROR("Failed to initialize GLFW");
			return false;
		}

#ifdef _DEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

		glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		State.lastWindowPos = IntVector2::ZERO();
		State.lastWindowSize = IntVector2 {width, height};

		State.window = glfwCreateWindow(width, height, windowTitle, nullptr, nullptr);
		if (!State.window) {
			throw std::exception("Failed to create GLFW window.");
		}

		glfwMakeContextCurrent(State.window);
		State.context = glfwGetCurrentContext();
		if (!State.context) {
			LOG_ERROR("Could not create OpenGL context");
			return false;
		}

		GLenum err = glewInit();
		if (err != GLEW_OK || !GLEW_VERSION_3_3) {
			LOG_ERROR("Could not initialize OpenGL 3.3");
			return false;
		}

		GLint maxSizeUBO = 0;
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxSizeUBO);
		if (maxSizeUBO < 64000) {
			LOG_ERROR("Max size of uniform buffer object is less than 64kb.");
			return false;
		}

		if (!GLEW_VERSION_4_0 && !GLEW_ARB_texture_cube_map_array) {
			LOG_ERROR("ARB_texture_cube_map_array not supported.");
			return false;
		}
		if (!GLEW_VERSION_4_3 && !GLEW_ARB_vertex_attrib_binding) {
			LOG_ERROR("ARB_vertex_attrib_binding not supported.");
			return false;
		}

#ifdef _DEBUG
		glEnable(GL_DEBUG_OUTPUT);

		if (GLEW_KHR_debug) {
			LOG_DEBUG("KHR_debug extension found");
			GLDEBUGPROC p = &(GLDebugCallback);
			glDebugMessageCallback(p, nullptr);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			LOG_DEBUG("debug callback enabled.");
		} else if (GL_ARB_debug_output) {
			LOG_DEBUG("GL_ARB_debug_output extension found.");
			GLDEBUGPROC p = &(GLDebugCallback);
			glDebugMessageCallbackARB(p, nullptr);
			LOG_DEBUG("debug callback enabled.");
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		} else {
			LOG_DEBUG("KHR_debug extension NOT found.");
		}

		// Exclude "detailed info" messages
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0, nullptr, GL_FALSE);
#endif

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_FRAMEBUFFER_SRGB);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

		glClearDepth(1.0f);
		glDepthRange(0.0f, 1.0f);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glFrontFace(GL_CW); // Use Direct3D convention, ie. clockwise vertices define a front face

		// Vertex array objects
		{
			// Define default VAO
			glGenVertexArrays(1, &State.defaultVAO);
			glBindVertexArray(State.defaultVAO);

			unsigned max_attribs;
			glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, (GLint*)&max_attribs);
			for (unsigned i = 0; i < max_attribs; ++i) {
				glEnableVertexAttribArray(i);
			}

			State.vaoCache.reserve(16);
		}

		// Define quad vertex buffer
		{
			const float quadVertexData[] = {
				// Position         // UV
				-1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
				1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
				-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
				1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
				1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
				-1.0f, -1.0f, 0.0f, 0.0f, 1.0f
			};
			const VertexElement elements[] = {
				{ELEM_VECTOR3, ATTR_POSITION},
				{ELEM_VECTOR2, ATTR_TEXCOORD}
			};
			State.quadVertexBuffer.Define(USAGE_DEFAULT, 6, elements, 2, quadVertexData);
		}
		SetVSync(false);

		StateInitialized = true;
		return true;
	}

	void Graphics::ShutDown()
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &State.defaultVAO);
		for (size_t i = 0; i < State.vaoCache.size(); ++i) {
			glDeleteVertexArrays(1, &State.vaoCache[i].vao);
		}
		State.vaoCache.clear();

		if (State.context) {
			State.context = nullptr;
		}

		if (State.window) {
			glfwDestroyWindow(State.window);
			State.window = nullptr;
		}

		glfwTerminate();
		StateInitialized = false;
	}

	bool Graphics::IsInitialized()
	{
		return StateInitialized;
	}

	void* Graphics::Window()
	{
		return State.window;
	}

	void Graphics::Resize(int width, int height)
	{
		glfwSetWindowSize(State.window, width, height);
	}

	IntVector2 Graphics::Size()
	{
		IntVector2 size;
		glfwGetWindowSize(State.window, &size.x, &size.y);
		return size;
	}

	IntVector2 Graphics::RenderSize()
	{
		IntVector2 size;
		glfwGetFramebufferSize(State.window, &size.x, &size.y);
		return size;
	}

	void Graphics::SetFullscreen(bool enable)
	{
		GLFWmonitor* monitor = glfwGetWindowMonitor(State.window);
		if (enable) {
			if (monitor) {
				return; // Already fullscreen mode
			}
			monitor = glfwGetPrimaryMonitor();

			glfwGetWindowPos(State.window, &State.lastWindowPos.x, &State.lastWindowPos.y);
			glfwGetWindowSize(State.window, &State.lastWindowSize.x, &State.lastWindowSize.y);

			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(State.window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

		} else {
			if (!monitor) {
				return; // Already windowed mode
			}
			glfwSetWindowMonitor(State.window, nullptr, State.lastWindowPos.x, State.lastWindowPos.y, State.lastWindowSize.x, State.lastWindowSize.y, GLFW_DONT_CARE);
		}
	}

	bool Graphics::IsFullscreen()
	{
		GLFWmonitor* monitor = glfwGetWindowMonitor(State.window);
		return (monitor != nullptr);
	}

	void Graphics::SetVSync(bool enable)
	{
		if (IsInitialized()) {
			glfwSwapInterval(enable ? 1 : 0);
			State.vsync = enable;
		}
	}

	bool Graphics::VSync()
	{
		return State.vsync;
	}

	void Graphics::SetViewport(const IntRect& viewRect)
	{
		glViewport(viewRect.left, viewRect.top, viewRect.right - viewRect.left, viewRect.bottom - viewRect.top);
	}

	std::shared_ptr<ShaderProgram> Graphics::CreateProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines)
	{
		ResourceCache* cache = ResourceCache::Instance();
		std::shared_ptr<Shader> shader = cache->LoadResource<Shader>(shaderName);
		if (shader) {
			return shader->Program(ShaderPermutation {vsDefines}, ShaderPermutation {fsDefines});
		}
		return {};
	}

	void Graphics::SetUniform(int location, float value)
	{
		glUniform1f(location, value);
	}

	void Graphics::SetUniform(int location, const Vector2& value)
	{
		glUniform2fv(location, 1, value.Data());
	}

	void Graphics::SetUniform(int location, const Vector3& value)
	{
		glUniform3fv(location, 1, value.Data());
	}

	void Graphics::SetUniform(int location, const Vector4& value)
	{
		glUniform4fv(location, 1, value.Data());
	}

	void Graphics::SetUniform(int location, const Matrix3& value)
	{
		glUniformMatrix3fv(location, 1, GL_FALSE, value.Data());
	}

	void Graphics::SetUniform(int location, const Matrix3x4& value)
	{
		glUniformMatrix3x4fv(location, 1, GL_FALSE, value.Data());
	}

	void Graphics::SetUniform(int location, const Matrix4& value)
	{
		glUniformMatrix4fv(location, 1, GL_FALSE, value.Data());
	}

	void Graphics::SetRenderState(BlendMode blendMode, CullMode cullMode, CompareMode depthTest, bool colorWrite, bool depthWrite)
	{
		if (blendMode != State.lastBlendMode) {
			if (blendMode == BLEND_REPLACE) {
				glDisable(GL_BLEND);
			} else {
				if (State.lastBlendMode == BLEND_REPLACE) {
					glEnable(GL_BLEND);
				}
				glBlendFunc(glSrcBlend[blendMode], glDestBlend[blendMode]);
				glBlendEquation(glBlendOp[blendMode]);
			}
			State.lastBlendMode = blendMode;
		}

		if (cullMode != State.lastCullMode) {
			if (cullMode == CULL_NONE) {
				glDisable(GL_CULL_FACE);
			} else {
				if (State.lastCullMode == CULL_NONE) {
					glEnable(GL_CULL_FACE);
				}
				glCullFace(glCullMode[cullMode]);
			}
			State.lastCullMode = cullMode;
		}

		if (depthTest != State.lastDepthTest) {
			glDepthFunc(glCompareFuncs[depthTest]);
			State.lastDepthTest = depthTest;
		}

		if (colorWrite != State.lastColorWrite) {
			GLboolean newColorWrite = colorWrite ? GL_TRUE : GL_FALSE;
			glColorMask(newColorWrite, newColorWrite, newColorWrite, newColorWrite);
			State.lastColorWrite = colorWrite;
		}

		if (depthWrite != State.lastDepthWrite) {
			GLboolean newDepthWrite = depthWrite ? GL_TRUE : GL_FALSE;
			glDepthMask(newDepthWrite);
			State.lastDepthWrite = depthWrite;
		}
	}

	void Graphics::SetDepthBias(float constantBias, float slopeScaleBias)
	{
		if (constantBias <= 0.0f && slopeScaleBias <= 0.0f) {
			if (State.lastDepthBias) {
				glDisable(GL_POLYGON_OFFSET_FILL);
				State.lastDepthBias = false;
			}
		} else {
			if (!State.lastDepthBias) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				State.lastDepthBias = true;
			}
			glPolygonOffset(slopeScaleBias, constantBias);
		}
	}

#if 0
	void Graphics::SetScissor(const IntRect& scissor)
	{
		if (scissor == IntRect::ZERO) {
			if (State.lastScissor) {
				glDisable(GL_SCISSOR_TEST);
				State.lastScissor = false;
			}
		} else {
			if (!State.lastScissor) {
				glEnable(GL_SCISSOR_TEST);
				State.lastScissor = true;
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
			State.lastColorWrite = true;
		}
		if (clearDepth) {
			glDepthMask(GL_TRUE);
			State.lastDepthWrite = true;
		}

		GLenum glClearBits = 0;
		if (clearColor) {
			glClearBits |= GL_COLOR_BUFFER_BIT;
		}
		if (clearDepth) {
			glClearBits |= GL_DEPTH_BUFFER_BIT;
		}

		if (clearRect == IntRect::ZERO()) {
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
		BindFramebuffer(dest, src);

		GLenum glBlitBits = 0;
		if (blitColor) {
			glBlitBits |= GL_COLOR_BUFFER_BIT;
		}
		if (blitDepth) {
			glBlitBits |= GL_DEPTH_BUFFER_BIT;
		}

		glBlitFramebuffer(srcRect.left, srcRect.top, srcRect.right, srcRect.bottom, destRect.left, destRect.top, destRect.right, destRect.bottom, glBlitBits, filter == FILTER_POINT ? GL_NEAREST : GL_LINEAR);
	}

	void Graphics::BindVertexBuffers(const VertexBufferBinding* bindings, size_t numBindings)
	{
		assert(bindings && numBindings && numBindings <= MAX_VERTEX_BINDING_POINTS);

		size_t hash = 0;
		for (size_t i = 0; i < numBindings; ++i) {
			const VertexBufferBinding& binding = bindings[i];
			if (binding.enabled) {
				assert(binding.buffer);
				hash = (hash << 24 | hash >> 40 & 0xFFFFFFFFFFu) ^ binding.buffer->ElementsHash();
			}
		}

		VAO* vao = State.boundVAO;
		if (!vao || vao->hash != hash) {
			auto it = std::find_if(State.vaoCache.begin(), State.vaoCache.end(), [hash](const VAO& v)
			{
				return v.hash == hash;
			});

			if (it == State.vaoCache.end()) {
				VAO& v = State.vaoCache.emplace_back();
				vao = &v;
				vao->hash = hash;

				glGenVertexArrays(1, &vao->vao);
				if (!vao->vao) {
					LOG_ERROR("Failed to create VAO.");
					return;
				}

				glBindVertexArray(vao->vao);
				State.boundVAO = vao;

				// Vertex binding index
				unsigned index = 0;
				for (size_t i = 0; i < numBindings; ++i) {
					const VertexBufferBinding& binding = bindings[i];
					if (!binding.enabled) {
						continue;
					}

					for (size_t e = 0; e < binding.buffer->NumElements(); ++e) {
						const VertexElement& element = binding.buffer->GetElement(e);
						glEnableVertexAttribArray(element.index);
						glVertexAttribFormat(
							element.index,
							glVertexElementSizes[element.type],
							glVertexElementTypes[element.type],
							(GLboolean)element.normalized,
							binding.buffer->GetElementOffset(e)
						);
						glVertexAttribBinding(element.index, index);
					}
					glVertexBindingDivisor(index, binding.divisor);

					++index;
				}

				for (unsigned i = 0; i < MAX_VERTEX_BINDING_POINTS; ++i) {
					vao->vertexBuffer[i] = nullptr;
					vao->vertexStart[i] = 0;
				}
			} else {
				vao = &*it;
				glBindVertexArray(vao->vao);
				State.boundVAO = vao;
			}
		}

		unsigned index = 0;
		for (size_t i = 0; i < numBindings; ++i) {
			const VertexBufferBinding& binding = bindings[i];
			if (!binding.enabled) {
				continue;
			}
			if (binding.buffer != vao->vertexBuffer[index] || binding.start != vao->vertexStart[index]) {
				glBindVertexBuffer(index, binding.buffer->GLBuffer(), binding.start * binding.buffer->VertexSize(), binding.buffer->VertexSize());
				vao->vertexBuffer[index] = binding.buffer;
				vao->vertexStart[index] = binding.start;
			}
			++index;
		}
	}

	void Graphics::BindVertexBuffers(VertexBuffer* buffer)
	{
		assert(buffer);
		VertexBufferBinding binding {buffer};
		BindVertexBuffers(&binding, 1);
	}

	void Graphics::UnbindVertexBuffers()
	{
		if (State.boundVAO) {
			glBindVertexArray(State.defaultVAO);
			State.boundVAO = nullptr;
		}
	}

	void Graphics::BindIndexBuffer(IndexBuffer* buffer)
	{
		if (State.boundVAO) {
			if (buffer != State.boundVAO->indexBuffer) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->GLBuffer());
				State.boundVAO->indexBuffer = buffer;
			}
		} else {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->GLBuffer());
		}
	}

	void Graphics::BindFramebuffer(FrameBuffer* draw, FrameBuffer* read)
	{
		if (draw != State.boundDrawBuffer) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw ? draw->GLBuffer() : 0);
			State.boundDrawBuffer = draw;
		}
		if (read != State.boundReadBuffer) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, read ? read->GLBuffer() : 0);
			State.boundReadBuffer = read;
		}
	}

	void Graphics::UnbindFramebuffer(FrameBuffer* buffer)
	{
		if (State.boundDrawBuffer == buffer) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			State.boundDrawBuffer = nullptr;
		}
		if (State.boundReadBuffer == buffer) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			State.boundReadBuffer = nullptr;
		}
	}

	void Graphics::BindProgram(ShaderProgram* program)
	{
		if (program != State.boundProgram) {
			if (program) {
				glUseProgram(program->GLProgram());
			}
			State.boundProgram = program;
		}
	}

	void Graphics::BindUniformBuffer(size_t index, UniformBuffer* buffer)
	{
		if (buffer != State.boundUniformBuffers[index]) {
			glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)index, buffer ? buffer->GLBuffer() : 0, 0, buffer ? buffer->Size() : 0);
			State.boundUniformBuffers[index] = buffer;
		}
	}

	void Graphics::BindTexture(size_t unit, Texture* texture, bool force)
	{
		assert(unit < MAX_TEXTURE_UNITS);

		if (!force && State.boundTextures[unit] == texture) {
			return;
		}

		if (State.activeTextureUnit != unit) {
			glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
			State.activeTextureUnit = unit;
		}

		unsigned& activeTarget = State.activeTargets[unit];
		if (texture) {
			unsigned target = texture->GLTarget();
			assert(target);

			if (activeTarget && activeTarget != target) {
				glBindTexture(activeTarget, 0);
			}

			glBindTexture(target, texture->GLTexture());
			activeTarget = target;

		} else if (activeTarget) {
			glBindTexture(activeTarget, 0);
			activeTarget = 0;
		}

		State.boundTextures[unit] = texture;
	}

	void Graphics::RemoveStateObject(VertexBuffer* buffer)
	{
		if (!buffer) {
			return;
		}
		for (size_t i = 0; i < State.vaoCache.size(); ++i) {
			VAO& vao = State.vaoCache[i];
			for (size_t j = 0; j < MAX_VERTEX_BINDING_POINTS; ++j) {
				if (vao.vertexBuffer[j] == buffer) {
					vao.vertexBuffer[j] = nullptr;
					vao.vertexStart[j] = 0;
				}
			}
		}
	}

	void Graphics::RemoveStateObject(IndexBuffer* buffer)
	{
		if (!buffer) {
			return;
		}
		for (size_t i = 0; i < State.vaoCache.size(); ++i) {
			VAO& vao = State.vaoCache[i];
			if (vao.indexBuffer == buffer) {
				vao.indexBuffer = nullptr;
			}
		}
	}

	void Graphics::RemoveStateObject(UniformBuffer* buffer)
	{
		if (!buffer) {
			return;
		}
		for (size_t i = 0; i < MAX_CONSTANT_BUFFER_SLOTS; ++i) {
			if (State.boundUniformBuffers[i] == buffer) {
				State.boundUniformBuffers[i] = nullptr;
			}
		}
	}

	void Graphics::RemoveStateObject(Texture* texture)
	{
		if (!texture) {
			return;
		}
		for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i) {
			if (State.boundTextures[i] == texture) {
				State.boundTextures[i] = nullptr;
			}
		}
	}

	void Graphics::Draw(PrimitiveType type, size_t drawStart, size_t drawCount)
	{
		glDrawArrays(glPrimitiveTypes[type], (GLint)drawStart, (GLsizei)drawCount);
	}

	void Graphics::DrawIndexed(PrimitiveType type, size_t drawStart, size_t drawCount)
	{
		size_t index_size = State.boundVAO->indexBuffer->IndexSize();
		GLenum index_type = (index_size == sizeof(unsigned short)) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElements(glPrimitiveTypes[type], (GLsizei)drawCount, index_type, (const void*)(drawStart * index_size));
	}

	void Graphics::DrawInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, size_t instanceCount)
	{
		glDrawArraysInstanced(glPrimitiveTypes[type], (GLint)drawStart, (GLsizei)drawCount, (GLsizei)instanceCount);
	}

	void Graphics::DrawIndexedInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, size_t instanceCount)
	{
		size_t index_size = State.boundVAO->indexBuffer->IndexSize();
		GLenum index_type = (index_size == sizeof(unsigned short)) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstanced(glPrimitiveTypes[type], (GLsizei)drawCount, index_type, (const void*)(drawStart * index_size), (GLsizei)instanceCount);
	}

	void Graphics::DrawQuad()
	{
		BindVertexBuffers(&State.quadVertexBuffer);
		Draw(PT_TRIANGLE_LIST, 0, 6);
	}

	void Graphics::Present()
	{
		glfwSwapBuffers(State.window);
	}

	unsigned Graphics::BeginOcclusionQuery(void* object)
	{
		GLuint queryId;

		if (State.freeQueries.size()) {
			queryId = State.freeQueries.back();
			State.freeQueries.pop_back();
		} else {
			glGenQueries(1, &queryId);
		}

		glBeginQuery(GL_ANY_SAMPLES_PASSED, queryId);
		State.pendingQueries.push_back(std::make_pair(queryId, object));

		return queryId;
	}

	void Graphics::EndOcclusionQuery()
	{
		glEndQuery(GL_ANY_SAMPLES_PASSED);
	}

	void Graphics::FreeOcclusionQuery(unsigned queryId)
	{
		if (!queryId) {
			return;
		}

		for (auto it = State.pendingQueries.begin(); it != State.pendingQueries.end(); ++it) {
			if (it->first == queryId) {
				State.pendingQueries.erase(it);
				break;
			}
		}

		glDeleteQueries(1, &queryId);
	}

	void Graphics::CheckOcclusionQueryResults(std::vector<OcclusionQueryResult>& result, bool isHighFrameRate)
	{
		GLuint available = 0;

		if (!State.vsync && isHighFrameRate) {
			// Vsync off and high framerate: check for query result availability to avoid stalling.
			// To save API calls, go through queries in reverse order
			// and assume that if a later query has its result available, then all earlier queries will have too
			GLuint available = 0;

			for (size_t i = State.pendingQueries.size() - 1; i < State.pendingQueries.size(); --i) {
				GLuint queryId = State.pendingQueries[i].first;

				if (!available) {
					glGetQueryObjectuiv(queryId, GL_QUERY_RESULT_AVAILABLE, &available);
				}

				if (available) {
					GLuint passed = 0;
					glGetQueryObjectuiv(queryId, GL_QUERY_RESULT, &passed);

					OcclusionQueryResult newResult;
					newResult.id = queryId;
					newResult.object = State.pendingQueries[i].second;
					newResult.visible = passed > 0;
					result.push_back(newResult);

					State.freeQueries.push_back(queryId);
					State.pendingQueries.erase(State.pendingQueries.begin() + i);
				}
			}
		} else {
			// Vsync on or low frame rate: check all query results, potentially stalling, to avoid stutter and large false occlusion errors
			for (auto it = State.pendingQueries.begin(); it != State.pendingQueries.end(); ++it) {
				GLuint queryId = it->first;
				GLuint passed = 0;
				glGetQueryObjectuiv(queryId, GL_QUERY_RESULT, &passed);

				OcclusionQueryResult newResult;
				newResult.id = queryId;
				newResult.object = it->second;
				newResult.visible = passed > 0;
				result.push_back(newResult);

				State.freeQueries.push_back(queryId);
			}

			State.pendingQueries.clear();
		}
	}

	size_t Graphics::PendingOcclusionQueries()
	{
		return State.pendingQueries.size();
	}

	int Graphics::FullscreenRefreshRate()
	{
		GLFWmonitor* monitor = glfwGetWindowMonitor(State.window);
		if (monitor) {
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			return mode->refreshRate;
		}
		return 0;
	}
}
