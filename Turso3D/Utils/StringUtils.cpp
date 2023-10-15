#include "StringUtils.h"
#include <string_view>

namespace Turso3D
{
	void SplitStringWhitespace(const std::string& str, std::vector<std::string>& dest, bool checked)
	{
		auto start = str.cbegin();

		for (auto it = start; it < str.cend(); ++it) {
			if (!std::isspace(*it)) {
				continue;
			}
			std::string_view define(&(*start), it - start);
			start = it + 1;
			if (define.length()) {
				if (checked && std::find(dest.cbegin(), dest.cend(), define) != dest.cend()) {
					continue;
				}
				dest.emplace_back(std::string(define));
			}
		}

		if (start < str.cend()) {
			std::string_view define(&(*start), str.cend() - start);
			if (define.length()) {
				if (checked && std::find(dest.cbegin(), dest.cend(), define) != dest.cend()) {
					return;
				}
				dest.emplace_back(std::string(define));
			}
		}
	}

	std::string JoinString(const std::vector<std::string>& src, char separator)
	{
		std::string result;
		for (const std::string& str : src) {
			if (str.size()) {
				if (result.size()) {
					result += separator;
				}
				result += str;
			}
		}
		return result;
	}
}
