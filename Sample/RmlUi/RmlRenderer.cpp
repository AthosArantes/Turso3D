#include "RmlRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <GLEW/glew.h>
#include <cassert>

namespace Turso3D
{
	static VertexElement VertexElementArray[] = {
		VertexElement {ELEM_VECTOR2, SEM_POSITION},
		VertexElement {ELEM_UBYTE4, SEM_COLOR},
		VertexElement {ELEM_VECTOR2, SEM_TEXCOORD}
	};

	// ==========================================================================================
	RmlRenderer::RmlRenderer(Graphics* graphics) :
		graphics(graphics)
	{
		for (int i = 0; i < 2; ++i) {
			buffer[i] = std::make_unique<Texture>();
			fbo[i] = std::make_unique<FrameBuffer>();
		}

		constexpr StringHash uniformTranslate {"uTranslate"};
		constexpr StringHash uniformTransform {"uTransform"};

		texProgram = graphics->CreateProgram("RmlUi.glsl", "TEXTURED", "TEXTURED");
		uTexTranslate = texProgram->Uniform(uniformTranslate);
		uTexTransform = texProgram->Uniform(uniformTransform);

		colorProgram = graphics->CreateProgram("RmlUi.glsl", "", "");
		uTranslate = colorProgram->Uniform(uniformTranslate);
		uTransform = colorProgram->Uniform(uniformTransform);

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
		for (std::unique_ptr<CompiledGeometry>& dg : discardedGeometries) {
			if (dg->vbo.NumVertices() == num_vertices && dg->ibo.NumIndices() == num_indices) {
				dg->vbo.SetData(0, num_vertices, vertices);
				dg->ibo.SetData(0, num_indices, indices);
				dg->texture = reinterpret_cast<Texture*>(texture);

				// Update the current discarded mem
				discardedGeometryMem -= dg->vbo.VertexSize() * num_vertices;
				discardedGeometryMem -= dg->ibo.IndexSize() * num_indices;

				Rml::CompiledGeometryHandle handle = reinterpret_cast<Rml::CompiledGeometryHandle>(dg.get());

				// Move to the back of the discarded vector, then move it to the current geometries and
				// finally pop the empty unique_ptr from the discarded vector.
				discardedGeometries.back().swap(dg);
				geometries.emplace_back().swap(discardedGeometries.back());
				discardedGeometries.pop_back();

				return handle;
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

		if (cg->texture) {
			texProgram->Bind();
			glUniform2f(uTexTranslate, translation.x, translation.y);
			glUniformMatrix4fv(uTexTransform, 1, GL_FALSE, transform.data());
			cg->vbo.Bind(texProgram->Attributes());
			cg->ibo.Bind();
			cg->texture->Bind(0);

		} else {
			colorProgram->Bind();
			glUniform2f(uTranslate, translation.x, translation.y);
			glUniformMatrix4fv(uTransform, 1, GL_FALSE, transform.data());
			cg->vbo.Bind(colorProgram->Attributes());
			cg->ibo.Bind();
		}

		graphics->DrawIndexed(PT_TRIANGLE_LIST, 0, cg->ibo.NumIndices());
	}

	void RmlRenderer::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle handle)
	{
		CompiledGeometry* cg = reinterpret_cast<CompiledGeometry*>(handle);
		for (size_t i = 0; i < geometries.size(); ++i) {
			if (geometries[i].get() == cg) {
				geometries[i].swap(geometries.back());

				// Keep the released geometry alive
				{
					std::unique_ptr<CompiledGeometry>& dg = discardedGeometries.emplace_back();
					dg.swap(geometries.back());
					geometries.pop_back();

					discardedGeometryMem += dg->vbo.VertexSize() * dg->vbo.NumVertices();
					discardedGeometryMem += dg->ibo.IndexSize() * dg->ibo.NumIndices();
				}

				// Free some geometries if exceeded the memory limit
				while (discardedGeometryMem > maxDiscardedGeometryMem && !discardedGeometries.empty()) {
					std::unique_ptr<CompiledGeometry>& dg = discardedGeometries[0];
					discardedGeometryMem -= dg->vbo.VertexSize() * dg->vbo.NumVertices();
					discardedGeometryMem -= dg->ibo.IndexSize() * dg->ibo.NumIndices();

					discardedGeometries.back().swap(dg);
					discardedGeometries.pop_back();
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
			//tex->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

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
		for (size_t i = 0; i < textures.size(); ++i) {
			if (textures[i].get() == tex) {
				textures[i].swap(textures.back());
				textures.pop_back();
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
		multisample = multisample_;

		for (int i = 0; i < 2; ++i) {
			buffer[i]->Define(TARGET_2D, size, FORMAT_RGBA8_SRGB_PACK32, multisample * i);
			buffer[i]->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
			fbo[i]->Define(buffer[i].get(), nullptr);

			if (multisample <= 1) {
				multisample = 1;
				break;
			}
		}

		projection = Rml::Matrix4f::ProjectOrtho(0, (float)viewSize.x, (float)viewSize.y, 0, -10000, 10000);
		SetTransform(nullptr);
	}

	void RmlRenderer::BeginRender()
	{
#ifdef _DEBUG
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "RmlUi Render");
#endif

		if (multisample > 1) {
			graphics->SetFrameBuffer(fbo[1].get());
		} else {
			graphics->SetFrameBuffer(fbo[0].get());
		}

		// IMPROVE: better organize this

		glClearStencil(0);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glDisable(GL_CULL_FACE);

		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, GLuint(-1));
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	}

	void RmlRenderer::EndRender()
	{
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);

		// Resolve multisampled buffer
		if (multisample > 1) {
			IntRect rc {0, 0, viewSize.x, viewSize.y};
			graphics->Blit(fbo[0].get(), rc, fbo[1].get(), rc, true, false, FILTER_BILINEAR);
		}

#ifdef _DEBUG
		glPopDebugGroup();
#endif
	}
}
