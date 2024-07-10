#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/fwd.h>
#include <vector>
#include <memory>

class RmlRenderer : public Rml::RenderInterface
{
	enum class ScissorState
	{
		None,
		Stencil,
		Scissor
	};

	struct ShaderProgramGroup
	{
		std::shared_ptr<Turso3D::ShaderProgram> program;
		int translateIndex; // Translation uniform location.
		int transformIndex; // Transform uniform location.
	};

	struct CompiledGeometry;

public:
	// Constructor
	// Graphics subsystem must have been initialized.
	RmlRenderer(Turso3D::Graphics* graphics);
	// Destructor
	~RmlRenderer();

	void RenderGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, const Rml::TextureHandle texture, const Rml::Vector2f& translation) override;
	Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture) override;
	void RenderCompiledGeometry(Rml::CompiledGeometryHandle handle, const Rml::Vector2f& translation) override;
	void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle handle) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(int x, int y, int width, int height) override;

	bool LoadTexture(Rml::TextureHandle& texture_handle, Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
	bool GenerateTexture(Rml::TextureHandle& texture_handle, const Rml::byte* source, const Rml::Vector2i& source_dimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture_handle) override;

	void SetTransform(const Rml::Matrix4f* new_transform) override;

	void UpdateBuffers(const Turso3D::IntVector2& size, int multisample);
	void BeginRender();
	void EndRender();

	// Return the color texture.
	Turso3D::Texture* GetTexture() { return buffer[0].get(); }
	// Return the mask texture.
	Turso3D::Texture* GetMaskTexture() { return buffer[1].get(); }

private:
	// Cached graphics subsystem
	Turso3D::Graphics* graphics;

	ShaderProgramGroup programs[2];

	// Color/Mask buffers.
	std::unique_ptr<Turso3D::Texture> buffer[2];
	// Color/Mask renderbuffers.
	std::unique_ptr<Turso3D::RenderBuffer> rbo[2];
	// Framebuffer.
	std::unique_ptr<Turso3D::FrameBuffer> fbo;

	// Framebuffers used to resolve multisampled renderbuffers
	std::unique_ptr<Turso3D::FrameBuffer> srcFbo[2];
	std::unique_ptr<Turso3D::FrameBuffer> dstFbo[2];

	// Textures in use by RmlUi
	std::vector<std::shared_ptr<Turso3D::Texture>> textures;
	// Geometries in use by RmlUi
	std::vector<std::unique_ptr<CompiledGeometry>> geometries;

	// Discarded geometries, kept alive to avoid possible reallocation.
	std::vector<std::unique_ptr<CompiledGeometry>> discardedGeometries;
	// Maximum amount of memory to be used for discarded geometries.
	size_t maxDiscardedGeometryMem;
	// Current memory of discarded geometries
	size_t discardedGeometryMem;

	Rml::Matrix4f projection;
	Rml::Matrix4f transform;
	bool usingTransform;

	ScissorState scissorState;

	Turso3D::IntVector2 viewSize;
	int multisample;
};
