#include <Turso3D/Graphics/VertexArrayObject.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>

namespace Turso3D
{
	static VertexArrayObject* BoundVAO = nullptr;

	// ==========================================================================================
	VertexArrayObject::VertexArrayObject() :
		vao(0)
	{
	}
	VertexArrayObject::~VertexArrayObject()
	{
		Release();
	}

	bool VertexArrayObject::Define()
	{
		if (vao) {
			return true;
		}

		glGenVertexArrays(1, &vao);
		if (!vao) {
			LOG_ERROR("Failed to create VAO.");
			return false;
		}

		Bind();

		return true;
	}

	void VertexArrayObject::Bind()
	{
		if (BoundVAO != this) {
			glBindVertexArray(vao);
			BoundVAO = this;
		}
	}

	void VertexArrayObject::Release()
	{
		if (vao) {
			glDeleteVertexArrays(1, &vao);
			vao = 0;
			if (BoundVAO == this) {
				BoundVAO = nullptr;
			}
		}
	}

	// ==========================================================================================
	void VertexArrayObject::Unbind()
	{
		glBindVertexArray(0);
		BoundVAO = nullptr;
	}
}
