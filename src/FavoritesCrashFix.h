#pragma once

namespace InventoryInjectorImproved::FavoritesCrashFix
{
	/**
	 * Hook FavoritesMenu::ProcessMessage so that, on show, FavoritesIconSetter.processList is
	 * wrapped to pre-set `_noIconColors` on the setter. I4 1.1.1 reads SkyrimVM with the 1.7.99
	 * layout on every runtime and crashes in its own lookup of that value; with the member set, I4
	 * skips the lookup. No-op on 1.7.99 and later, where I4 reads SkyrimVM correctly. Safe to call
	 * repeatedly.
	 */
	void Install();
}
