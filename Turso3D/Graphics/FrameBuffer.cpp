#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/RenderBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cassert>

namespace Turso3D
{
	FrameBuffer::FrameBuffer() :
		buffer(0)
	{
	}

	FrameBuffer::~FrameBuffer()
	{
		Release();
	}

	void FrameBuffer::Define(RenderBuffer* colorBuffer, RenderBuffer* depthStencilBuffer)
	{
		if (!buffer) {
			Create();
		}

		Graphics::BindFramebuffer(this, nullptr);

		IntVector2 size = IntVector2::ZERO();
		if (colorBuffer) {
			size = colorBuffer->Size();
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer->GLBuffer());
		} else {
			glDrawBuffer(GL_NONE);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
		}

		if (depthStencilBuffer) {
			if (size != IntVector2::ZERO() && size != depthStencilBuffer->Size()) {
				LOG_WARNING("Framebuffer color and depth dimensions don't match");
			} else {
				size = depthStencilBuffer->Size();
			}
			unsigned stencil = Texture::IsStencil(depthStencilBuffer->Format()) ? depthStencilBuffer->GLBuffer() : 0;
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer->GLBuffer());
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil);
		} else {
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
		}

		LOG_DEBUG("Defined framebuffer object from render buffer: [{:d} x {:d}]", size.x, size.y);
	}

	void FrameBuffer::Define(RenderBuffer* const* colorBuffer, size_t countBuffers, RenderBuffer* depthStencilBuffer)
	{
		if (!buffer) {
			Create();
		}

		Graphics::BindFramebuffer(this, nullptr);

		IntVector2 size = IntVector2::ZERO();
		std::vector<GLenum> drawBufferIds;
		for (size_t i = 0; i < countBuffers; ++i) {
			if (colorBuffer[i]) {
				if (size != IntVector2::ZERO() && size != colorBuffer[i]->Size()) {
					LOG_WARNING("Framebuffer color dimensions don't match");
				} else {
					size = colorBuffer[i]->Size();
				}
				drawBufferIds.push_back(GL_COLOR_ATTACHMENT0 + (GLenum)i);
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_RENDERBUFFER, colorBuffer[i]->GLBuffer());
			} else {
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_RENDERBUFFER, 0);
			}
		}

		if (drawBufferIds.size()) {
			glDrawBuffers((GLsizei)drawBufferIds.size(), &drawBufferIds[0]);
		} else {
			glDrawBuffer(GL_NONE);
		}

		if (depthStencilBuffer) {
			if (size != IntVector2::ZERO() && size != depthStencilBuffer->Size()) {
				LOG_WARNING("Framebuffer color and depth dimensions don't match");
			} else {
				size = depthStencilBuffer->Size();
			}
			unsigned stencil = Texture::IsStencil(depthStencilBuffer->Format()) ? depthStencilBuffer->GLBuffer() : 0;
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer->GLBuffer());
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil);
		} else {
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
		}

		LOG_DEBUG("Defined MRT framebuffer object from render buffer: [{:d} x {:d}]", size.x, size.y);
	}

	void FrameBuffer::Define(Texture* colorTexture, Texture* depthStencilTexture)
	{
		if (!buffer) {
			Create();
		}

		Graphics::BindFramebuffer(this, nullptr);

		IntVector2 size = IntVector2::ZERO();
		if (colorTexture && colorTexture->Target() == TARGET_2D) {
			size = colorTexture->Size2D();
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture->GLTarget(), colorTexture->GLTexture(), 0);
		} else {
			glDrawBuffer(GL_NONE);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		}

		if (depthStencilTexture) {
			if (size != IntVector2::ZERO() && size != depthStencilTexture->Size2D()) {
				LOG_WARNING("Framebuffer color and depth dimensions don't match");
			} else {
				size = depthStencilTexture->Size2D();
			}
			unsigned stencil = Texture::IsStencil(depthStencilTexture->Format()) ? depthStencilTexture->GLTexture() : 0;
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthStencilTexture->GLTarget(), depthStencilTexture->GLTexture(), 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, depthStencilTexture->GLTarget(), stencil, 0);
		} else {
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		}

		LOG_DEBUG("Defined framebuffer object from texture: [{:d} x {:d}]", size.x, size.y);
	}

	void FrameBuffer::Define(Texture* colorTexture, size_t cubeMapFace, Texture* depthStencilTexture)
	{
		if (!buffer) {
			Create();
		}

		Graphics::BindFramebuffer(this, nullptr);

		IntVector2 size = IntVector2::ZERO();
		if (colorTexture && colorTexture->Target() == TARGET_CUBE) {
			size = colorTexture->Size2D();
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)cubeMapFace, colorTexture->GLTexture(), 0);
		} else {
			glDrawBuffer(GL_NONE);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		}

		if (depthStencilTexture) {
			if (size != IntVector2::ZERO() && size != depthStencilTexture->Size2D()) {
				LOG_WARNING("Framebuffer color and depth dimensions don't match");
			} else {
				size = depthStencilTexture->Size2D();
			}
			unsigned stencil = Texture::IsStencil(depthStencilTexture->Format()) ? depthStencilTexture->GLTexture() : 0;
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthStencilTexture->GLTarget(), depthStencilTexture->GLTexture(), 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, depthStencilTexture->GLTarget(), stencil, 0);
		} else {
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		}

		LOG_DEBUG("Defined framebuffer object from cube texture: [{:d} x {:d}] Face [{:d}]", size.x, size.y, cubeMapFace);
	}

	void FrameBuffer::Define(Texture* const* colorTextures, size_t countColorTextures, Texture* depthStencilTexture)
	{
		if (!buffer) {
			Create();
		}

		Graphics::BindFramebuffer(this, nullptr);

		IntVector2 size = IntVector2::ZERO();
		std::vector<GLenum> drawBufferIds;
		for (size_t i = 0; i < countColorTextures; ++i) {
			if (colorTextures[i] && colorTextures[i]->Target() == TARGET_2D) {
				if (size != IntVector2::ZERO() && size != colorTextures[i]->Size2D()) {
					LOG_WARNING("Framebuffer color dimensions don't match");
				} else {
					size = colorTextures[i]->Size2D();
				}
				drawBufferIds.push_back(GL_COLOR_ATTACHMENT0 + (GLenum)i);
				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, colorTextures[i]->GLTarget(), colorTextures[i]->GLTexture(), 0);
			} else {
				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D, 0, 0);
			}
		}

		if (drawBufferIds.size()) {
			glDrawBuffers((GLsizei)drawBufferIds.size(), &drawBufferIds[0]);
		} else {
			glDrawBuffer(GL_NONE);
		}

		if (depthStencilTexture) {
			if (size != IntVector2::ZERO() && size != depthStencilTexture->Size2D()) {
				LOG_WARNING("Framebuffer color and depth dimensions don't match");
			} else {
				size = depthStencilTexture->Size2D();
			}
			unsigned stencil = Texture::IsStencil(depthStencilTexture->Format()) ? depthStencilTexture->GLTexture() : 0;
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthStencilTexture->GLTarget(), depthStencilTexture->GLTexture(), 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, depthStencilTexture->GLTarget(), stencil, 0);
		} else {
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		}

		LOG_DEBUG("Defined MRT framebuffer object: {:d} [{:d} x {:d}]", countColorTextures, size.x, size.y);
	}

	bool FrameBuffer::Create()
	{
		glGenFramebuffers(1, &buffer);
		if (!buffer) {
			LOG_ERROR("Failed to create framebuffer object");
			return false;
		}
		return true;
	}

	void FrameBuffer::Release()
	{
		if (buffer) {
			Graphics::UnbindFramebuffer(this);
			glDeleteFramebuffers(1, &buffer);
			buffer = 0;
		}
	}
}
