#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace InventoryInjectorImproved
{
	/**
	 * One category tab's inputs to the tab-emptiness test: its filter bitmask and whether
	 * it is pinned visible (bDontHide) regardless of contents.
	 */
	struct CategoryInput
	{
		std::uint32_t flag;
		bool          bDontHide;
	};

	/**
	 * A category is non-empty iff it is pinned or its flag intersects the combined item mask.
	 * Shared by the fast GFx apply and the pure fold so both use one formula.
	 */
	[[nodiscard]] inline bool CategoryNonEmpty(const CategoryInput& a_c, std::uint32_t a_combined)
	{
		return a_c.bDontHide || ((a_c.flag & a_combined) != 0);
	}

	/**
	 * OR-reduce a list of item filter bitmasks into the combined category-coverage mask.
	 */
	[[nodiscard]] inline std::uint32_t FoldItemMask(std::span<const std::uint32_t> a_itemFlags)
	{
		std::uint32_t combined = 0;
		for (const auto f : a_itemFlags) {
			combined |= f;
		}
		return combined;
	}

	/**
	 * Per-category non-empty flags (0/1) from an already-folded combined mask.
	 */
	[[nodiscard]] inline std::vector<std::uint8_t> CategoryFlagsFromMask(
		std::uint32_t a_combined, std::span<const CategoryInput> a_categories)
	{
		std::vector<std::uint8_t> out;
		out.reserve(a_categories.size());
		for (const auto& c : a_categories) {
			out.push_back(CategoryNonEmpty(c, a_combined) ? std::uint8_t{ 1 } : std::uint8_t{ 0 });
		}
		return out;
	}

	/**
	 * O(N+C) equivalent of SkyUI's tab-emptiness loop: fold item flags, then test each category.
	 */
	[[nodiscard]] inline std::vector<std::uint8_t> ComputeCategoryFlags(
		std::span<const std::uint32_t> a_itemFlags, std::span<const CategoryInput> a_categories)
	{
		return CategoryFlagsFromMask(FoldItemMask(a_itemFlags), a_categories);
	}

	/**
	 * Literal transcription of vanilla InventoryLists.as:254-278 (reset then nested loop),
	 * kept only as the equivalence reference for the host tests.
	 */
	[[nodiscard]] inline std::vector<std::uint8_t> ComputeCategoryFlagsNaive(
		std::span<const std::uint32_t> a_itemFlags, std::span<const CategoryInput> a_categories)
	{
		std::vector<std::uint8_t> out;
		out.reserve(a_categories.size());
		for (const auto& c : a_categories) {
			out.push_back(c.bDontHide ? std::uint8_t{ 1 } : std::uint8_t{ 0 });
		}
		for (const auto item : a_itemFlags) {
			for (std::size_t ci = 0; ci < a_categories.size(); ++ci) {
				if (out[ci] == 0 && (item & a_categories[ci].flag) != 0) {
					out[ci] = 1;
				}
			}
		}
		return out;
	}
}
