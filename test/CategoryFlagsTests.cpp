#include <catch2/catch_test_macros.hpp>

#include "CategoryFlags.h"

#include <cstdint>
#include <random>
#include <vector>

using InventoryInjectorImproved::CategoryFlagsFromMask;
using InventoryInjectorImproved::CategoryInput;
using InventoryInjectorImproved::ComputeCategoryFlags;
using InventoryInjectorImproved::ComputeCategoryFlagsNaive;
using InventoryInjectorImproved::FoldItemMask;

TEST_CASE("CategoryFlagsFromMask marks a category non-empty iff its flag intersects the mask", "[categoryflags]")
{
	const std::vector<CategoryInput> cats{
		{ .flag = 0x1, .bDontHide = false },
		{ .flag = 0x2, .bDontHide = false },
		{ .flag = 0x8, .bDontHide = false }
	};
	// mask covers bits 0 and 1 but not bit 3
	const auto out = CategoryFlagsFromMask(0x1 | 0x2, cats);
	CHECK(out == std::vector<std::uint8_t>{ 1, 1, 0 });
}

TEST_CASE("bDontHide categories are always non-empty regardless of mask", "[categoryflags]")
{
	const std::vector<CategoryInput> cats{
		{ .flag = 0x4, .bDontHide = true },
		{ .flag = 0x4, .bDontHide = false }
	};
	const auto out = CategoryFlagsFromMask(0x0, cats);  // empty inventory
	CHECK(out == std::vector<std::uint8_t>{ 1, 0 });
}

TEST_CASE("a zero-flag category (divider) is empty unless bDontHide", "[categoryflags]")
{
	const std::vector<CategoryInput> cats{
		{ .flag = 0x0, .bDontHide = false },
		{ .flag = 0x0, .bDontHide = true }
	};
	const auto out = CategoryFlagsFromMask(0xFFFFFFFF, cats);
	CHECK(out == std::vector<std::uint8_t>{ 0, 1 });
}

TEST_CASE("ComputeCategoryFlags folds item flags then matches", "[categoryflags]")
{
	const std::vector<std::uint32_t> items{ 0x1, 0x0, 0x4 };  // a 0 (undefined-as-0) item contributes nothing
	const std::vector<CategoryInput> cats{
		{ .flag = 0x1, .bDontHide = false },
		{ .flag = 0x2, .bDontHide = false },
		{ .flag = 0x4, .bDontHide = false }
	};
	CHECK(FoldItemMask(items) == (0x1u | 0x4u));
	CHECK(ComputeCategoryFlags(items, cats) == std::vector<std::uint8_t>{ 1, 0, 1 });
}

TEST_CASE("ComputeCategoryFlags equals the naive nested loop across cases", "[categoryflags]")
{
	// Hand-picked: empty, single, high bit 31, all-match.
	const std::vector<std::vector<std::uint32_t>> itemSets{
		{},
		{ 0x0 },
		{ 0x80000000 },
		{ 0x1, 0x2, 0x4, 0x8 },
		{ 0xFFFFFFFF }
	};
	const std::vector<CategoryInput> cats{
		{ .flag = 0x1, .bDontHide = false },
		{ .flag = 0x2, .bDontHide = true },
		{ .flag = 0x0, .bDontHide = false },
		{ .flag = 0x80000000, .bDontHide = false },
		{ .flag = 0xF, .bDontHide = false }
	};
	for (const auto& items : itemSets) {
		CHECK(ComputeCategoryFlags(items, cats) == ComputeCategoryFlagsNaive(items, cats));
	}

	// Randomized sweep (fixed seed for reproducibility).
	std::mt19937                                 rng(1234);
	std::uniform_int_distribution<std::uint32_t> flagDist(0, 0xFFFFFFFF);
	std::uniform_int_distribution<int>           countDist(0, 40);
	for (int trial = 0; trial < 500; ++trial) {
		std::vector<std::uint32_t> items(static_cast<std::size_t>(countDist(rng)));
		for (auto& it : items) {
			it = flagDist(rng);
		}
		std::vector<CategoryInput> rcats(static_cast<std::size_t>(countDist(rng) % 16));
		for (auto& c : rcats) {
			c = { .flag = flagDist(rng), .bDontHide = (flagDist(rng) & 1u) != 0 };
		}
		CHECK(ComputeCategoryFlags(items, rcats) == ComputeCategoryFlagsNaive(items, rcats));
	}
}
