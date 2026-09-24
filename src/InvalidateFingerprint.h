#pragma once

#include "ConfigHash.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace InventoryInjectorImproved
{
	/**
	 * One itemList entry, pre-coerced from GFxValue to plain integers/bool.
	 * filterFlag is excluded from hashing: it's a processing-derived category
	 * flag SkyUI refines pass-to-pass. processed IS hashed: it's the engine's
	 * own "needs reprocessing" signal, so a pending entry must change the
	 * fingerprint even when its other fields haven't moved yet.
	 */
	struct FpEntry
	{
		std::uint64_t formId{ 0 };
		std::uint64_t count{ 0 };
		std::uint64_t filterFlag{ 0 };
		std::uint64_t flags{ 0 };
		std::uint64_t equipState{ 0 };
		bool          processed{ false };
	};

	// Filter-chain config that determines the enumeration output.
	struct FpConfig
	{
		std::uint64_t             itemFilter{ 0 };
		std::string               filterText;
		std::vector<std::string>  sortAttributes;
		std::vector<std::int64_t> sortOptions;
	};

	namespace detail
	{
		inline std::uint64_t FnvU64(std::uint64_t a_h, std::uint64_t a_v)
		{
			a_h = FnvU32(a_h, static_cast<std::uint32_t>(a_v & 0xFFFFFFFFULL));
			return FnvU32(a_h, static_cast<std::uint32_t>(a_v >> 32));
		}
	}

	/**
	 * Order-insensitive fingerprint over the entry content multiset, mixed with an
	 * order-sensitive fold of entry count and filter config. Entries combine via an
	 * additive sum of per-entry hashes so a pure reorder (which SkyUI does every
	 * InvalidateData) hashes equal. Weaker than XOR/multiplicative folding (two
	 * entries could swap content and sum unchanged); accepted as negligible for
	 * organic gameplay data.
	 */
	[[nodiscard]] inline std::uint64_t InvalidateFingerprint(
		std::span<const FpEntry> a_entries, const FpConfig& a_config)
	{
		std::uint64_t base = detail::kFnvOffset;
		base = detail::FnvU32(base, static_cast<std::uint32_t>(a_entries.size()));

		base = detail::FnvU64(base, a_config.itemFilter);
		base = detail::FnvU32(base, static_cast<std::uint32_t>(a_config.filterText.size()));
		base = detail::FnvBytes(base, std::as_bytes(std::span{ a_config.filterText.data(), a_config.filterText.size() }));

		base = detail::FnvU32(base, static_cast<std::uint32_t>(a_config.sortAttributes.size()));
		for (const auto& attr : a_config.sortAttributes) {
			base = detail::FnvU32(base, static_cast<std::uint32_t>(attr.size()));
			base = detail::FnvBytes(base, std::as_bytes(std::span{ attr.data(), attr.size() }));
		}

		base = detail::FnvU32(base, static_cast<std::uint32_t>(a_config.sortOptions.size()));
		for (const auto opt : a_config.sortOptions) {
			base = detail::FnvU64(base, static_cast<std::uint64_t>(opt));
		}

		std::uint64_t sum = 0;
		for (const auto& e : a_entries) {
			std::uint64_t h = detail::FnvU64(detail::kFnvOffset, e.formId);
			h = detail::FnvU64(h, e.count);
			h = detail::FnvU64(h, e.flags);
			h = detail::FnvU64(h, e.equipState);
			h = detail::FnvU32(h, e.processed ? 1U : 0U);
			sum += h;
		}
		return base ^ sum;
	}

	/**
	 * Count of entries that still need their display data built: not yet processed
	 * and processable (filterFlag != 0; SkyUI's ItemcardDataExtender guard never
	 * processes filterFlag == 0 entries such as gold, so they never count).
	 */
	[[nodiscard]] inline std::size_t UnrenderedCount(std::span<const FpEntry> a_entries)
	{
		return static_cast<std::size_t>(std::ranges::count_if(a_entries, [](const FpEntry& e) {
			return !e.processed && e.filterFlag != 0;
		}));
	}

	/**
	 * Gate for skipping a reprocess: true only when nothing is left pending. A
	 * fresh native rebuild leaves every renderable entry unprocessed, so it is
	 * never mistaken for a fully-settled list.
	 */
	[[nodiscard]] inline bool ListAlreadyRendered(std::span<const FpEntry> a_entries)
	{
		return UnrenderedCount(a_entries) == 0;
	}
}
