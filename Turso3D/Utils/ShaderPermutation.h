#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <string_view>
#include <initializer_list>

namespace Turso3D
{
	// Immutable class used to hold shader defines.
	class ShaderPermutation
	{
	public:
		ShaderPermutation() noexcept;
		ShaderPermutation(const ShaderPermutation& p);
		ShaderPermutation(ShaderPermutation&& p) noexcept;
		ShaderPermutation(std::string&& defines);
		ShaderPermutation(std::initializer_list<std::string_view> defines);

		ShaderPermutation& operator=(const ShaderPermutation& rhs);
		ShaderPermutation& operator=(ShaderPermutation&& rhs) noexcept;

		// Create a new Permutation with current defines plus the specified defines.
		ShaderPermutation Append(std::string_view newDefines) const;
		// Create a new Permutation with current defines plus the specified defines.
		ShaderPermutation Append(std::initializer_list<std::string_view> newDefines) const;

		// Return all defines.
		const std::vector<std::string_view>& Defines() const noexcept { return defines; }
		// Return the combined hash of all defines.
		size_t Hash() const noexcept { return hash; }

		// Returns the name part from a define with value, separated by '=' character.
		static std::string_view NamePart(std::string_view define) noexcept;
		// Returns the value part from a define with value, separated by '=' character.
		static std::string_view ValuePart(std::string_view define) noexcept;

	private:
		// Split the defines by space or semicolon from the source string.
		void Process();
		// Check if a define is present.
		// If present and outIndex is not nullptr, outIndex will be set to the index in the defines vector.
		bool Exists(std::string_view define, size_t* outIndex = nullptr) const;
		// Add a single define.
		// Overwrites if already defined.
		void Add(std::string_view define);
		// Update the combined hash of all defines.
		void UpdateHash();

	private:
		// The source string containing all defines.
		std::string buffer;
		// Individual defines.
		std::vector<std::string_view> defines;
		// Combined hash of all defines.
		size_t hash;
	};

	// ==========================================================================================
	inline ShaderPermutation ShaderPermutation::Append(std::string_view newDefines) const
	{
		std::string new_buffer;
		new_buffer.reserve(buffer.length() + newDefines.length() + 1);
		new_buffer.append(buffer);
		new_buffer.push_back(';');
		new_buffer.append(newDefines);

		return ShaderPermutation {std::move(new_buffer)};
	}

	inline ShaderPermutation ShaderPermutation::Append(std::initializer_list<std::string_view> newDefines) const
	{
		size_t length = buffer.length();
		for (auto it = newDefines.begin(); it != newDefines.end(); ++it) {
			length += it->length() + 1;
		}

		std::string new_buffer;
		new_buffer.reserve(length);
		new_buffer.append(buffer);
		for (auto it = newDefines.begin(); it != newDefines.end(); ++it) {
			new_buffer.push_back(';');
			new_buffer.append(*it);
		}

		return ShaderPermutation {std::move(new_buffer)};
	}
}

template <>
struct std::hash<Turso3D::ShaderPermutation>
{
	size_t operator()(const Turso3D::ShaderPermutation& p) const noexcept
	{
		return p.Hash();
	}
};
