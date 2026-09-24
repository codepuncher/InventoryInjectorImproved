#include "PCH.h"

#include "InvalidateMemo.h"

#include "InvalidateFingerprint.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace InventoryInjectorImproved::InvalidateMemo
{
	namespace
	{
		// Main-thread only, behind a getter.
		struct State
		{
			std::optional<std::uint64_t> lastProcessedFp;
			std::uint32_t                skipped{ 0 };
			std::uint32_t                processed{ 0 };
		};

		State& MemoState()
		{
			static State s;
			return s;
		}

		bool& EnabledFlag()
		{
			static bool on = true;
			return on;
		}

		/**
		 * AS ToInt coercion of a GFx value read as an integer field: non-numeric or
		 * non-finite -> 0.
		 */
		std::uint64_t AsUint64(const RE::GFxValue& a_v)
		{
			if (a_v.IsBool()) {
				return a_v.GetBool() ? 1ULL : 0ULL;
			}
			if (!a_v.IsNumber()) {
				return 0;
			}
			const double d = a_v.GetNumber();
			if (!std::isfinite(d)) {
				return 0;
			}
			return static_cast<std::uint64_t>(static_cast<std::int64_t>(d));
		}

		std::uint64_t MemberU64(RE::GFxValue& a_obj, const char* a_name)
		{
			RE::GFxValue v;
			if (!a_obj.GetMember(a_name, &v)) {
				return 0;
			}
			return AsUint64(v);
		}

		FpEntry ReadEntry(RE::GFxValue& a_entry)
		{
			RE::GFxValue processed;
			const bool   hasProcessed = a_entry.GetMember("skyui_itemDataProcessed", &processed);
			return {
				.formId = MemberU64(a_entry, "formId"),
				.count = MemberU64(a_entry, "count"),
				.filterFlag = MemberU64(a_entry, "filterFlag"),
				.flags = MemberU64(a_entry, "flags"),
				.equipState = MemberU64(a_entry, "equipState"),
				.processed = hasProcessed && processed.IsBool() && processed.GetBool(),
			};
		}

		std::vector<FpEntry> ReadEntries(RE::GFxValue& a_itemList)
		{
			std::vector<FpEntry> out;
			RE::GFxValue         entryList;
			if (!a_itemList.GetMember("entryList", &entryList) || !entryList.IsArray()) {
				return out;
			}
			const std::uint32_t n = entryList.GetArraySize();
			out.reserve(n);
			for (std::uint32_t i = 0; i < n; ++i) {
				RE::GFxValue entry;
				if (!entryList.GetElement(i, &entry)) {
					continue;
				}
				out.push_back(ReadEntry(entry));
			}
			return out;
		}

		std::vector<std::string> ReadStringArray(RE::GFxValue& a_arr)
		{
			std::vector<std::string> out;
			if (!a_arr.IsArray()) {
				return out;
			}
			const std::uint32_t n = a_arr.GetArraySize();
			out.reserve(n);
			for (std::uint32_t i = 0; i < n; ++i) {
				RE::GFxValue v;
				if (a_arr.GetElement(i, &v) && v.IsString()) {
					out.emplace_back(v.GetString());
					continue;
				}
				out.emplace_back();
			}
			return out;
		}

		std::vector<std::int64_t> ReadIntArray(RE::GFxValue& a_arr)
		{
			std::vector<std::int64_t> out;
			if (!a_arr.IsArray()) {
				return out;
			}
			const std::uint32_t n = a_arr.GetArraySize();
			out.reserve(n);
			for (std::uint32_t i = 0; i < n; ++i) {
				RE::GFxValue v;
				if (a_arr.GetElement(i, &v)) {
					out.push_back(static_cast<std::int64_t>(AsUint64(v)));
					continue;
				}
				out.push_back(0);
			}
			return out;
		}

		/**
		 * Reads by member, not by filter class, so a filter reorder doesn't matter.
		 * Assumes each field name is unique to one filter class (true upstream).
		 */
		FpConfig ReadConfig(RE::GFxValue& a_itemList)
		{
			FpConfig     cfg;
			RE::GFxValue enumeration;
			if (!a_itemList.GetMember("listEnumeration", &enumeration) || !enumeration.IsObject()) {
				return cfg;
			}
			RE::GFxValue chain;
			if (!enumeration.GetMember("_filterChain", &chain) || !chain.IsArray()) {
				return cfg;
			}
			const std::uint32_t n = chain.GetArraySize();
			for (std::uint32_t i = 0; i < n; ++i) {
				RE::GFxValue filter;
				if (!chain.GetElement(i, &filter) || !filter.IsObject()) {
					continue;
				}
				RE::GFxValue itemFilter;
				if (filter.GetMember("_itemFilter", &itemFilter) && itemFilter.IsNumber()) {
					cfg.itemFilter = AsUint64(itemFilter);
				}
				RE::GFxValue filterText;
				if (filter.GetMember("_filterText", &filterText) && filterText.IsString()) {
					cfg.filterText = filterText.GetString();
				}
				RE::GFxValue sortAttributes;
				if (filter.GetMember("_sortAttributes", &sortAttributes) && sortAttributes.IsArray()) {
					cfg.sortAttributes = ReadStringArray(sortAttributes);
				}
				RE::GFxValue sortOptions;
				if (filter.GetMember("_sortOptions", &sortOptions) && sortOptions.IsArray()) {
					cfg.sortOptions = ReadIntArray(sortOptions);
				}
			}
			return cfg;
		}

		std::uint64_t ComputeFingerprint(RE::GFxValue& a_itemList)
		{
			const std::vector<FpEntry> entries = ReadEntries(a_itemList);
			const FpConfig             config = ReadConfig(a_itemList);
			return InvalidateFingerprint(entries, config);
		}

		void LogTally(const State& a_s)
		{
			if (!spdlog::should_log(spdlog::level::debug)) {
				return;
			}
			logger::debug("InvalidateMemo: skipped {} / {}", a_s.skipped, a_s.skipped + a_s.processed);
		}

		// Debug-only diagnostic; costs nothing at the default log level.
		std::uint64_t CountChecksum(std::span<const FpEntry> a_entries)
		{
			std::uint64_t sum = 0;
			for (const auto& e : a_entries) {
				sum += e.count;
			}
			return sum;
		}

		/**
		 * Wrap for itemList.InvalidateData: skips the original only when the
		 * content+config fingerprint matches the last processed one AND nothing
		 * is left pending.
		 */
		class Handler : public RE::GFxFunctionHandler
		{
		public:
			explicit Handler(RE::GFxValue a_original) :
				_original(std::move(a_original))
			{}

			void Call(Params& a_params) override
			{
				RE::GFxValue& itemList = *a_params.thisPtr;

				if (!EnabledFlag()) {
					InvokeOriginal(a_params);
					return;
				}

				RE::GFxValue suspended;
				if (itemList.GetMember("_bSuspended", &suspended) && suspended.IsBool() && suspended.GetBool()) {
					InvokeOriginal(a_params);
					return;
				}

				/**
				 * ItemMenu.as's RestoreIndices sets a one-shot onInvalidate to restore
				 * scroll/selection after suspend; skipping here would drop it silently.
				 */
				RE::GFxValue onInvalidate;
				if (itemList.GetMember("onInvalidate", &onInvalidate) && !onInvalidate.IsUndefined()) {
					InvokeOriginal(a_params);
					return;
				}

				const std::vector<FpEntry> entries = ReadEntries(itemList);
				const FpConfig             config = ReadConfig(itemList);
				const std::uint64_t        fp = InvalidateFingerprint(entries, config);

				State&     s = MemoState();
				const bool fpMatches = s.lastProcessedFp && fp == *s.lastProcessedFp;
				const bool rendered = ListAlreadyRendered(entries);
				if (fpMatches && rendered) {
					++s.skipped;
					if (spdlog::should_log(spdlog::level::debug)) {
						logger::debug("InvalidateMemo: SKIP fp=0x{:x} entries={} unrendered={} countSum={}",
							fp, entries.size(), UnrenderedCount(entries), CountChecksum(entries));
					}
					LogTally(s);
					return;
				}

				const bool invoked = InvokeOriginal(a_params);
				++s.processed;
				/**
				 * Record the settled post-process fingerprint, not the pre-process one:
				 * data processors mutate hashed fields (e.g. book flags) during the pass.
				 * Skip recording on invoke failure so a failed pass can't look settled.
				 */
				if (invoked) {
					const std::uint64_t postFp = ComputeFingerprint(itemList);
					if (spdlog::should_log(spdlog::level::debug)) {
						logger::debug(
							"InvalidateMemo: PROCESS fp=0x{:x}->0x{:x} entries={} unrendered={} countSum={} "
							"fpMatched={} rendered={}",
							fp, postFp, entries.size(), UnrenderedCount(entries), CountChecksum(entries), fpMatches, rendered);
					}
					s.lastProcessedFp = postFp;
				}
				LogTally(s);
			}

		private:
			// Prepends thisPtr like I4Hook does.
			bool InvokeOriginal(Params& a_params)
			{
				const std::span<RE::GFxValue> args{ a_params.args, a_params.argCount };
				std::vector<RE::GFxValue>     callArgs;
				callArgs.reserve(1 + args.size());
				callArgs.push_back(*a_params.thisPtr);
				callArgs.insert(callArgs.end(), args.begin(), args.end());
				if (_original.Invoke("call", a_params.retVal, callArgs.data(), callArgs.size())) {
					return true;
				}
				logger::warn("InvalidateMemo: original InvalidateData invoke failed (passthrough)");
				return false;
			}

			RE::GFxValue _original;
		};
	}

	void Install(RE::GFxMovieView* a_view)
	{
		if (!a_view) {
			return;
		}

		RE::GFxValue itemList;
		if (!a_view->GetVariable(&itemList, "_level0.Menu_mc.inventoryLists.itemList")) {
			return;  // not an inventory-family menu movie; nothing to wrap
		}

		// Fires once per menu-open event; reset so no skip decision carries across a reopen.
		MemoState() = State{};

		RE::GFxValue marker;
		if (itemList.GetMember("_fu_invalidate_memo", &marker) && marker.IsBool() && marker.GetBool()) {
			return;
		}

		RE::GFxValue original;
		if (!itemList.GetMember("InvalidateData", &original) || !original.IsObject()) {
			logger::warn("InvalidateMemo: itemList.InvalidateData not found");
			return;
		}

		auto         handler = RE::make_gptr<Handler>(std::move(original));
		RE::GFxValue hook;
		a_view->CreateFunction(&hook, handler.get());
		if (!itemList.SetMember("InvalidateData", hook)) {
			logger::warn("InvalidateMemo: failed to replace itemList.InvalidateData");
			return;
		}

		RE::GFxValue done;
		done.SetBoolean(true);
		if (!itemList.SetMember("_fu_invalidate_memo", done)) {
			logger::warn("InvalidateMemo: failed to set install marker (may double-wrap on next Install)");
		}
		logger::info("InvalidateMemo: installed content+config memo on itemList.InvalidateData");
	}

	void SetEnabled(bool a_on)
	{
		EnabledFlag() = a_on;
	}
}
