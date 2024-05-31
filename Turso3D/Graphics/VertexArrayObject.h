#pragma once

namespace Turso3D
{
	class VertexArrayObject
	{
	public:
		VertexArrayObject();
		~VertexArrayObject();

		// Create a new VAO and bind it.
		// no-op if already defined.
		bool Define();
		// Bind the VAO, no-op if already bound.
		void Bind();

		// Return whether this VAO is defined.
		bool IsDefined() const { return vao != 0; }
		// Return OpenGL internal object.
		unsigned GLObject() const { return vao; }

		// Unbind current VAO.
		static void Unbind();

	private:
		void Release();

	private:
		// OpenGL object
		unsigned vao;
	};
}
