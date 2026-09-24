#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "FrameSpike.h"

#include <vector>

using Catch::Matchers::WithinAbs;
using InventoryInjectorImproved::ComputeSpike;
using InventoryInjectorImproved::FrameRing;

TEST_CASE("FrameRing keeps last N in oldest-to-newest order", "[frameprobe]")
{
	FrameRing ring(3);
	ring.Push(1.0);
	ring.Push(2.0);
	CHECK(ring.Snapshot() == std::vector<double>{ 1.0, 2.0 });

	ring.Push(3.0);
	ring.Push(4.0);  // evicts 1.0
	CHECK(ring.Size() == 3);
	CHECK(ring.Snapshot() == std::vector<double>{ 2.0, 3.0, 4.0 });
}

TEST_CASE("FrameRing::Clear empties the ring and it is reusable", "[frameprobe]")
{
	FrameRing ring(4);
	ring.Push(1.0);
	ring.Push(2.0);
	ring.Clear();
	CHECK(ring.Size() == 0);
	CHECK(ring.Snapshot().empty());

	ring.Push(5.0);  // reuse after clear starts fresh
	CHECK(ring.Snapshot() == std::vector<double>{ 5.0 });
}

TEST_CASE("ComputeSpike isolates the peak frame against a baseline", "[frameprobe]")
{
	const std::vector<double> window{ 16.7, 16.9, 41.2, 17.1, 16.8 };
	const auto                r = ComputeSpike(window);
	CHECK_THAT(r.peakMs, WithinAbs(41.2, 0.001));
	CHECK_THAT(r.baselineMs, WithinAbs(16.85, 0.001));  // median of {16.7,16.8,16.9,17.1}
	CHECK_THAT(r.spikeMs, WithinAbs(24.35, 0.001));
}

TEST_CASE("ComputeSpike reports no spike on a flat window", "[frameprobe]")
{
	const std::vector<double> window{ 16.7, 16.7, 16.7, 16.7 };
	const auto                r = ComputeSpike(window);
	CHECK_THAT(r.spikeMs, WithinAbs(0.0, 0.001));
}

TEST_CASE("ComputeSpike handles empty and single-element windows", "[frameprobe]")
{
	const auto empty = ComputeSpike(std::span<const double>{});
	CHECK(empty.peakMs == 0.0);
	CHECK(empty.baselineMs == 0.0);
	CHECK(empty.spikeMs == 0.0);

	const std::vector<double> one{ 20.0 };
	const auto                r = ComputeSpike(one);
	CHECK_THAT(r.peakMs, WithinAbs(20.0, 0.001));
	CHECK_THAT(r.baselineMs, WithinAbs(0.0, 0.001));
	CHECK_THAT(r.spikeMs, WithinAbs(20.0, 0.001));
}

TEST_CASE("ComputeSpike reports the peak frame index", "[frameprobe]")
{
	const std::vector<double> window{ 16.7, 16.9, 41.2, 17.1, 16.8 };
	const auto                r = ComputeSpike(window);
	CHECK(r.peakIndex == 2);
}

TEST_CASE("ComputeSplit derives flash by subtracting native from the spike", "[frameprobe]")
{
	const auto s = InventoryInjectorImproved::ComputeSplit(12.6, 8.9);
	CHECK_THAT(s.nativeMs, WithinAbs(8.9, 0.001));
	CHECK_THAT(s.flashMs, WithinAbs(3.7, 0.001));
}

TEST_CASE("ComputeSplit clamps flash and native at zero", "[frameprobe]")
{
	const auto over = InventoryInjectorImproved::ComputeSplit(5.0, 8.0);  // native exceeds spike
	CHECK_THAT(over.nativeMs, WithinAbs(8.0, 0.001));
	CHECK_THAT(over.flashMs, WithinAbs(0.0, 0.001));

	const auto neg = InventoryInjectorImproved::ComputeSplit(5.0, -1.0);  // guard a bad native value
	CHECK_THAT(neg.nativeMs, WithinAbs(0.0, 0.001));
	CHECK_THAT(neg.flashMs, WithinAbs(5.0, 0.001));
}

TEST_CASE("ComputeInvalidate sums call counts and durations across a window", "[frameprobe]")
{
	const std::vector<double> counts{ 0.0, 2.0, 0.0, 1.0 };
	const std::vector<double> ms{ 0.0, 3.0, 0.0, 1.5 };
	const auto                r = InventoryInjectorImproved::ComputeInvalidate(counts, ms);
	CHECK(r.count == 3);
	CHECK_THAT(r.totalMs, WithinAbs(4.5, 0.001));
}

TEST_CASE("ComputeInvalidate on empty windows is zero", "[frameprobe]")
{
	const auto r = InventoryInjectorImproved::ComputeInvalidate(std::span<const double>{}, std::span<const double>{});
	CHECK(r.count == 0);
	CHECK_THAT(r.totalMs, WithinAbs(0.0, 0.001));
}

TEST_CASE("ComputeInvalidate folds only the overlapping prefix on mismatched lengths", "[frameprobe]")
{
	const std::vector<double> counts{ 1.0, 1.0, 1.0 };
	const std::vector<double> ms{ 2.0, 4.0 };  // shorter
	const auto                r = InventoryInjectorImproved::ComputeInvalidate(counts, ms);
	CHECK(r.count == 2);
	CHECK_THAT(r.totalMs, WithinAbs(6.0, 0.001));
}
