#include "PCH.h"

#include "FrameProbe.h"

#include "FrameSpike.h"

#include <array>
#include <chrono>

#include <fmt/ranges.h>

namespace InventoryInjectorImproved::FrameProbe
{
	namespace
	{
		using clock = std::chrono::steady_clock;
		using UpdateFn = void(RE::Main*, float);
		using ItemListUpdateFn = void(RE::ItemList*, RE::TESObjectREFR*);

		constexpr std::size_t kWindow = 16;     // frames retained around a transfer
		constexpr int         kPostFrames = 4;  // dump this many frames after a rebuild

		/**
		 * Module offsets of the seven direct `call ItemList::Update_Impl` sites on AE
		 * 1.6.1170, reverse-engineered from the (Steamless-decrypted) binary. Update_Impl
		 * has no entry seam CommonLib's trampoline can safely detour (write_branch clobbers
		 * a function head), so each call site is redirected instead. Version-specific;
		 * install self-validates each site (see InstallNativeUpdateHooks), so other builds
		 * simply skip the native split rather than mis-patch.
		 */
		constexpr std::array<std::uintptr_t, 7> kUpdateImplCallSites{
			0x8fb99c, 0x8fd9bd, 0x92cd6f, 0x92daef, 0x92e2bb, 0x92f601, 0x92f835
		};

		/**
		 * The trampolined original main-update, behind a getter to avoid a mutable global.
		 */
		REL::Relocation<UpdateFn>& OriginalUpdate()
		{
			static REL::Relocation<UpdateFn> original;
			return original;
		}

		/**
		 * The original ItemList::Update_Impl, resolved from the patched call sites (they
		 * all target the same function). Behind a getter to avoid a mutable global.
		 */
		REL::Relocation<ItemListUpdateFn>& OriginalUpdateImpl()
		{
			static REL::Relocation<ItemListUpdateFn> original;
			return original;
		}

		struct ProbeState
		{
			FrameRing         ring{ kWindow };
			FrameRing         nativeRing{ kWindow };  // native-update ms, aligned frame-for-frame with ring
			clock::time_point lastFrame;              // default-constructs to the clock epoch (zero); first-frame sentinel
			bool              pending{ false };
			int               framesAfter{ 0 };
			const char*       menu{ "" };
			std::uint32_t     count{ 0 };
			std::int64_t      nativeUsThisFrame{ 0 };     // accrued by UpdateImpl, flushed each frame
			FrameRing         invldCountRing{ kWindow };  // InvalidateListData calls per frame, aligned with ring
			FrameRing         invldMsRing{ kWindow };     // InvalidateListData ms per frame, aligned with ring
			std::int64_t      invldUsThisFrame{ 0 };      // accrued by detail::NoteInvalidateListData, flushed each frame
			std::uint32_t     invldCountThisFrame{ 0 };   // "
		};

		/**
		 * All probe state, in a function-local static (avoids a mutable global). Touched
		 * only on the main thread: OnFrame from Main::Update, NoteRefresh from the
		 * processList hook, UpdateImpl from the ItemList::Update_Impl detour, and
		 * detail::NoteInvalidateListData from the InvalidateListData GFx wrap, all of which
		 * run there, so no locking is needed.
		 */
		ProbeState& State()
		{
			static ProbeState state;
			return state;
		}

		/**
		 * Record this frame's duration; a few frames after a rebuild, dump its window.
		 */
		void OnFrame()
		{
			auto& s = State();

			// Near-zero when off: drop bookkeeping and clear stale window state so a re-enable starts clean.
			if (!spdlog::should_log(spdlog::level::debug)) {
				s.pending = false;
				s.framesAfter = 0;
				s.lastFrame = clock::time_point{};
				s.nativeUsThisFrame = 0;
				s.invldUsThisFrame = 0;
				s.invldCountThisFrame = 0;
				s.ring.Clear();
				s.nativeRing.Clear();
				s.invldCountRing.Clear();
				s.invldMsRing.Clear();
				return;
			}

			const auto now = clock::now();
			if (s.lastFrame.time_since_epoch().count() != 0) {
				const double ms = std::chrono::duration<double, std::milli>(now - s.lastFrame).count();
				s.ring.Push(ms);
				s.nativeRing.Push(static_cast<double>(s.nativeUsThisFrame) / 1000.0);
				s.invldCountRing.Push(static_cast<double>(s.invldCountThisFrame));
				s.invldMsRing.Push(static_cast<double>(s.invldUsThisFrame) / 1000.0);
			}
			s.nativeUsThisFrame = 0;
			s.invldUsThisFrame = 0;
			s.invldCountThisFrame = 0;
			s.lastFrame = now;

			if (!s.pending) {
				return;
			}
			if (++s.framesAfter < kPostFrames) {
				return;
			}

			s.pending = false;
			const auto   window = s.ring.Snapshot();
			const auto   nativeWindow = s.nativeRing.Snapshot();
			const auto   r = ComputeSpike(window);
			const double nativeMs = r.peakIndex < nativeWindow.size() ? nativeWindow[r.peakIndex] : 0.0;
			const auto   split = ComputeSplit(r.spikeMs, nativeMs);
			const auto   invld = ComputeInvalidate(s.invldCountRing.Snapshot(), s.invldMsRing.Snapshot());
			logger::debug(
				"I4 frameprobe [{}] N={} | window ms: {:.1f} | peak {:.1f}ms baseline {:.1f}ms spike +{:.1f}ms | native {:.1f}ms flash {:.1f}ms | invld {}x {:.1f}ms",
				s.menu, s.count, fmt::join(window, " "), r.peakMs, r.baselineMs, r.spikeMs, split.nativeMs, split.flashMs, invld.count, invld.totalMs);
		}

		void Update(RE::Main* a_main, float a_delta)
		{
			OriginalUpdate()(a_main, a_delta);
			OnFrame();
		}

		/**
		 * Call-site detour for ItemList::Update_Impl: time the native rebuild and add it to
		 * this frame's tally. Always calls through, so inventory behaviour is unchanged.
		 */
		void UpdateImpl(RE::ItemList* a_this, RE::TESObjectREFR* a_owner)
		{
			if (!spdlog::should_log(spdlog::level::debug)) {
				OriginalUpdateImpl()(a_this, a_owner);
				return;
			}
			const auto t0 = clock::now();
			OriginalUpdateImpl()(a_this, a_owner);
			const auto us = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
			State().nativeUsThisFrame += us;
		}

		/**
		 * Redirect the ItemList::Update_Impl call sites to the timing detour. Gated on the
		 * exact game version the offsets were reverse-engineered against (AE 1.6.1170): on
		 * any other build nothing is read or patched, so the native split is simply absent
		 * rather than dereferencing an unmapped address. Within that version each site is
		 * still self-validated (a `call rel32` whose target resolves to Update_Impl) to
		 * catch an offset transcription error before patching.
		 */
		void InstallNativeUpdateHooks(SKSE::Trampoline& a_trampoline)
		{
			if (REL::Module::get().version() != REL::Version(1, 6, 1170, 0)) {
				logger::info("FrameProbe: native split unavailable (mapped for AE 1.6.1170 only)");
				return;
			}

			const auto target = REL::Relocation<std::uintptr_t>{ REL::RelocationID(50099, 51031) }.address();
			OriginalUpdateImpl() = target;  // all validated sites resolve to this same function

			int hooked = 0;
			for (const auto offset : kUpdateImplCallSites) {
				const auto address = REL::Offset(offset).address();
				// NOLINTNEXTLINE(performance-no-int-to-ptr): read the code byte at a runtime-resolved address to validate the site
				if (*reinterpret_cast<const std::uint8_t*>(address) != 0xE8) {
					continue;
				}
				// NOLINTNEXTLINE(performance-no-int-to-ptr): read the call's rel32 displacement to confirm its target
				const auto rel = *reinterpret_cast<const std::int32_t*>(address + 1);
				const auto callTarget = address + 5 + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
				if (callTarget != target) {
					continue;
				}
				a_trampoline.write_call<5>(address, UpdateImpl);
				++hooked;
			}
			logger::info("FrameProbe: installed {} of {} ItemList::Update_Impl call-site hooks",
				hooked, kUpdateImplCallSites.size());
		}
	}

	void Install()
	{
		static bool installed = false;
		if (installed) {
			return;
		}
		installed = true;

		auto& trampoline = SKSE::GetTrampoline();

		/**
		 * Main-loop per-frame update. RelocationID(35551, 36544) with the call-site
		 * offset below is the SE/AE main-update seam, verified in-game 2026-07-02: it
		 * ticks every rendered frame including while an inventory/container menu is open.
		 */
		const REL::Relocation<std::uintptr_t> hook{ REL::RelocationID(35551, 36544),
			REL::Relocate(0x11F, 0x160) };
		OriginalUpdate() = trampoline.write_call<5>(hook.address(), Update);
		logger::info("FrameProbe: installed main-update hook");

		InstallNativeUpdateHooks(trampoline);
	}

	void NoteRefresh(const char* a_menu, std::uint32_t a_count)
	{
		if (!spdlog::should_log(spdlog::level::debug)) {
			return;
		}
		auto& s = State();
		s.menu = a_menu;
		s.count = a_count;
		s.pending = true;
		s.framesAfter = 0;
	}

	namespace detail
	{
		void NoteInvalidateListData(std::int64_t a_microseconds)
		{
			auto& s = State();
			s.invldUsThisFrame += a_microseconds;
			s.invldCountThisFrame += 1;
		}
	}
}
