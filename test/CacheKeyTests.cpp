#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "CacheKey.h"

using InventoryInjectorImproved::CacheSetter;
using InventoryInjectorImproved::DecodeCacheKey;
using InventoryInjectorImproved::IsDynamicForm;
using InventoryInjectorImproved::MakeCacheKey;

TEST_CASE("IsDynamicForm detects 0xFF runtime forms only", "[cachekey]")
{
	CHECK(IsDynamicForm(0xFF000000));
	CHECK(IsDynamicForm(0xFF123456));
	CHECK(IsDynamicForm(0xFFFFFFFF));

	CHECK_FALSE(IsDynamicForm(0x00000014));  // player
	CHECK_FALSE(IsDynamicForm(0x0002E4E2));  // a normal base form
	CHECK_FALSE(IsDynamicForm(0x01000800));  // plugin index 0x01
	CHECK_FALSE(IsDynamicForm(0xFE012345));  // ESL / light plugin (0xFE) is NOT dynamic
}

TEST_CASE("MakeCacheKey: a normal item keys on its formId alone", "[cachekey]")
{
	CHECK(MakeCacheKey(0x0002E4E2, false, 0, CacheSetter::kInventory) == 0x0002E4E2ULL);
	CHECK(MakeCacheKey(0x00012345, false, 0, CacheSetter::kInventory) == 0x00012345ULL);
	CHECK(MakeCacheKey(0x00012345, false, 7, CacheSetter::kInventory) == 0x00012345ULL);
}

TEST_CASE("MakeCacheKey: formId occupies only the low 32 bits for normal items", "[cachekey]")
{
	const auto key = MakeCacheKey(0xFEFFFFFF, false, 0, CacheSetter::kInventory);
	CHECK((key & 0xFFFFFFFFULL) == 0xFEFFFFFFULL);
	CHECK((key >> 32) == 0);
}

TEST_CASE("MakeCacheKey: a soul gem yields a distinct key per fill-status", "[cachekey]")
{
	constexpr std::uint32_t gem = 0x0002E4E2;

	const auto empty = MakeCacheKey(gem, true, 0, CacheSetter::kInventory);
	const auto partial = MakeCacheKey(gem, true, 1, CacheSetter::kInventory);
	const auto full = MakeCacheKey(gem, true, 2, CacheSetter::kInventory);

	CHECK(empty != partial);
	CHECK(partial != full);
	CHECK(empty != full);

	CHECK((empty & 0xFFFFFFFFULL) == gem);
	CHECK((full & 0xFFFFFFFFULL) == gem);
}

TEST_CASE("MakeCacheKey: a soul gem never collides with a normal item sharing the formId",
	"[cachekey]")
{
	constexpr std::uint32_t id = 0x12345678;

	CHECK(MakeCacheKey(id, true, 0, CacheSetter::kInventory) != MakeCacheKey(id, false, 0, CacheSetter::kInventory));
	CHECK(MakeCacheKey(id, true, 1, CacheSetter::kInventory) != MakeCacheKey(id, false, 0, CacheSetter::kInventory));
	CHECK(MakeCacheKey(id, true, 2, CacheSetter::kInventory) != MakeCacheKey(id, false, 0, CacheSetter::kInventory));
}

TEST_CASE("DecodeCacheKey inverts MakeCacheKey for a normal item", "[cachekey]")
{
	const auto d = DecodeCacheKey(MakeCacheKey(0x0002E4E2, false, 0, CacheSetter::kInventory));
	CHECK(d.formID == 0x0002E4E2u);
	CHECK_FALSE(d.soulGem);
	CHECK(d.status == 0u);
}

TEST_CASE("DecodeCacheKey inverts MakeCacheKey for a soul gem with status", "[cachekey]")
{
	const auto d = DecodeCacheKey(MakeCacheKey(0x12345678, true, 2, CacheSetter::kInventory));
	CHECK(d.formID == 0x12345678u);
	CHECK(d.soulGem);
	CHECK(d.status == 2u);
}

TEST_CASE("DecodeCacheKey preserves a full 0xFE light-plugin formId", "[cachekey]")
{
	const auto d = DecodeCacheKey(MakeCacheKey(0xFEFFFFFF, false, 0, CacheSetter::kInventory));
	CHECK(d.formID == 0xFEFFFFFFu);
	CHECK_FALSE(d.soulGem);
}

TEST_CASE("MakeCacheKey: different setters never collide for the same formId/soulGem/status", "[cachekey]")
{
	constexpr std::uint32_t formID = 0x0002E4E2;

	const auto inv = MakeCacheKey(formID, false, 0, CacheSetter::kInventory);
	const auto craft = MakeCacheKey(formID, false, 0, CacheSetter::kCrafting);
	const auto magic = MakeCacheKey(formID, false, 0, CacheSetter::kMagic);

	CHECK(inv != craft);
	CHECK(craft != magic);
	CHECK(inv != magic);

	// The formId payload itself is unchanged by the setter.
	CHECK((inv & 0xFFFFFFFFULL) == formID);
	CHECK((craft & 0xFFFFFFFFULL) == formID);
	CHECK((magic & 0xFFFFFFFFULL) == formID);
}

TEST_CASE("DecodeCacheKey round-trips the setter", "[cachekey]")
{
	const auto d = DecodeCacheKey(MakeCacheKey(0x12345678, true, 2, CacheSetter::kCrafting));
	CHECK(d.formID == 0x12345678u);
	CHECK(d.soulGem);
	CHECK(d.status == 2u);
	CHECK(d.setter == CacheSetter::kCrafting);
}
