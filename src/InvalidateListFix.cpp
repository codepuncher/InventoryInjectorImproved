#include "PCH.h"

#include "InvalidateListFix.h"

#include "CategoryFlags.h"
#include "FrameProbe.h"

#include <cmath>
#include <span>

namespace InventoryInjectorImproved::InvalidateListFix
{
	namespace
	{
		/**
		 * The verify-oracle toggle, behind a getter to avoid a mutable global. Main-thread only.
		 */
		bool& VerifyFlag()
		{
			static bool on = false;
			return on;
		}

		/**
		 * AS ToInt32/coercion for a GFx value read as a bitmask: non-numeric or non-finite -> 0.
		 */
		std::uint32_t AsUint32(const RE::GFxValue& a_v)
		{
			if (!a_v.IsNumber()) {
				return 0;
			}
			const double d = a_v.GetNumber();
			if (!std::isfinite(d)) {
				return 0;
			}
			return static_cast<std::uint32_t>(static_cast<std::int64_t>(d));
		}

		/**
		 * OR every entry's filterFlag across an entryList array (undefined/non-numeric -> 0).
		 */
		std::uint32_t FoldEntryFilterFlags(RE::GFxValue& a_entryList)
		{
			std::uint32_t       combined = 0;
			const std::uint32_t n = a_entryList.GetArraySize();
			for (std::uint32_t i = 0; i < n; ++i) {
				RE::GFxValue entry;
				if (!a_entryList.GetElement(i, &entry)) {
					continue;
				}
				RE::GFxValue ff;
				entry.GetMember("filterFlag", &ff);
				combined |= AsUint32(ff);
			}
			return combined;
		}

		/**
		 * Read (flag, bDontHide) for one category entry; undefined bDontHide -> false.
		 */
		CategoryInput ReadCategory(RE::GFxValue& a_entry)
		{
			RE::GFxValue flag;
			a_entry.GetMember("flag", &flag);
			RE::GFxValue bd;
			a_entry.GetMember("bDontHide", &bd);
			return { .flag = AsUint32(flag), .bDontHide = bd.IsBool() && bd.GetBool() };
		}

		/**
		 * DLL-side reimplementation of InventoryLists.InvalidateListData. On the fast path it
		 * replaces the O(N x categories) tab-emptiness loop with an O(N+C) OR-reduction; on any
		 * precondition miss it calls the original (never wrong, at worst vanilla-speed).
		 */
		class Handler : public RE::GFxFunctionHandler
		{
		public:
			Handler(RE::GFxValue a_original, RE::GFxMovieView* a_view) :
				_original(std::move(a_original)),
				_view(a_view)
			{}

			void Call(Params& a_params) override
			{
				if (VerifyFlag()) {
					RunVerify(a_params);
					return;
				}
				FrameProbe::TimeInvalidateListData([&] { RunFast(a_params); });
			}

		private:
			/**
			 * Call the original InvalidateListData through, prepending thisPtr like I4Hook.
			 */
			void InvokeOriginal(Params& a_params)
			{
				const std::span<RE::GFxValue> args{ a_params.args, a_params.argCount };
				std::vector<RE::GFxValue>     callArgs;
				callArgs.reserve(1 + args.size());
				callArgs.push_back(*a_params.thisPtr);
				callArgs.insert(callArgs.end(), args.begin(), args.end());
				if (!_original.Invoke("call", a_params.retVal, callArgs.data(), callArgs.size())) {
					logger::warn("InvalidateListFix: original InvalidateListData invoke failed (passthrough)");
				}
			}

			/**
			 * Up-front gate: everything the fast path needs must exist before any side effect, so
			 * a miss falls back to the original without having mutated state.
			 */
			static bool Preconditions(RE::GFxValue& a_lists, RE::GFxValue& a_itemList, RE::GFxValue& a_categoryList)
			{
				if (!a_lists.GetMember("itemList", &a_itemList) || !a_itemList.IsObject()) {
					return false;
				}
				if (!a_lists.GetMember("categoryList", &a_categoryList) || !a_categoryList.IsObject()) {
					return false;
				}
				RE::GFxValue tmp;
				if (!a_itemList.GetMember("entryList", &tmp) || !tmp.IsArray()) {
					return false;
				}
				if (!a_categoryList.GetMember("entryList", &tmp) || !tmp.IsArray()) {
					return false;
				}
				if (!a_itemList.GetMember("InvalidateData", &tmp) || !tmp.IsObject()) {
					return false;
				}
				if (!a_categoryList.GetMember("UpdateList", &tmp) || !tmp.IsObject()) {
					return false;
				}
				return true;
			}

			/**
			 * Read categoryList.selectedEntry.flag (undefined if no selection).
			 */
			static RE::GFxValue ReadSelectedFlag(RE::GFxValue& a_categoryList)
			{
				RE::GFxValue flag;
				RE::GFxValue selectedEntry;
				if (a_categoryList.GetMember("selectedEntry", &selectedEntry) && selectedEntry.IsObject()) {
					selectedEntry.GetMember("flag", &flag);
				}
				return flag;
			}

			/**
			 * Build {type, index} and dispatch it on the lists object.
			 */
			void Dispatch(RE::GFxValue& a_lists, const char* a_type, const RE::GFxValue& a_index)
			{
				RE::GFxValue evt;
				_view->CreateObject(&evt);
				RE::GFxValue type;
				type.SetString(a_type);
				evt.SetMember("type", type);
				evt.SetMember("index", a_index);
				RE::GFxValue res;
				if (!a_lists.Invoke("dispatchEvent", &res, &evt, 1)) {
					logger::warn("InvalidateListFix: dispatchEvent {} failed", a_type);
				}
			}

			/**
			 * Replicate InvalidateListData's event tail (InventoryLists.as:280-292).
			 */
			void DispatchTail(RE::GFxValue& a_lists, RE::GFxValue& a_itemList, RE::GFxValue& a_categoryList,
				const RE::GFxValue& a_savedFlag)
			{
				RE::GFxValue selectedEntry;
				if (a_categoryList.GetMember("selectedEntry", &selectedEntry) && selectedEntry.IsObject()) {
					RE::GFxValue nowFlag;
					selectedEntry.GetMember("flag", &nowFlag);
					if (AsUint32(a_savedFlag) != AsUint32(nowFlag)) {
						RE::GFxValue typeFilter;
						if (a_lists.GetMember("_typeFilter", &typeFilter) && typeFilter.IsObject()) {
							typeFilter.SetMember("itemFilter", nowFlag);
						}
						RE::GFxValue selIndex;
						a_categoryList.GetMember("selectedIndex", &selIndex);
						Dispatch(a_lists, "categoryChange", selIndex);
					}
				}

				RE::GFxValue itemSelIndex;
				a_itemList.GetMember("selectedIndex", &itemSelIndex);
				if (itemSelIndex.IsNumber() && itemSelIndex.GetNumber() == -1.0) {
					RE::GFxValue negOne;
					negOne.SetNumber(-1.0);
					Dispatch(a_lists, "showItemsList", negOne);
					return;
				}
				Dispatch(a_lists, "itemHighlightChange", itemSelIndex);
			}

			/**
			 * The fast path proper: passthrough InvalidateData, O(N+C) category flags, passthrough
			 * UpdateList, event tail. Preconditions are validated by the caller; no fallback here.
			 */
			void RunFastBody(RE::GFxValue& a_lists, RE::GFxValue& a_itemList, RE::GFxValue& a_categoryList)
			{
				const RE::GFxValue savedFlag = ReadSelectedFlag(a_categoryList);

				RE::GFxValue res;
				if (!a_itemList.Invoke("InvalidateData", &res, nullptr, 0)) {
					logger::warn("InvalidateListFix: itemList.InvalidateData invoke failed");
				}

				RE::GFxValue itemEntryList;
				RE::GFxValue catEntryList;
				a_itemList.GetMember("entryList", &itemEntryList);
				a_categoryList.GetMember("entryList", &catEntryList);

				const std::uint32_t combined = FoldEntryFilterFlags(itemEntryList);

				const std::uint32_t c = catEntryList.GetArraySize();
				std::uint32_t       writeFailures = 0;
				for (std::uint32_t i = 0; i < c; ++i) {
					RE::GFxValue entry;
					if (!catEntryList.GetElement(i, &entry)) {
						continue;
					}
					RE::GFxValue value;
					value.SetNumber(CategoryNonEmpty(ReadCategory(entry), combined) ? 1.0 : 0.0);
					if (!entry.SetMember("filterFlag", value)) {
						++writeFailures;
					}
				}
				if (writeFailures != 0) {
					logger::warn("InvalidateListFix: {} category filterFlag write(s) failed", writeFailures);
				}

				RE::GFxValue res2;
				if (!a_categoryList.Invoke("UpdateList", &res2, nullptr, 0)) {
					logger::warn("InvalidateListFix: categoryList.UpdateList invoke failed");
				}

				DispatchTail(a_lists, a_itemList, a_categoryList, savedFlag);
			}

			void RunFast(Params& a_params)
			{
				RE::GFxValue& lists = *a_params.thisPtr;
				RE::GFxValue  itemList;
				RE::GFxValue  categoryList;
				if (!Preconditions(lists, itemList, categoryList)) {
					InvokeOriginal(a_params);
					return;
				}
				RunFastBody(lists, itemList, categoryList);
			}

			/**
			 * Oracle: run vanilla (ground truth), then assert our O(N+C) prediction reproduces the
			 * category flags vanilla actually produced, on the live data. Logs any divergence.
			 */
			void RunVerify(Params& a_params)
			{
				InvokeOriginal(a_params);

				RE::GFxValue& lists = *a_params.thisPtr;
				RE::GFxValue  itemList;
				RE::GFxValue  categoryList;
				if (!lists.GetMember("itemList", &itemList) || !itemList.IsObject() ||
					!lists.GetMember("categoryList", &categoryList) || !categoryList.IsObject()) {
					logger::warn("InvalidateListFix verify: itemList/categoryList missing; skipped");
					return;
				}
				RE::GFxValue itemEntryList;
				RE::GFxValue catEntryList;
				if (!itemList.GetMember("entryList", &itemEntryList) || !itemEntryList.IsArray() ||
					!categoryList.GetMember("entryList", &catEntryList) || !catEntryList.IsArray()) {
					logger::warn("InvalidateListFix verify: entryList missing; skipped");
					return;
				}

				const std::uint32_t combined = FoldEntryFilterFlags(itemEntryList);
				const std::uint32_t c = catEntryList.GetArraySize();
				int                 mismatches = 0;
				for (std::uint32_t i = 0; i < c; ++i) {
					RE::GFxValue entry;
					if (!catEntryList.GetElement(i, &entry)) {
						continue;
					}
					const CategoryInput ci = ReadCategory(entry);
					RE::GFxValue        ff;
					entry.GetMember("filterFlag", &ff);
					const std::uint32_t actual = AsUint32(ff);
					const std::uint32_t predicted = CategoryNonEmpty(ci, combined) ? 1 : 0;
					if (actual != predicted) {
						++mismatches;
						logger::warn("InvalidateListFix verify: category {} flag=0x{:x} bDontHide={} vanilla={} predicted={}",
							i, ci.flag, ci.bDontHide, actual, predicted);
					}
				}
				if (mismatches == 0) {
					logger::info("InvalidateListFix verify: OK ({} categories, {} items, mask 0x{:x})",
						c, itemEntryList.GetArraySize(), combined);
					return;
				}
				logger::warn("InvalidateListFix verify: {} mismatch(es)", mismatches);
			}

			RE::GFxValue      _original;
			RE::GFxMovieView* _view;
		};
	}

	void Install(RE::GFxMovieView* a_view)
	{
		if (!a_view) {
			return;
		}

		RE::GFxValue lists;
		if (!a_view->GetVariable(&lists, "_level0.Menu_mc.inventoryLists")) {
			return;  // not an inventory-family menu movie; nothing to wrap
		}

		RE::GFxValue marker;
		if (lists.GetMember("_fu_invld_fixed", &marker) && marker.IsBool() && marker.GetBool()) {
			return;
		}

		RE::GFxValue original;
		if (!lists.GetMember("InvalidateListData", &original) || !original.IsObject()) {
			logger::warn("InvalidateListFix: inventoryLists.InvalidateListData not found");
			return;
		}

		auto         handler = RE::make_gptr<Handler>(std::move(original), a_view);
		RE::GFxValue hook;
		a_view->CreateFunction(&hook, handler.get());
		if (!lists.SetMember("InvalidateListData", hook)) {
			logger::warn("InvalidateListFix: failed to replace InvalidateListData");
			return;
		}

		RE::GFxValue wrapped;
		wrapped.SetBoolean(true);
		lists.SetMember("_fu_invld_fixed", wrapped);
		logger::info("InvalidateListFix: installed O(N+C) InvalidateListData");
	}

	void SetVerify(bool a_on)
	{
		VerifyFlag() = a_on;
	}
}
