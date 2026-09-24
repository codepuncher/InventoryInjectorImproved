#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace InventoryInjectorImproved
{
	struct CachedField
	{
		enum class Kind : std::uint8_t
		{
			kDelete = 0,
			kNumber = 1,
			kString = 2,
			kBool = 3,
			kNull = 4,
			kKeywordObj = 5
		};

		std::string              name;
		Kind                     kind{ Kind::kNull };
		double                   number{ 0.0 };
		std::string              str;
		bool                     boolean{ false };
		std::vector<std::string> keywords;
	};

	using Delta = std::vector<CachedField>;

	struct CacheEntry
	{
		std::uint32_t formID{ 0 };
		bool          soulGem{ false };
		std::uint32_t status{ 0 };
		Delta         delta;
	};
}
