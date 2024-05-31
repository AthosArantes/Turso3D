#include "RmlRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/RenderBuffer.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Math/Matrix3x4.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <GLEW/glew.h>
#include <cassert>

namespace Turso3D
{
	enum ProgramGroupIndex
	{
		COLOR_PROGRAM,
		TEXTURED_PROGRAM
	};

	static VertexElement VertexElementArray[] = {
		VertexElement {ELEM_VECTOR2, SEM_POSITION},
		VertexElement {ELEM_UBYTE4, SEM_COLOR},
		VertexElement {ELEM_VECTOR2, SEM_TEXCOORD}
	};

	// ==========================================================================================
	struct RmlRenderer::CompiledGeometry
	{
		VertexBuffer vbo;
		IndexBuffer ibo;
		Texture* texture;
	};

	// ==========================================================================================
	RmlRenderer::RmlRenderer(Graphics* graphics) :
		graphics(graphics)
	{
		for (int i = 0; i < 2; ++i) {
			buffer[i] = std::make_unique<Texture>();
			rbo[i] = std::make_unique<RenderBuffer>();
			srcFbo[i] = std::make_unique<FrameBuffer>();
			dstFbo[i] = std::make_unique<FrameBuffer>();
		}
		fbo = std::make_unique<FrameBuffer>();

		constexpr const char* defines[] {
			"",
			"TEXTURED"
		};
		constexpr StringHash uniformTranslateHash {"translate"};
		constexpr StringHash uniformTransformHash {"transform"};

		for (unsigned i = 0; i < 2; ++i) {
			programs[i].program = graphics->CreateProgram("RmlUi.glsl", defines[i], defines[i]);
			programs[i].translateIndex = programs[i].program->Uniform(uniformTranslateHash);
			programs[i].transformIndex = programs[i].program->Uniform(uniformTransformHash);
		}

		//*
		vao.Define();

		glEnableVertexAttribArray(ATTR_POSITION);
		glVertexAttribFormat(ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, offsetof(Rml::Vertex, position));
		glVertexAttribBinding(ATTR_POSITION, 0);

		glEnableVertexAttribArray(ATTR_VERTEXCOLOR);
		glVertexAttribFormat(ATTR_VERTEXCOLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(Rml::Vertex, colour));
		glVertexAttribBinding(ATTR_VERTEXCOLOR, 0);

		glEnableVertexAttribArray(ATTR_TEXCOORD);
		glVertexAttribFormat(ATTR_TEXCOORD, 2, GL_FLOAT, GL_FALSE, offsetof(Rml::Vertex, tex_coord));
		glVertexAttribBinding(ATTR_TEXCOORD, 0);

		graphics->DefaultVao().Bind();
		//*/

		maxDiscardedGeometryMem = 8 * 1000 * 1000; // 8 MB
		discardedGeometryMem = 0;
	}

	RmlRenderer::~RmlRenderer()
	{
	}

	void RmlRenderer::RenderGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, const Rml::TextureHandle texture, const Rml::Vector2f& translation)
	{
		CompiledGeometry cg;
		cg.vbo.Define(USAGE_DEFAULT, num_vertices, VertexElementArray, std::size(VertexElementArray), vertices);
		cg.ibo.Define(USAGE_DEFAULT, num_indices, sizeof(unsigned), indices);
		cg.texture = reinterpret_cast<Texture*>(texture);

		RenderCompiledGeometry(reinterpret_cast<Rml::CompiledGeometryHandle>(&cg), translation);
	}

	Rml::CompiledGeometryHandle RmlRenderer::CompileGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture)
	{
		Log::Scope ls {"RmlRenderer::CompileGeometry"};

		// Try to find a previously discarded geometry
		for (auto it = discardedGeometries.begin(); it != discardedGeometries.end(); ++it) {
			CompiledGeometry* cg = it->get();
			if (cg->vbo.NumVertices() == num_vertices && cg->ibo.NumIndices() == num_indices) {
				cg->vbo.SetData(0, num_vertices, vertices);
				cg->ibo.SetData(0, num_indices, indices);
				cg->texture = reinterpret_cast<Texture*>(texture);

				// Update the current discarded mem
				discardedGeometryMem -= cg->vbo.VertexSize() * num_vertices;
				discardedGeometryMem -= cg->ibo.IndexSize() * num_indices;

				it->release();
				discardedGeometries.erase(it);
				geometries.emplace_back(std::unique_ptr<CompiledGeometry> {cg});

				return reinterpret_cast<Rml::CompiledGeometryHandle>(cg);
			}
		}

		std::unique_ptr<CompiledGeometry>& cg = geometries.emplace_back(std::make_unique<CompiledGeometry>());
		cg->vbo.Define(USAGE_DEFAULT, num_vertices, VertexElementArray, std::size(VertexElementArray), vertices);
		cg->ibo.Define(USAGE_DEFAULT, num_indices, sizeof(unsigned), indices);
		cg->texture = reinterpret_cast<Texture*>(texture);

		return reinterpret_cast<Rml::CompiledGeometryHandle>(cg.get());
	}

	void RmlRenderer::RenderCompiledGeometry(Rml::CompiledGeometryHandle handle, const Rml::Vector2f& translation)
	{
		CompiledGeometry* cg = reinterpret_cast<CompiledGeometry*>(handle);

		ShaderProgramGroup& program_group = programs[(cg->texture != nullptr) ? TEXTURED_PROGRAM : COLOR_PROGRAM];
		program_group.program->Bind();
		glUniform2f(program_group.translateIndex, translation.x, translation.y);
		glUniformMatrix4fv(program_group.transformIndex, 1, GL_FALSE, transform.data());

		glBindVertexBuffer(0, cg->vbo.GLBuffer(), 0, sizeof(Rml::Vertex));
		cg->ibo.Bind();

		if (cg->texture) {
			cg->texture->Bind(0);
		}

		glDrawElements(GL_TRIANGLES, cg->ibo.NumIndices(), GL_UNSIGNED_INT, nullptr);
	}

	void RmlRenderer::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle handle)
	{
		CompiledGeometry* cg = reinterpret_cast<CompiledGeometry*>(handle);

		for (auto it = geometries.begin(); it != geometries.end(); ++it) {
			if (it->get() == cg) {
				it->release();
				geometries.erase(it);

				// Keep the released geometry alive
				discardedGeometries.emplace_back(std::unique_ptr<CompiledGeometry> {cg});
				discardedGeometryMem += cg->vbo.VertexSize() * cg->vbo.NumVertices();
				discardedGeometryMem += cg->ibo.IndexSize() * cg->ibo.NumIndices();

				// Free some geometries if exceeded the memory limit
				while (discardedGeometryMem > maxDiscardedGeometryMem && !discardedGeometries.empty()) {
					std::unique_ptr<CompiledGeometry>& dg = discardedGeometries[0];
					discardedGeometryMem -= dg->vbo.VertexSize() * dg->vbo.NumVertices();
					discardedGeometryMem -= dg->ibo.IndexSize() * dg->ibo.NumIndices();
					discardedGeometries.erase(discardedGeometries.begin());
				}
				return;
			}
		}
	}

	void RmlRenderer::EnableScissorRegion(bool enable)
	{
		ScissorState new_state = ScissorState::None;

		if (enable) {
			new_state = (usingTransform ? ScissorState::Stencil : ScissorState::Scissor);
		}

		if (new_state != scissorState) {
			// Disable old
			if (scissorState == ScissorState::Scissor) {
				glDisable(GL_SCISSOR_TEST);
			} else if (scissorState == ScissorState::Stencil) {
				glStencilFunc(GL_ALWAYS, 1, GLuint(-1));
			}

			// Enable new
			if (new_state == ScissorState::Scissor) {
				glEnable(GL_SCISSOR_TEST);
			} else if (new_state == ScissorState::Stencil) {
				glStencilFunc(GL_EQUAL, 1, GLuint(-1));
			}

			scissorState = new_state;
		}
	}

	void RmlRenderer::SetScissorRegion(int x, int y, int width, int height)
	{
		if (usingTransform) {
			const float left = float(x);
			const float right = float(x + width);
			const float top = float(y);
			const float bottom = float(y + height);

			Rml::Vertex vertices[4];
			vertices[0].position = {left, top};
			vertices[1].position = {right, top};
			vertices[2].position = {right, bottom};
			vertices[3].position = {left, bottom};

			int indices[6] = {0, 2, 1, 0, 3, 2};

			glClear(GL_STENCIL_BUFFER_BIT);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glStencilFunc(GL_ALWAYS, 1, GLuint(-1));
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

			RenderGeometry(vertices, 4, indices, 6, 0, Rml::Vector2f(0, 0));

			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			glStencilFunc(GL_EQUAL, 1, GLuint(-1));
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		} else {
			glScissor(x, viewSize.y - (y + height), width, height);
		}
	}

	bool RmlRenderer::LoadTexture(Rml::TextureHandle& texture_handle, Rml::Vector2i& texture_dimensions, const Rml::String& source)
	{
		ResourceCache* cache = ResourceCache::Instance();
		std::shared_ptr<Texture> tex = cache->LoadResource<Texture>(source, Texture::LOAD_FLAG_SRGB);
		if (tex) {
			tex->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

			texture_dimensions.x = tex->Width();
			texture_dimensions.y = tex->Height();

			texture_handle = reinterpret_cast<Rml::TextureHandle>(tex.get());
			textures.emplace_back().swap(tex);
			return true;
		}
		return false;
	}

	bool RmlRenderer::GenerateTexture(Rml::TextureHandle& texture_handle, const Rml::byte* source, const Rml::Vector2i& source_dimensions)
	{
		Log::Scope ls {"RmlRenderer::GenerateTexture"};

		std::shared_ptr<Texture> tex = std::make_shared<Texture>();
		if (tex) {
			IntVector3 sz {source_dimensions.x, source_dimensions.y, 1};
			ImageLevel il {source, 0, IntBox {0, 0, 0, sz.x, sz.y, 0}, 0, 0};

			bool defined = tex->Define(TARGET_2D, sz, FORMAT_RGBA8_SRGB_PACK32, 1, 1);
			if (!defined) {
				LOG_ERROR("Failed to define texture.");
				return false;
			}
			tex->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
			tex->SetData(il);

			texture_handle = reinterpret_cast<Rml::TextureHandle>(tex.get());
			textures.push_back(tex);
			return true;
		}
		return false;
	}

	void RmlRenderer::ReleaseTexture(Rml::TextureHandle texture_handle)
	{
		Texture* tex = reinterpret_cast<Texture*>(texture_handle);
		for (auto it = textures.begin(); it != textures.end(); ++it) {
			if (it->get() == tex) {
				textures.erase(it);
				return;
			}
		}
	}

	void RmlRenderer::SetTransform(const Rml::Matrix4f* new_transform)
	{
		transform = projection * (new_transform ? *new_transform : Rml::Matrix4f::Identity());
		usingTransform = (new_transform != nullptr);
	}

	void RmlRenderer::UpdateBuffers(const IntVector2& size, int multisample_)
	{
		Log::Scope ls {"RmlRenderer::UpdateBuffers"};

		viewSize = size;
		multisample = std::max(multisample_, 1);

		// Color/Mask formats
		constexpr ImageFormat formats[] = {FORMAT_RGBA8_SRGB_PACK32, FORMAT_R8_UNORM_PACK8};

		for (int i = 0; i < 2; ++i) {
			buffer[i]->Define(TARGET_2D, size, formats[i], 1, 1);
			buffer[i]->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		}

		if (multisample > 1) {
			for (int i = 0; i < 2; ++i) {
				rbo[i]->Define(size, formats[i], multisample);

				srcFbo[i]->Define(rbo[i].get(), nullptr);
				dstFbo[i]->Define(buffer[i].get(), nullptr);
			}

			RenderBuffer* const mrt[] {rbo[0].get(), rbo[1].get()};
			fbo->Define(mrt, 2, nullptr);

		} else {
			Texture* const mrt[] {buffer[0].get(), buffer[1].get()};
			fbo->Define(mrt, 2, nullptr);
		}

		projection = Rml::Matrix4f::ProjectOrtho(0, (float)size.x, (float)size.y, 0, -10000, 10000);
		SetTransform(nullptr);
	}

	void RmlRenderer::BeginRender()
	{
		fbo->Bind();

		glClearStencil(0);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		graphics->SetRenderState(BLEND_ADDALPHA, CULL_NONE, CMP_ALWAYS, true, false);

		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, GLuint(-1));
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		vao.Bind();
	}

	void RmlRenderer::EndRender()
	{
		glDisable(GL_STENCIL_TEST);

		// Resolve multisampled buffer
		if (multisample > 1) {
			IntRect rc {IntVector2::ZERO(), viewSize};
			for (int i = 0; i < 2; ++i) {
				graphics->Blit(dstFbo[i].get(), rc, srcFbo[i].get(), rc, true, false, FILTER_BILINEAR);
			}
		}

		graphics->DefaultVao().Bind();
	}
}
