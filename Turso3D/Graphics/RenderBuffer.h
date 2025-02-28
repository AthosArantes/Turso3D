#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Math/IntVector2.h>

namespace Turso3D
{
	// GPU renderbuffer object for rendering and blitting, that cannot be sampled as a texture.
	class RenderBuffer
	{
	public:
		// Construct.
		RenderBuffer();
		// Destruct.
		~RenderBuffer();

		// Define renderbuffer type and dimensions.
		bool Define(const IntVector2& size, ImageFormat format, int multisample = 1);

		// Return dimensions.
		const IntVector2& Size() const { return size; }
		// Return the buffer format.
		ImageFormat Format() const { return format; }
		// Return multisampling level, or 1 if not multisampled.
		int Multisample() const { return multisample; }

		// Return the OpenGL buffer identifier.
		unsigned GLBuffer() const { return buffer; }

	private:
		// Release the renderbuffer.
		void Release();

	private:
		// OpenGL object identifier.
		unsigned buffer;
		// Texture dimensions in pixels.
		IntVector2 size;
		// Image format.
		ImageFormat format;
		// Multisampling level.
		int multisample;
	};
}
