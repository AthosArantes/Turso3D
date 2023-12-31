#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Math/IntRect.h>

namespace Turso3D
{
	class RenderBuffer;
	class Texture;

	// GPU framebuffer object for rendering.
	// Combines color and depth-stencil textures or buffers.
	class FrameBuffer
	{
	public:
		// Construct.
		FrameBuffer();
		// Destruct.
		~FrameBuffer();

		// Define renderbuffers to render to.
		// Leave buffer(s) null for color-only or depth-only rendering.
		void Define(RenderBuffer* colorBuffer, RenderBuffer* depthStencilBuffer);
		// Define textures to render to.
		// Leave texture(s) null for color-only or depth-only rendering.
		void Define(Texture* colorTexture, Texture* depthStencilTexture);
		// Define cube map face to render to.
		void Define(Texture* colorTexture, size_t cubeMapFace, Texture* depthStencilTexture);
		// Define MRT textures to render to.
		void Define(Texture** colorTextures, size_t countColorTextures, Texture* depthStencilTexture);
		
		// Bind as draw framebuffer.
		// No-op if already bound.
		// Used also when defining.
		void Bind();

		// Return the OpenGL object identifier.
		unsigned GLBuffer() const { return buffer; }

		// Bind separate framebuffers for drawing and reading.
		static void Bind(FrameBuffer* draw, FrameBuffer* read);
		// Unbind the current draw and read framebuffers and return to backbuffer rendering.
		static void Unbind();

	private:
		// Create a framebuffer object.
		bool Create();
		// Release the framebuffer object.
		void Release();

		// OpenGL buffer object identifier.
		unsigned buffer;
	};
}
