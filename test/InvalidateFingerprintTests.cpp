#include <catch2/catch_test_macros.hpp>

#include "InvalidateFingerprint.h"

#include <cstdint>
#include <string>
#include <vector>

using InventoryInjectorImproved::FpConfig;
using InventoryInjectorImproved::FpEntry;
using InventoryInjectorImproved::InvalidateFingerprint;
using InventoryInjectorImproved::ListAlreadyRendered;
using InventoryInjectorImproved::UnrenderedCount;

namespace
{
	std::vector<FpEntry> SampleEntries()
	{
		return {
			{ .formId = 0x1234, .count = 3, .filterFlag = 0x8, .flags = 0x1, .equipState = 0, .processed = true },
			{ .formId = 0x5678, .count = 1, .filterFlag = 0x4, .flags = 0x0, .equipState = 2, .processed = true },
		};
	}

	FpConfig SampleConfig()
	{
		return { .itemFilter = 0xFFFFFFFF, .filterText = "iron", .sortAttributes = { "name", "value" }, .sortOptions = { 1, 18 } };
	}

	std::uint64_t Fp(const std::vector<FpEntry>& a_e, const FpConfig& a_c)
	{
		return InvalidateFingerprint(a_e, a_c);
	}
}

TEST_CASE("identical content and config hash equal")
{
	REQUIRE(Fp(SampleEntries(), SampleConfig()) == Fp(SampleEntries(), SampleConfig()));
}

TEST_CASE("flipping a hashed content field changes the fingerprint")
{
	const auto base = Fp(SampleEntries(), SampleConfig());

	auto e1 = SampleEntries();
	e1[0].formId = 0x9999;
	REQUIRE(Fp(e1, SampleConfig()) != base);

	auto e2 = SampleEntries();
	e2[0].count = 4;
	REQUIRE(Fp(e2, SampleConfig()) != base);

	auto e4 = SampleEntries();
	e4[1].flags = 0x2;
	REQUIRE(Fp(e4, SampleConfig()) != base);

	auto e5 = SampleEntries();
	e5[0].equipState = 1;
	REQUIRE(Fp(e5, SampleConfig()) != base);
}

TEST_CASE("filterFlag is excluded from the fingerprint")
{
	const auto base = Fp(SampleEntries(), SampleConfig());

	auto ff = SampleEntries();
	ff[1].filterFlag = 0x10;
	REQUIRE(Fp(ff, SampleConfig()) == base);
}

TEST_CASE("flipping processed changes the fingerprint")
{
	const auto base = Fp(SampleEntries(), SampleConfig());

	auto pr = SampleEntries();
	pr[0].processed = false;
	REQUIRE(Fp(pr, SampleConfig()) != base);
}

TEST_CASE("flipping any config field changes the fingerprint")
{
	const auto base = Fp(SampleEntries(), SampleConfig());

	auto c1 = SampleConfig();
	c1.itemFilter = 0x8;
	REQUIRE(Fp(SampleEntries(), c1) != base);

	auto c2 = SampleConfig();
	c2.filterText = "steel";
	REQUIRE(Fp(SampleEntries(), c2) != base);

	auto c3 = SampleConfig();
	c3.sortAttributes[0] = "weight";
	REQUIRE(Fp(SampleEntries(), c3) != base);

	auto c4 = SampleConfig();
	c4.sortOptions[0] = 17;
	REQUIRE(Fp(SampleEntries(), c4) != base);
}

TEST_CASE("entry order is NOT significant")
{
	auto reordered = SampleEntries();
	std::swap(reordered[0], reordered[1]);
	REQUIRE(Fp(reordered, SampleConfig()) == Fp(SampleEntries(), SampleConfig()));
}

TEST_CASE("entry count is significant")
{
	auto one = SampleEntries();
	one.pop_back();
	REQUIRE(Fp(one, SampleConfig()) != Fp(SampleEntries(), SampleConfig()));
}

TEST_CASE("swapping a field between two entries changes the multiset fingerprint")
{
	auto swapped = SampleEntries();
	std::swap(swapped[0].count, swapped[1].count);
	REQUIRE(Fp(swapped, SampleConfig()) != Fp(SampleEntries(), SampleConfig()));
}

TEST_CASE("empty entries and default config are stable")
{
	REQUIRE(InvalidateFingerprint({}, FpConfig{}) == InvalidateFingerprint({}, FpConfig{}));
}

TEST_CASE("UnrenderedCount counts only unprocessed processable entries")
{
	REQUIRE(UnrenderedCount(SampleEntries()) == 0);

	auto goldUnprocessed = SampleEntries();
	goldUnprocessed.push_back({ .formId = 0xF, .count = 100, .filterFlag = 0, .flags = 0, .equipState = 0, .processed = false });
	REQUIRE(UnrenderedCount(goldUnprocessed) == 0);  // filterFlag == 0 never renders

	auto itemUnprocessed = SampleEntries();
	itemUnprocessed[0].processed = false;
	REQUIRE(UnrenderedCount(itemUnprocessed) == 1);

	REQUIRE(UnrenderedCount({}) == 0);
}

TEST_CASE("ListAlreadyRendered requires zero pending entries")
{
	// Fully-rendered list (redundant reprocess): skippable.
	REQUIRE(ListAlreadyRendered(SampleEntries()));

	// Even one pending entry blocks the skip, however large the rest of the list is.
	std::vector<FpEntry> churn(10, FpEntry{ .formId = 1, .count = 1, .filterFlag = 8, .flags = 0, .equipState = 0, .processed = true });
	churn[0].processed = false;
	REQUIRE_FALSE(ListAlreadyRendered(churn));

	// Fresh native rebuild (every renderable entry unprocessed): must not be skipped.
	std::vector<FpEntry> rebuilt(10, FpEntry{ .formId = 1, .count = 1, .filterFlag = 8, .flags = 0, .equipState = 0, .processed = false });
	REQUIRE_FALSE(ListAlreadyRendered(rebuilt));

	// Empty list is trivially rendered.
	REQUIRE(ListAlreadyRendered({}));
}
