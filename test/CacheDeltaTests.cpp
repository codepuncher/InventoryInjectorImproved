#include <catch2/catch_test_macros.hpp>

#include "CacheDelta.h"
#include "CacheKey.h"

using InventoryInjectorImproved::CachedField;
using InventoryInjectorImproved::CacheEntry;
using InventoryInjectorImproved::CacheSetter;

TEST_CASE("CachedField defaults to a null field", "[cachedelta]")
{
	CachedField f;
	CHECK(f.kind == CachedField::Kind::kNull);
	CHECK(f.name.empty());
}

TEST_CASE("CacheEntry holds a decoded entry and its delta", "[cachedelta]")
{
	CacheEntry e{ .formID = 0x14, .soulGem = false, .status = 0, .setter = CacheSetter::kInventory, .delta = {} };
	e.delta.push_back(CachedField{ .name = "iconLabel", .kind = CachedField::Kind::kString, .str = "Sword" });
	CHECK(e.formID == 0x14u);
	REQUIRE(e.delta.size() == 1);
	CHECK(e.delta.front().str == "Sword");
}

TEST_CASE("DeltaSetsIconLabel: true only when iconLabel is set to a real value", "[cachedelta]")
{
	using InventoryInjectorImproved::Delta;
	using InventoryInjectorImproved::DeltaSetsIconLabel;

	Delta withLabel;
	withLabel.push_back(CachedField{ .name = "iconLabel", .kind = CachedField::Kind::kString, .str = "Sword" });
	CHECK(DeltaSetsIconLabel(withLabel));

	Delta withOtherFields;
	withOtherFields.push_back(CachedField{ .name = "iconColor", .kind = CachedField::Kind::kNumber, .number = 1.0 });
	CHECK_FALSE(DeltaSetsIconLabel(withOtherFields));

	Delta withDeletedLabel;
	withDeletedLabel.push_back(CachedField{ .name = "iconLabel", .kind = CachedField::Kind::kDelete });
	CHECK_FALSE(DeltaSetsIconLabel(withDeletedLabel));

	Delta empty;
	CHECK_FALSE(DeltaSetsIconLabel(empty));
}
