#pragma once

namespace RE
{
	class GFxMovieView;
}

namespace InventoryInjectorImproved::InvalidateMemo
{
	/**
	 * Wrap itemList.InvalidateData so a reprocess whose content+config fingerprint
	 * matches the last processed one returns without calling the original, skipping
	 * the redundant deferred commitInvalidate refresh. No-op on non-inventory movies.
	 */
	void Install(RE::GFxMovieView* a_view);

	/**
	 * Runtime toggle for same-session before/after measurement. Default enabled.
	 * When disabled the wrap always calls the original (vanilla behaviour).
	 */
	void SetEnabled(bool a_on);
}
