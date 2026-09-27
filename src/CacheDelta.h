#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "CacheKey.h"

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
		CacheSetter   setter{ CacheSetter::kInventory };
		Delta         delta;
	};

	/**
	 * True if the delta explicitly sets iconLabel to a value (not deletes it).
	 * A delta missing this is not safe to cache: replaying it on a fresh entry
	 * (which starts with no iconLabel of its own) would leave the item icon-less.
	 */
	[[nodiscard]] inline bool DeltaSetsIconLabel(const Delta& a_delta)
	{
		return std::ranges::any_of(a_delta, [](const CachedField& a_f) {
			return a_f.name == "iconLabel" && a_f.kind != CachedField::Kind::kDelete;
		});
	}
}
