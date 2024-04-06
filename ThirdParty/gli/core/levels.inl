#include <glm/gtc/integer.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

namespace gli
{
	template <glm::length_t L, typename T, glm::qualifier Q>
	inline T levels(glm::vec<L, T, Q> const& Extent)
	{
		return glm::log2(glm::compMax(Extent)) + static_cast<T>(1);
	}

	template <typename T>
	inline T levels(T Extent)
	{
		return static_cast<T>(glm::log2(Extent) + static_cast<size_t>(1));
	}
/*
	inline int levels(int Extent)
	{
		return glm::log2(Extent) + static_cast<int>(1);
	}
*/
}//namespace gli
