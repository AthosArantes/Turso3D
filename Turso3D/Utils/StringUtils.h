#pragma once

#include <string>
#include <vector>

namespace Turso3D
{
	void SplitStringWhitespace(const std::string& str, std::vector<std::string>& dest, bool checked = true);

	inline std::vector<std::string> SplitStringWhitespace(const std::string& str, bool checked = true)
	{
		std::vector<std::string> result;
		SplitStringWhitespace(str, result, checked);
		return result;
	}

	std::string JoinString(const std::vector<std::string>& src, char separator);
}
