#include <Turso3D/IO/Stream.h>

namespace Turso3D
{
	Stream::Stream() :
		position(0),
		size(0)
	{
	}

	Stream::~Stream() = default;

	void Stream::SetName(const std::string& newName)
	{
		name = newName;
	}

	void Stream::SetName(const char* newName)
	{
		name = newName;
	}

	std::string Stream::ReadLine()
	{
		std::string ret;

		while (!IsEof()) {
			char c = Read<char>();
			if (c == '\n') {
				break;
			}
			if (c == '\r') {
				// Peek next char to see if it's 10, and skip it too
				if (!IsEof()) {
					char next = Read<char>();
					if (next != '\n') {
						Seek(position - 1);
					}
				}
				break;
			}
			ret += c;
		}

		return ret;
	}

	void Stream::WriteLine(const std::string& data)
	{
		Write(data.c_str(), data.length());
		Write('\r');
		Write('\n');
	}
}
