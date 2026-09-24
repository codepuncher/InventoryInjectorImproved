#pragma once

namespace RE
{
	class GFxMovieView;
}

namespace InventoryInjectorImproved::InvalidateListFix
{
	/**
	 * Replace InventoryLists.InvalidateListData on this menu's movie with a DLL-side
	 * reimplementation that computes the category tab-emptiness flags in O(N+C) instead of
	 * SkyUI's O(N x categories) loop. Idempotent (marker-guarded); a no-op on a movie without
	 * inventoryLists. Call on menu open, same as I4Hook::Inject.
	 */
	void Install(RE::GFxMovieView* a_view);

	/**
	 * Toggle the in-game equivalence oracle. When on, the handler runs vanilla and asserts our
	 * O(N+C) prediction matches vanilla's category flags (logging divergence) instead of taking
	 * the fast path. Off by default. Main-thread only.
	 */
	void SetVerify(bool a_on);
}
