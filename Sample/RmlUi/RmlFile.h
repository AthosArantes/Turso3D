#include <RmlUi/Core/FileInterface.h>
#include <vector>
#include <memory>

namespace Turso3D
{
	class Stream;

	class RmlFile : public Rml::FileInterface
	{
	public:
		Rml::FileHandle Open(const Rml::String& path) override;
		void Close(Rml::FileHandle file) override;
		
		size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
		bool Seek(Rml::FileHandle file, long offset, int origin) override;
		size_t Tell(Rml::FileHandle file) override;

	private:
		// Resources in use by RmlUi
		std::vector<std::unique_ptr<Stream>> resources;
	};
}
