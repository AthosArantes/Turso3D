#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>

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
		// Define renderbuffers to render to.
		// Leave buffer(s) null for color-only or depth-only rendering.
		void Define(RenderBuffer* const* colorBuffer, size_t countBuffers, RenderBuffer* depthStencilBuffer);

		// Define textures to render to.
		// Leave texture(s) null for color-only or depth-only rendering.
		void Define(Texture* colorTexture, Texture* depthStencilTexture);
		// Define cube map face to render to.
		void Define(Texture* colorTexture, size_t cubeMapFace, Texture* depthStencilTexture);
		// Define MRT textures to render to.
		void Define(Texture* const* colorTextures, size_t countColorTextures, Texture* depthStencilTexture);

		// Return the OpenGL object identifier.
		unsigned GLBuffer() const { return buffer; }

	private:
		// Create a framebuffer object.
		bool Create();
		// Release the framebuffer object.
		void Release();

		// OpenGL buffer object identifier.
		unsigned buffer;
	};
}
