#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

#include "CacheDelta.h"
#include "CacheSerialization.h"

using InventoryInjectorImproved::CachedField;
using InventoryInjectorImproved::CacheEntry;
using InventoryInjectorImproved::Serial::DeserializeEntries;
using InventoryInjectorImproved::Serial::SerializeEntries;

namespace
{
	std::vector<CacheEntry> Sample()
	{
		CacheEntry normal{ .formID = 0x0002E4E2, .soulGem = false, .status = 0 };
		normal.delta.push_back({ .name = "num", .kind = CachedField::Kind::kNumber, .number = 42.5 });
		normal.delta.push_back({ .name = "txt", .kind = CachedField::Kind::kString, .str = "Iron Sword" });
		normal.delta.push_back({ .name = "flag", .kind = CachedField::Kind::kBool, .boolean = true });
		normal.delta.push_back({ .name = "gone", .kind = CachedField::Kind::kDelete });
		normal.delta.push_back({ .name = "empty", .kind = CachedField::Kind::kNull });
		CachedField kw{ .name = "kw", .kind = CachedField::Kind::kKeywordObj };
		kw.keywords = { "WeapTypeSword", "MagicDisallowEnchanting" };
		normal.delta.push_back(kw);

		CacheEntry gem{ .formID = 0x0002E4E5, .soulGem = true, .status = 2 };
		gem.delta.push_back({ .name = "icon", .kind = CachedField::Kind::kString, .str = "soulgem" });

		return { normal, gem };
	}

	bool FieldEqual(const CachedField& a, const CachedField& b)
	{
		return a.name == b.name && a.kind == b.kind && a.number == b.number &&
		       a.str == b.str && a.boolean == b.boolean && a.keywords == b.keywords;
	}
}

TEST_CASE("SerializeEntries round-trips every field kind", "[serial]")
{
	const auto in = Sample();
	const auto bytes = SerializeEntries(in);
	const auto out = DeserializeEntries(bytes);

	REQUIRE(out.has_value());
	REQUIRE(out->size() == in.size());
	for (std::size_t i = 0; i < in.size(); ++i) {
		CHECK((*out)[i].formID == in[i].formID);
		CHECK((*out)[i].soulGem == in[i].soulGem);
		CHECK((*out)[i].status == in[i].status);
		REQUIRE((*out)[i].delta.size() == in[i].delta.size());
		for (std::size_t j = 0; j < in[i].delta.size(); ++j) {
			CHECK(FieldEqual((*out)[i].delta[j], in[i].delta[j]));
		}
	}
}

TEST_CASE("DeserializeEntries rejects a truncated buffer", "[serial]")
{
	auto bytes = SerializeEntries(Sample());
	bytes.resize(bytes.size() / 2);
	CHECK_FALSE(DeserializeEntries(bytes).has_value());
}

TEST_CASE("DeserializeEntries rejects an unknown field kind", "[serial]")
{
	auto bytes = SerializeEntries(Sample());
	/**
	 * Corrupt the first field's kind byte: layout is u32 count, then first entry's
	 * u32 formID, u8 soulGem, u32 status, u32 fieldCount, then u8 kind.
	 */
	const std::size_t kindOffset = 4 + 4 + 1 + 4 + 4;
	bytes[kindOffset] = static_cast<std::byte>(0x7F);
	CHECK_FALSE(DeserializeEntries(bytes).has_value());
}

TEST_CASE("SerializeEntries handles an empty cache", "[serial]")
{
	const auto out = DeserializeEntries(SerializeEntries({}));
	REQUIRE(out.has_value());
	CHECK(out->empty());
}

TEST_CASE("DeserializeEntries rejects trailing bytes", "[serial]")
{
	auto bytes = SerializeEntries(Sample());
	bytes.push_back(std::byte{ 0 });
	CHECK_FALSE(DeserializeEntries(bytes).has_value());
}

TEST_CASE("DeserializeEntries rejects an inflated count without over-reserving", "[serial]")
{
	/**
	 * A 4-byte buffer claims a huge entry count but carries no entry data; the
	 * reserve is bounded by remaining bytes so no over-allocation occurs, and the
	 * first read fails to nullopt.
	 */
	std::vector<std::byte>  bytes(4);
	constexpr std::uint32_t bigCount = 1'000'000;
	for (int i = 0; i < 4; ++i) {
		bytes[i] = static_cast<std::byte>((bigCount >> (8 * i)) & 0xFF);
	}
	CHECK_FALSE(DeserializeEntries(bytes).has_value());
}
