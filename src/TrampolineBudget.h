#pragma once

#include <cstddef>

namespace InventoryInjectorImproved
{
	inline constexpr std::size_t kCallVeneerSize = 14;  // sizeof(TrampolineAssembly) in CommonLibSSE-NG SKSE::Trampoline::write_5branch

	inline constexpr std::size_t kConsoleHookVeneers = 1;

#ifdef I5_FRAME_PROBE
	inline constexpr std::size_t kFrameProbeVeneers = 2;  // budgets the max: main update + UpdateImpl (UpdateImpl hooked only on AE 1.6.1170); CommonLib shares one veneer per destination
#else
	inline constexpr std::size_t kFrameProbeVeneers = 0;
#endif

	inline constexpr std::size_t kTrampolineSize = (kConsoleHookVeneers + kFrameProbeVeneers) * kCallVeneerSize;
}
