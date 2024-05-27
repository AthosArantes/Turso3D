#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/VertexArrayObject.h>
#include <Turso3D/Math/Color.h>
#include <Turso3D/Math/IntRect.h>
#include <Turso3D/Math/Matrix3x4.h>
#include <vector>
#include <string>
#include <memory>

struct GLFWwindow;

namespace Turso3D
{
	class FrameBuffer;
	class IndexBuffer;
	class ShaderProgram;
	class Texture;
	class UniformBuffer;
	class VertexBuffer;

	// Occlusion query result.
	struct OcclusionQueryResult
	{
		// Query ID.
		unsigned id;
		// Associated object.
		void* object;
		// Visibility result.
		bool visible;
	};

	// Graphics rendering context and application window.
	class Graphics
	{
	public:
		class Marker
		{
		public:
			Marker(const char* name);
			~Marker();
		};

	public:
		// Construct.
		Graphics();
		// Destruct. Closes the application window.
		~Graphics();

		// Create window and rendering context.
		// Return true on success.
		bool Initialize(const char* windowTitle, int width, int height);

		// Set new window size.
		void Resize(int width, int height);
		// Set fullscreen mode.
		void SetFullscreen(bool enable);
		// Set vertical sync on/off.
		void SetVSync(bool enable);
		// Present the contents of the backbuffer.
		void Present();

		// Bind a framebuffer for rendering.
		// Null buffer parameter to unbind and return to backbuffer rendering.
		// Provided for convenience.
		void SetFrameBuffer(FrameBuffer* buffer);
		// Set the viewport rectangle.
		void SetViewport(const IntRect& viewRect);

		// Bind a shader program for use.
		// Return pointer on success or null otherwise.
		// Low performance, provided for convenience.
		ShaderProgram* SetProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines);
		// Create a shader program, but do not bind immediately.
		// Return pointer on success or null otherwise.
		std::shared_ptr<ShaderProgram> CreateProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines);

		// Set a float uniform.
		void SetUniform(int location, float value);
		// Set a Vector2 uniform.
		void SetUniform(int location, const Vector2& value);
		// Set a Vector3 uniform.
		void SetUniform(int location, const Vector3& value);
		// Set a Vector4 uniform.
		void SetUniform(int location, const Vector4& value);
		// Set a Matrix3 uniform.
		void SetUniform(int location, const Matrix3& value);
		// Set a Matrix3x4 uniform.
		void SetUniform(int location, const Matrix3x4& value);
		// Set a Matrix4 uniform.
		void SetUniform(int location, const Matrix4& value);

		// Bind a uniform buffer for use in slot index.
		// Null buffer parameter to unbind.
		// Provided for convenience.
		void SetUniformBuffer(size_t index, UniformBuffer* buffer);

		// Bind a texture for use in texture unit.
		// Null texture parameter to unbind.
		// Provided for convenience.
		void SetTexture(size_t index, Texture* texture);
		// Bind a vertex buffer for use with the specified shader program's attribute bindings.
		// Provided for convenience.
		void SetVertexBuffer(VertexBuffer* buffer, ShaderProgram* program);
		// Bind an index buffer for use.
		// Provided for convenience.
		void SetIndexBuffer(IndexBuffer* buffer);

		// Set basic renderstates.
		void SetRenderState(BlendMode blendMode, CullMode cullMode = CULL_BACK, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
		// Set depth bias.
		void SetDepthBias(float constantBias = 0.0f, float slopeScaleBias = 0.0f);
		// Clear the current framebuffer.
		void Clear(bool clearColor = true, bool clearDepth = true, const IntRect& clearRect = IntRect::ZERO(), const Color& backgroundColor = Color::BLACK());

		// Blit from one framebuffer to another.
		// The destination framebuffer will be left bound for rendering.
		void Blit(FrameBuffer* dest, const IntRect& destRect, FrameBuffer* src, const IntRect& srcRect, bool blitColor, bool blitDepth, TextureFilterMode filter);

		// Draw non-indexed geometry with the currently bound vertex buffer.
		void Draw(PrimitiveType type, size_t drawStart, size_t drawCount);
		// Draw indexed geometry with the currently bound vertex and index buffer.
		void DrawIndexed(PrimitiveType type, size_t drawStart, size_t drawCount);
		// Draw instanced non-indexed geometry with the currently bound vertex and index buffer, and the specified instance data vertex buffer.
		void DrawInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount);
		// Draw instanced indexed geometry with the currently bound vertex and index buffer, and the specified instance data vertex buffer.
		void DrawIndexedInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount);
		// Draw a quad with current renderstate.
		// The quad vertex buffer is left bound.
		void DrawQuad();

		// Begin an occlusion query and associate an object with it for checking results.
		// Return the query ID.
		unsigned BeginOcclusionQuery(void* object);
		// End an occlusion query.
		void EndOcclusionQuery();
		// Free an occlusion query when its associated object is destroyed early.
		void FreeOcclusionQuery(unsigned id);
		// Check for and return arrived query results.
		// These are only retained for one frame.
		// Should be called on the next frame after rendering queries, ie. after Present().
		void CheckOcclusionQueryResults(std::vector<OcclusionQueryResult>& result, bool isHighFrameRate);
		// Return number of pending occlusion queries.
		size_t PendingOcclusionQueries() const { return pendingQueries.size(); }

		// Return whether is initialized.
		bool IsInitialized() const { return context != nullptr; }
		// Return whether has instancing support.
		bool HasInstancing() const { return hasInstancing; }

		// Return current window size.
		IntVector2 Size() const;
		// Return current window width.
		int Width() const { return Size().x; }
		// Return current window height.
		int Height() const { return Size().y; }

		// Return window render size, which can be different if the OS is doing resolution scaling.
		IntVector2 RenderSize() const;
		// Return window render width.
		int RenderWidth() const { return RenderSize().x; }
		// Return window render height.
		int RenderHeight() const { return RenderSize().y; }

		// Return whether is fullscreen.
		bool IsFullscreen() const;
		// Return whether is using vertical sync.
		bool VSync() const { return vsync; }
		// Return the fullscreen refresh rate.
		// Return 0 if windowed mode.
		int FullscreenRefreshRate() const;
		// Return the OS-level window.
		GLFWwindow* Window() const { return window; }

		// Return the default VAO.
		VertexArrayObject& DefaultVao() { return defaultVAO; }

	private:
		// Set up the vertex buffer for quad rendering.
		void DefineQuadVertexBuffer();

	private:
		// OS-level rendering window.
		GLFWwindow* window;
		// OpenGL context.
		void* context;
		// Quad vertex buffer.
		std::unique_ptr<VertexBuffer> quadVertexBuffer;
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
		// Vertical sync flag.
		bool vsync;
		// Instancing support flag.
		bool hasInstancing;
		// Whether instance vertex elements are enabled.
		bool instancingEnabled;
		// Pending occlusion queries.
		std::vector<std::pair<unsigned, void*>> pendingQueries;
		// Free occlusion queries.
		std::vector<unsigned> freeQueries;

		VertexArrayObject defaultVAO;

		// The window position before going full screen.
		IntVector2 lastWindowPos;
		// The window size before going full screen.
		IntVector2 lastWindowSize;
	};
}

#ifdef _DEBUG
#define TURSO3D_GL_MARKER(name) Turso3D::Graphics::Marker glscope__ {name}
#else
#define TURSO3D_GL_MARKER(name)
#endif
