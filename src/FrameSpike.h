#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace InventoryInjectorImproved
{
	/**
	 * Fixed-capacity ring of recent frame durations (ms). Snapshot is oldest-to-newest.
	 */
	class FrameRing
	{
	public:
		explicit FrameRing(std::size_t a_cap) :
			_buf(a_cap == 0 ? 1 : a_cap)
		{}

		void Push(double a_ms)
		{
			_buf[_head] = a_ms;
			_head = (_head + 1) % _buf.size();
			if (_count < _buf.size()) {
				++_count;
			}
		}

		void Clear()
		{
			_head = 0;
			_count = 0;
		}

		[[nodiscard]] std::size_t Size() const { return _count; }

		[[nodiscard]] std::vector<double> Snapshot() const
		{
			std::vector<double> out;
			out.reserve(_count);
			const std::size_t start = (_head + _buf.size() - _count) % _buf.size();
			for (std::size_t i = 0; i < _count; ++i) {
				out.push_back(_buf[(start + i) % _buf.size()]);
			}
			return out;
		}

	private:
		std::vector<double> _buf;
		std::size_t         _head{ 0 };
		std::size_t         _count{ 0 };
	};

	struct SpikeResult
	{
		double      baselineMs{ 0.0 };
		double      spikeMs{ 0.0 };
		double      peakMs{ 0.0 };
		std::size_t peakIndex{ 0 };
	};

	/**
	 * Peak-vs-baseline read of a frame-duration window. peak is the slowest frame;
	 * baseline is the median of the remaining frames; spike is peak - baseline.
	 * peakIndex marks the peak frame within the window so a caller can align a
	 * per-frame side measurement to it (robust to the one-frame lag between a
	 * rebuild and its measured duration, since the slow frame is always the peak).
	 */
	[[nodiscard]] inline SpikeResult ComputeSpike(std::span<const double> a_frames)
	{
		if (a_frames.empty()) {
			return {};
		}

		std::vector<double> rest(a_frames.begin(), a_frames.end());
		const auto          peakIt = std::ranges::max_element(rest);
		const auto          peakIndex = static_cast<std::size_t>(std::ranges::distance(rest.begin(), peakIt));
		const double        peak = *peakIt;
		rest.erase(peakIt);

		double baseline = 0.0;
		if (!rest.empty()) {
			std::ranges::sort(rest);
			const std::size_t mid = rest.size() / 2;
			baseline = (rest.size() % 2 == 0) ? (rest[mid - 1] + rest[mid]) / 2.0 : rest[mid];
		}

		return { .baselineMs = baseline, .spikeMs = peak - baseline, .peakMs = peak, .peakIndex = peakIndex };
	}

	struct SplitResult
	{
		double nativeMs{ 0.0 };
		double flashMs{ 0.0 };
	};

	/**
	 * Split a spike into its native and Flash halves. native is the measured native
	 * re-enumeration time for the peak frame (clamped non-negative); flash is the
	 * remainder of the spike (clamped non-negative), i.e. the inferred ActionScript
	 * render cost.
	 */
	[[nodiscard]] inline SplitResult ComputeSplit(double a_spikeMs, double a_nativeMsAtPeak)
	{
		const double native = std::max<double>(0.0, a_nativeMsAtPeak);
		return { .nativeMs = native, .flashMs = std::max<double>(0.0, a_spikeMs - native) };
	}

	struct InvalidateStat
	{
		std::uint32_t count{ 0 };
		double        totalMs{ 0.0 };
	};

	/**
	 * Fold a window's per-frame InvalidateListData call counts and durations into a
	 * total call count and total milliseconds. The two spans are parallel per-frame
	 * series; a length mismatch folds only the overlapping prefix.
	 */
	[[nodiscard]] inline InvalidateStat ComputeInvalidate(
		std::span<const double> a_countsPerFrame, std::span<const double> a_msPerFrame)
	{
		InvalidateStat    out;
		const std::size_t n = std::min<std::size_t>(a_countsPerFrame.size(), a_msPerFrame.size());
		for (std::size_t i = 0; i < n; ++i) {
			out.count += static_cast<std::uint32_t>(a_countsPerFrame[i]);
			out.totalMs += a_msPerFrame[i];
		}
		return out;
	}
}
