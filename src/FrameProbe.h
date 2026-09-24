#pragma once

#include <cstdint>
#include <utility>

#ifdef I5_FRAME_PROBE
#	include <chrono>

#	include <spdlog/spdlog.h>
#endif

namespace InventoryInjectorImproved::FrameProbe
{
#ifdef I5_FRAME_PROBE
	/**
	 * Install the per-frame main-update hook once. The caller must already have reserved
	 * the trampoline budget in TrampolineBudget.h through a single SKSE::AllocTrampoline
	 * call. Safe to call repeatedly.
	 */
	void Install();

	/**
	 * Signal that a list rebuild landed this frame, tagging the pending dump with the
	 * menu and entry count. No-op unless debug logging is on. Called from the
	 * processList hook on the main thread.
	 */
	void NoteRefresh(const char* a_menu, std::uint32_t a_count);

	namespace detail
	{
		/**
		 * Add one InventoryLists.InvalidateListData call and its duration (microseconds) to
		 * the current frame's tally, later folded across the transfer window and printed with
		 * the spike. Called from TimeInvalidateListData on the main thread.
		 */
		void NoteInvalidateListData(std::int64_t a_microseconds);
	}

	/**
	 * Timing is skipped unless debug logging is on.
	 */
	template <class F>
	void TimeInvalidateListData(F&& a_fn)
	{
		if (!spdlog::should_log(spdlog::level::debug)) {
			std::forward<F>(a_fn)();
			return;
		}
		const auto t0 = std::chrono::steady_clock::now();
		std::forward<F>(a_fn)();
		const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
		detail::NoteInvalidateListData(us);
	}
#else
	inline void Install() {}
	inline void NoteRefresh([[maybe_unused]] const char* a_menu, [[maybe_unused]] std::uint32_t a_count) {}

	template <class F>
	void TimeInvalidateListData(F&& a_fn)
	{
		std::forward<F>(a_fn)();
	}
#endif
}
