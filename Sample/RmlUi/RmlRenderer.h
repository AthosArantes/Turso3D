#include <RmlUi/Core/RenderInterface.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Math/IntVector2.h>
#include <vector>
#include <memory>

namespace Turso3D
{
	class Graphics;
	class FrameBuffer;
	class ShaderProgram;
	class Texture;
	class IntVector2;

	class RmlRenderer : public Rml::RenderInterface
	{
		enum class ScissorState
		{
			None,
			Stencil,
			Scissor
		};

		struct CompiledGeometry
		{
			VertexBuffer vbo;
			IndexBuffer ibo;
			Texture* texture;

			ShaderProgram* program;
			int uTranslate; // uniform translate location
			int uTransform; // uniform transform location
		};

	public:
		// Constructor
		// Graphics subsystem must have been initialized.
		RmlRenderer(Graphics* graphics);
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

		void UpdateBuffers(const IntVector2& size, int multisample);
		void BeginRender();
		void EndRender();

		FrameBuffer* GetFramebuffer() { return fbo[0].get(); }
		Texture* GetTexture() { return buffer[0].get(); }

	private:
		// Cached graphics subsystem
		Graphics* graphics;

		std::shared_ptr<ShaderProgram> texProgram;
		std::shared_ptr<ShaderProgram> colorProgram;

		// Multisampled/Resolved buffers
		std::unique_ptr<Texture> buffer[2];
		std::unique_ptr<FrameBuffer> fbo[2];

		IntVector2 viewSize;
		unsigned multisample;

		// Textures in use by RmlUi
		std::vector<std::shared_ptr<Texture>> textures;
		// Geometries in use by RmlUi
		std::vector<std::unique_ptr<CompiledGeometry>> geometries;

		ScissorState scissorState;

		Rml::Matrix4f projection;
		Rml::Matrix4f transform;
		bool usingTransform;
	};
}
