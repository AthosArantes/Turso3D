#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Math/Color.h>
#include <Turso3D/Math/IntRect.h>
#include <Turso3D/Math/Matrix3x4.h>
#include <vector>
#include <string>
#include <memory>

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

	struct VertexBufferBinding
	{
		VertexBufferBinding(VertexBuffer* buffer, size_t start = 0, unsigned divisor = 0, bool enabled = true) :
			buffer(buffer),
			start(start),
			divisor(divisor),
			enabled(enabled)
		{
		}

		// The vertex buffer object.
		VertexBuffer* buffer;
		// The starting vertex position in the buffer.
		size_t start;
		// The buffer divisor.
		unsigned divisor;
		// Sets whether this buffer is used.
		bool enabled;
	};

#ifdef _DEBUG
	class GraphicsMarker
	{
	public:
		GraphicsMarker(const char* name);
		~GraphicsMarker();
	};
#endif

	// Graphics rendering context and application window.
	namespace Graphics
	{
		// Create window and rendering context.
		// Return true on success.
		bool Initialize(const char* windowTitle, int width, int height);
		// Delete window and rendering context.
		void ShutDown();

		// Return whether is initialized.
		bool IsInitialized();

		// Return the OS-level window.
		void* Window();
		// Set new window size.
		void Resize(int width, int height);
		// Return current window size.
		IntVector2 Size();
		// Return window render size, which can be different if the OS is doing resolution scaling.
		IntVector2 RenderSize();
		// Set fullscreen mode.
		void SetFullscreen(bool enable);
		// Return whether is fullscreen.
		bool IsFullscreen();
		// Set vertical sync on/off.
		void SetVSync(bool enable);
		// Return whether is using vertical sync.
		bool VSync();

		// Return the fullscreen refresh rate.
		// Return 0 if windowed mode.
		int FullscreenRefreshRate();

		// Set the viewport rectangle.
		void SetViewport(const IntRect& viewRect);

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

		// Set basic renderstates.
		void SetRenderState(BlendMode blendMode, CullMode cullMode = CULL_BACK, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
		// Set depth bias.
		void SetDepthBias(float constantBias = 0.0f, float slopeScaleBias = 0.0f);
		// Clear the current framebuffer.
		void Clear(bool clearColor = true, bool clearDepth = true, const IntRect& clearRect = IntRect::ZERO(), const Color& backgroundColor = Color::BLACK());

		// Blit from one framebuffer to another.
		// The destination framebuffer will be left bound for rendering.
		void Blit(FrameBuffer* dest, const IntRect& destRect, FrameBuffer* src, const IntRect& srcRect, bool blitColor, bool blitDepth, TextureFilterMode filter);

		// Bind the vertex buffers.
		// numBindings must not exceed max vertex binding points.
		void BindVertexBuffers(const VertexBufferBinding* bindings, size_t numBindings);
		// Bind a single vertex buffer.
		void BindVertexBuffers(VertexBuffer* buffer);
		// Unbind vertex buffers.
		void UnbindVertexBuffers();
		// Bind the index buffer.
		void BindIndexBuffer(IndexBuffer* buffer);

		// Bind separate framebuffers for drawing and reading.
		void BindFramebuffer(FrameBuffer* draw, FrameBuffer* read);
		// Unbind the specified framebuffer if it's bound, and return to backbuffer rendering.
		void UnbindFramebuffer(FrameBuffer* buffer);

		// Bind the shader program.
		void BindProgram(ShaderProgram* program);
		// Bind the uniform buffer.
		// If buffer is nullptr, the buffer slot is unbound.
		void BindUniformBuffer(size_t index, UniformBuffer* buffer = nullptr);
		// Bind to texture unit.
		// No-op if already bound (unless force is true).
		// If texture is nullptr, the texture unit is unbound.
		void BindTexture(size_t unit, Texture* texture = nullptr, bool force = false);

		// Remove the vertex buffer from the current state, allowing a rebind.
		void RemoveStateObject(VertexBuffer* buffer);
		// Remove the index buffer from the current state, allowing a rebind.
		void RemoveStateObject(IndexBuffer* buffer);
		// Remove the uniform buffer from the current state, allowing a rebind.
		void RemoveStateObject(UniformBuffer* buffer);
		// Remove the texture from the current state, allowing a rebind.
		void RemoveStateObject(Texture* texture);

		// Draw non-indexed geometry with the currently bound vertex buffer.
		void Draw(PrimitiveType type, size_t drawStart, size_t drawCount);
		// Draw indexed geometry with the currently bound vertex and index buffer.
		void DrawIndexed(PrimitiveType type, size_t drawStart, size_t drawCount);
		// Draw instanced non-indexed geometry with the currently bound vertex.
		void DrawInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, size_t instanceCount);
		// Draw instanced indexed geometry with the currently bound vertex and index buffer.
		void DrawIndexedInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, size_t instanceCount);
		// Draw a quad with current renderstate.
		// The quad vertex buffer is left bound.
		void DrawQuad();

		// Present the contents of the backbuffer.
		void Present();

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
		size_t PendingOcclusionQueries();
	};
}

#ifdef _DEBUG
#define TURSO3D_GRAPHICS_MARKER(name) Turso3D::GraphicsMarker gl_scope__ {name}
#else
#define TURSO3D_GRAPHICS_MARKER(name)
#endif
