#pragma once

namespace Turso3D
{
	class VertexArrayObject
	{
	public:
		VertexArrayObject();
		~VertexArrayObject();

		// Create a new VAO.
		bool Define();
		// Bind the VAO, no-op if already bound.
		void Bind();

		operator bool() const { return vao != 0; }

		static void Unbind();

	private:
		void Release();

	private:
		// OpenGL object
		unsigned vao;
	};
}
