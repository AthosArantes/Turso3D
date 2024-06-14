#pragma once

#include <Turso3D/fwd.h>
#include <RmlUi/Core/FileInterface.h>
#include <vector>
#include <memory>

class RmlFile : public Rml::FileInterface
{
public:
	RmlFile();
	~RmlFile();

	Rml::FileHandle Open(const Rml::String& path) override;
	void Close(Rml::FileHandle file) override;

	size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;

private:
	// Resources in use by RmlUi
	std::vector<std::unique_ptr<Turso3D::Stream>> resources;
};
