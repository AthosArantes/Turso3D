#include "ShaderPermutation.h"
#include <type_traits>

namespace
{
	template <typename Pred>
	void IterateDefines(std::string_view defines, Pred pred)
	{
		static_assert(std::is_invocable<Pred, std::string_view>::value);

		while (!defines.empty()) {
			// Remove leading space
			while (std::isspace(static_cast<const unsigned char&>(defines.front()))) {
				defines.remove_prefix(1);
				continue;
			}

			// Find the next separator
			size_t count = 0;
			size_t sep = 0;
			for (; count < defines.length(); ++count) {
				const unsigned char& c = static_cast<const unsigned char&>(defines[count]);
				if (std::isspace(c) || c == ';') {
					sep = 1;
					break;
				}
			}

			if (count) {
				pred(defines.substr(0, count));
			}
			defines.remove_prefix(count + sep);
		}
	}
}

namespace Turso3D
{
	ShaderPermutation::ShaderPermutation() noexcept :
		hash(0)
	{
	}

	ShaderPermutation::ShaderPermutation(const ShaderPermutation& p)
	{
		buffer = p.buffer;
		Process();
	}

	ShaderPermutation::ShaderPermutation(ShaderPermutation&& p) noexcept
	{
		buffer.swap(p.buffer);
		defines.swap(p.defines);
		hash = p.hash;
	}

	ShaderPermutation::ShaderPermutation(std::string&& defines)
	{
		buffer.swap(defines);
		Process();
	}

	ShaderPermutation::ShaderPermutation(std::initializer_list<std::string_view> defines)
	{
		size_t length = 0;
		for (auto it = defines.begin(); it != defines.end(); ++it) {
			length += it->length() + 1;
		}

		std::string new_buffer;
		new_buffer.reserve(length);

		for (auto it = defines.begin(); it != defines.end(); ++it) {
			new_buffer.push_back(';');
			new_buffer.append(*it);
		}

		buffer.swap(new_buffer);
		Process();
	}

	ShaderPermutation& ShaderPermutation::operator=(const ShaderPermutation& rhs)
	{
		buffer = rhs.buffer;
		Process();
		return *this;
	}

	ShaderPermutation& ShaderPermutation::operator=(ShaderPermutation&& rhs) noexcept
	{
		buffer.swap(rhs.buffer);
		defines.swap(rhs.defines);
		hash = rhs.hash;
		return *this;
	}

	void ShaderPermutation::Process()
	{
		std::string_view src {buffer};

		// Count how many defines to do a single allocation for the vector
		size_t count = 0;
		IterateDefines(src, [&count](std::string_view)
		{
			++count;
		});

		defines.clear();
		defines.reserve(count);
		IterateDefines(src, [&](std::string_view def)
		{
			Add(def);
		});

		UpdateHash();
	}

	bool ShaderPermutation::Exists(std::string_view define, size_t* outIndex) const
	{
		std::string_view name = NamePart(define);
		if (name.empty()) {
			return false;
		}

		for (size_t i = 0; i < defines.size(); ++i) {
			if (NamePart(defines[i]) == name) {
				if (outIndex) {
					*outIndex = i;
				}
				return true;
			}
		}

		return false;
	}

	void ShaderPermutation::Add(std::string_view define)
	{
		size_t i;
		if (Exists(define, &i)) {
			defines[i] = define;
			return;
		}

		std::string_view name = NamePart(define);
		if (!name.empty()) {
			std::string_view value = ValuePart(define);
			defines.push_back(value.empty() ? name : define);
		}
	}

	void ShaderPermutation::UpdateHash()
	{
		hash = 0;
		for (size_t i = 0; i < defines.size(); ++i) {
			hash ^= std::hash<std::string_view> {}(defines[i]);
		}
	}

	// ==========================================================================================
	std::string_view ShaderPermutation::NamePart(std::string_view define) noexcept
	{
		for (size_t i = 0; i < define.length(); ++i) {
			if (define[i] == '=') {
				return define.substr(0, i);
			}
		}
		return define;
	}

	std::string_view ShaderPermutation::ValuePart(std::string_view define) noexcept
	{
		for (size_t i = 0; i < define.length(); ++i) {
			if (define[i] == '=') {
				return define.substr(i + 1);
			}
		}
		return std::string_view {};
	}
}
