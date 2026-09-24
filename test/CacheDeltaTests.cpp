#include <catch2/catch_test_macros.hpp>

#include "CacheDelta.h"

using InventoryInjectorImproved::CachedField;
using InventoryInjectorImproved::CacheEntry;

TEST_CASE("CachedField defaults to a null field", "[cachedelta]")
{
	CachedField f;
	CHECK(f.kind == CachedField::Kind::kNull);
	CHECK(f.name.empty());
}

TEST_CASE("CacheEntry holds a decoded entry and its delta", "[cachedelta]")
{
	CacheEntry e{ .formID = 0x14, .soulGem = false, .status = 0, .delta = {} };
	e.delta.push_back(CachedField{ .name = "iconLabel", .kind = CachedField::Kind::kString, .str = "Sword" });
	CHECK(e.formID == 0x14u);
	REQUIRE(e.delta.size() == 1);
	CHECK(e.delta.front().str == "Sword");
}
