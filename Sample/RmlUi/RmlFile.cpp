#include "RmlFile.h"
#include <Turso3D/IO/Log.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <cassert>

using namespace Turso3D;

RmlFile::RmlFile()
{
}

RmlFile::~RmlFile()
{
}

Rml::FileHandle RmlFile::Open(const Rml::String& path)
{
	ResourceCache* cache = ResourceCache::Instance();
	std::unique_ptr<Stream> data = cache->OpenData(path);
	if (data) {
		std::unique_ptr<Stream>& res = resources.emplace_back(std::move(data));
		return reinterpret_cast<Rml::FileHandle>(res.get());
	}
	return {};
}

void RmlFile::Close(Rml::FileHandle file)
{
	Stream* stream = reinterpret_cast<Stream*>(file);
	for (auto it = resources.begin(); it != resources.end(); ++it) {
		if (it->get() == stream) {
			it->swap(resources.back());
			resources.pop_back();
			return;
		}
	}
}

size_t RmlFile::Read(void* buffer, size_t size, Rml::FileHandle file)
{
	Stream* stream = reinterpret_cast<Stream*>(file);
	return stream->Read(buffer, size);
}

bool RmlFile::Seek(Rml::FileHandle file, long offset, int origin)
{
	Stream* stream = reinterpret_cast<Stream*>(file);

	size_t prevPos = stream->Position();
	size_t pos;
	switch (origin) {
		case SEEK_SET:
			pos = stream->Seek(offset);
			break;
		case SEEK_CUR:
			pos = stream->Seek(stream->Position() + offset);
			break;
		case SEEK_END:
			pos = stream->Seek(stream->Size() - offset);
			break;
		default:
			return false;
	}

	return pos != prevPos;
}

size_t RmlFile::Tell(Rml::FileHandle file)
{
	Stream* stream = reinterpret_cast<Stream*>(file);
	return stream->Position();
}
