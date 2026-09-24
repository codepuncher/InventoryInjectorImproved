#include "PCH.h"

#include "I4Hook.h"

#include "CacheDelta.h"
#include "CacheFingerprint.h"
#include "CacheKey.h"
#include "CacheSerialization.h"
#include "FrameProbe.h"  // NOLINT(readability-duplicate-include): false positive, included once and required

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <optional>
#include <span>
#include <unordered_map>

namespace InventoryInjectorImproved::I4Hook
{
	namespace
	{
		/**
		 * A cached dynamic-item delta plus its reuse-guard token (see DynamicToken).
		 * Dynamic (0xFF) items key on their session-stable formId; the token gates the
		 * hit so a freed id reassigned to a different item does not serve a stale delta.
		 */
		struct DynamicEntry
		{
			Delta         delta;
			std::uint64_t token{ 0 };
		};

		struct HookState
		{
			std::unordered_map<std::uint64_t, Delta> cache;
			// In-session only; keyed by session formId + reuse token; never serialized (0xFF IDs are unstable across saves).
			std::unordered_map<std::uint64_t, DynamicEntry> dynamicCache;
			std::mutex                                      cacheMutex;
			std::size_t                                     restoredThisSession = 0;

			std::atomic<bool> bypass{ false };

			std::mutex   statsMutex;
			TimingSample lastTiming{};
			TimingSample worstTiming{};
		};

		/**
		 * All hook state in a function-local static: avoids mutable namespace-scope
		 * globals and defers the maps' construction to first use, so their init cannot
		 * throw before main (bugprone-throwing-static-initialization).
		 */
		HookState& State()
		{
			static HookState state;
			return state;
		}

		/**
		 * Tag the current menu by querying the UI at call time. One shared handler
		 * serves all four inventory-family menus (they share InventoryIconSetter),
		 * so the menu cannot be bound at Inject time.
		 */
		const char* CurrentMenuTag()
		{
			auto* const ui = RE::UI::GetSingleton();
			if (!ui) {
				return "?";
			}
			if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
				return "Container";
			}
			if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
				return "Barter";
			}
			if (ui->IsMenuOpen(RE::GiftMenu::MENU_NAME)) {
				return "Gift";
			}
			if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
				return "Inventory";
			}
			if (ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) {
				return "Crafting";
			}
			if (ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
				return "Magic";
			}
			return "?";
		}

		/**
		 * Records last always; worst only from real cached-path runs, so a raw-I4
		 * bypass sample never masquerades as the cached worst case in status.
		 */
		void RecordTiming(const char* a_menu, std::uint32_t a_count, std::uint32_t a_hits,
			std::int64_t a_total_us, std::int64_t a_i4_us, bool a_bypass)
		{
			const TimingSample s{ .valid = true, .menu = a_menu, .count = a_count, .hits = a_hits, .total_us = a_total_us, .i4_us = a_i4_us, .bypass = a_bypass };
			auto&              hs = State();
			std::scoped_lock   lock(hs.statsMutex);
			hs.lastTiming = s;
			if (!a_bypass && (!hs.worstTiming.valid || a_total_us > hs.worstTiming.total_us)) {
				hs.worstTiming = s;
			}
		}

		constexpr std::uint32_t kHeadRecord = 'HEAD';
		constexpr std::uint32_t kEntryRecord = 'ENTR';
		constexpr std::uint32_t kRecordVersion = 1;
		// Reject an ENTR record claiming more than this (corrupt length) before allocating.
		constexpr std::uint32_t kMaxEntryBufBytes = 16U * 1024U * 1024U;
		constexpr std::uint32_t kConfigReadChunkBytes = 64U * 1024U;

		// Bump when the serialized format or the delta-capture logic changes.
		constexpr std::uint32_t kSchemaVersion = 2;

		/**
		 * v1 does not read I4's DLL version at runtime; the config-bytes hash plus
		 * schema version carry validity. Stored for forward-compat (see spec open point).
		 */
		constexpr std::uint32_t kI4Version = 0;

		std::uint32_t PluginVersionStamp()
		{
			const auto* decl = SKSE::PluginDeclaration::GetSingleton();
			if (!decl) {
				return 0;
			}
			const auto v = decl->GetVersion();
			return (static_cast<std::uint32_t>(v[0]) << 24) |
			       (static_cast<std::uint32_t>(v[1]) << 16) |
			       (static_cast<std::uint32_t>(v[2]) << 8) |
			       static_cast<std::uint32_t>(v[3]);
		}

		std::uint64_t ComputeConfigHashUncached()
		{
			ConfigHasher hasher;
			auto* const  dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return hasher.Digest();
			}

			std::vector<std::byte> chunk(kConfigReadChunkBytes);
			for (auto* const file : dataHandler->files) {
				if (!file || file->recordFlags.none(RE::TESFile::RecordFlag::kChecked)) {
					continue;
				}

				const std::filesystem::path espPath =
					std::filesystem::path("Data") / file->fileName;

				std::error_code     ec;
				const auto          rawSz = std::filesystem::file_size(espPath, ec);
				const std::uint64_t sz = ec ? 0U : static_cast<std::uint64_t>(rawSz);
				ec.clear();
				const auto          rawMt = std::filesystem::last_write_time(espPath, ec);
				const std::uint64_t mt = ec ? 0U : static_cast<std::uint64_t>(rawMt.time_since_epoch().count());

				std::array<std::byte, 16> header{};
				for (std::size_t i = 0; i < 8; ++i) {
					header.at(i) = static_cast<std::byte>((sz >> (8 * i)) & 0xFFU);
					header.at(8 + i) = static_cast<std::byte>((mt >> (8 * i)) & 0xFFU);
				}

				auto jsonName = std::filesystem::path(file->fileName);
				jsonName.replace_extension("json");
				const auto jsonPath =
					std::filesystem::path("SKSE/Plugins/InventoryInjector") / jsonName;

				RE::BSResourceNiBinaryStream stream{ jsonPath.string() };
				const std::uint32_t          jsonSize = stream.good() ? stream.stream->totalSize : 0U;

				hasher.BeginFile(file->fileName, static_cast<std::uint32_t>(header.size()) + jsonSize);
				hasher.Update(header);
				for (std::uint32_t remaining = jsonSize; remaining > 0;) {
					const auto n = std::min<std::uint32_t>(remaining, kConfigReadChunkBytes);
					if (!stream.read(chunk.data(), n)) {
						logger::warn("I4Hook: short read of config JSON {}; hash covers only the bytes before it", jsonPath.string());
						// Marks where the read stopped so a truncated file is less likely to collide with another config.
						hasher.Update(std::as_bytes(std::span{ &remaining, 1 }));
						break;
					}
					hasher.Update(std::span{ chunk.data(), n });
					remaining -= n;
				}
			}

			return hasher.Digest();
		}

		std::uint64_t ComputeConfigHash()
		{
			static const std::uint64_t cached = ComputeConfigHashUncached();
			return cached;
		}

		std::optional<double> GetNumber(RE::GFxValue& a_entry, const char* a_name)
		{
			RE::GFxValue v;
			if (!a_entry.GetMember(a_name, &v) || !v.IsNumber()) {
				return std::nullopt;
			}
			return v.GetNumber();
		}

		std::optional<RE::FormID> GetFormID(RE::GFxValue& a_entry)
		{
			if (const auto n = GetNumber(a_entry, "formId")) {
				return static_cast<RE::FormID>(*n);
			}
			return std::nullopt;
		}

		bool IsSoulGem(RE::GFxValue& a_entry)
		{
			const auto n = GetNumber(a_entry, "formType");
			return n && static_cast<RE::FormType>(static_cast<std::uint32_t>(*n)) == RE::FormType::SoulGem;
		}

		std::uint32_t GetStatus(RE::GFxValue& a_entry)
		{
			return static_cast<std::uint32_t>(GetNumber(a_entry, "status").value_or(0.0));
		}

		bool ScalarEqual(const RE::GFxValue& a, const RE::GFxValue& b)
		{
			if (a.GetType() != b.GetType()) {
				return false;
			}
			switch (a.GetType()) {
			case RE::GFxValue::ValueType::kNumber:
				return a.GetNumber() == b.GetNumber();
			case RE::GFxValue::ValueType::kBoolean:
				return a.GetBool() == b.GetBool();
			case RE::GFxValue::ValueType::kString:
				return std::strcmp(a.GetString(), b.GetString()) == 0;
			default:
				return true;  // objects are filtered out in BuildDelta before reaching here
			}
		}

		std::unordered_map<std::string, RE::GFxValue> Snapshot(const RE::GFxValue& a_entry)
		{
			std::unordered_map<std::string, RE::GFxValue> members;
			a_entry.VisitMembers([&members](const char* a_name, const RE::GFxValue& a_val) {
				members.emplace(a_name, a_val);
			});
			return members;
		}

		CachedField CaptureField(const std::string& a_name, const RE::GFxValue& a_val)
		{
			CachedField f;
			f.name = a_name;
			switch (a_val.GetType()) {
			case RE::GFxValue::ValueType::kNumber:
				f.kind = CachedField::Kind::kNumber;
				f.number = a_val.GetNumber();
				break;
			case RE::GFxValue::ValueType::kBoolean:
				f.kind = CachedField::Kind::kBool;
				f.boolean = a_val.GetBool();
				break;
			case RE::GFxValue::ValueType::kString:
				f.kind = CachedField::Kind::kString;
				f.str = a_val.GetString();
				break;
			case RE::GFxValue::ValueType::kObject:
				f.kind = CachedField::Kind::kKeywordObj;
				a_val.VisitMembers(
					[&f](const char* a_kw, const RE::GFxValue&) { f.keywords.emplace_back(a_kw); });
				break;
			default:
				f.kind = CachedField::Kind::kNull;
				break;
			}
			return f;
		}

		/**
		 * Reuse-guard token for a dynamic item: a fingerprint of the fields I4 never
		 * writes, the display name `text` and `formType`. It must hash inputs only,
		 * not I4 outputs like effectKeywords/iconColor, which flip on the reused entry
		 * object and were the fingerprint churn this key scheme replaced. So the token
		 * is stable across refreshes and take/use, yet changes when the engine
		 * reassigns a freed 0xFF id to an item with a different name or formType,
		 * rejecting the stale delta. Residual: a reused id whose new occupant shares
		 * both name and formType collides and shows the old icon until the next load
		 * (dynamic cache is session-only); rare and cosmetic, accepted over churn.
		 */
		std::uint64_t DynamicToken(RE::GFxValue& a_entry)
		{
			std::vector<CachedField> fields;
			RE::GFxValue             member;
			if (a_entry.GetMember("text", &member)) {
				fields.push_back(CaptureField("text", member));
			}
			if (a_entry.GetMember("formType", &member)) {
				fields.push_back(CaptureField("formType", member));
			}
			return FingerprintFields(std::move(fields));
		}

		Delta BuildDelta(
			const std::unordered_map<std::string, RE::GFxValue>& a_before,
			const std::unordered_map<std::string, RE::GFxValue>& a_after)
		{
			Delta delta;
			for (const auto& [name, val] : a_after) {
				/**
				 * Skip object members from the applied delta. Config-rule output is
				 * scalar-only (I4's CustomData variant), and the one object I4 does
				 * emit, effectKeywords (IconSetter ExtendMagicItemData), is consumed
				 * only within I4's own processList pass and never read off a
				 * cache-applied entry, so rebuilding it per hit via CreateObject is
				 * wasted work. The remaining objects (keywords, useSound, ...) are
				 * input already present on the fresh entry.
				 */
				if (val.GetType() == RE::GFxValue::ValueType::kObject) {
					continue;
				}
				const auto it = a_before.find(name);
				if (it == a_before.end() || !ScalarEqual(it->second, val)) {
					delta.push_back(CaptureField(name, val));
				}
			}
			for (const auto& before : a_before) {
				if (!a_after.contains(before.first)) {
					CachedField f;
					f.name = before.first;
					f.kind = CachedField::Kind::kDelete;
					delta.push_back(std::move(f));
				}
			}
			return delta;
		}

		void ApplyDelta(RE::GFxMovie* a_movie, RE::GFxValue& a_entry, const Delta& a_delta)
		{
			for (const auto& f : a_delta) {
				switch (f.kind) {
				case CachedField::Kind::kDelete:
					a_entry.DeleteMember(f.name.c_str());
					break;
				case CachedField::Kind::kNumber:
					a_entry.SetMember(f.name.c_str(), RE::GFxValue{ f.number });
					break;
				case CachedField::Kind::kBool:
					{
						RE::GFxValue v;
						v.SetBoolean(f.boolean);
						a_entry.SetMember(f.name.c_str(), v);
					}
					break;
				case CachedField::Kind::kString:
					a_entry.SetMember(f.name.c_str(), RE::GFxValue{ f.str.c_str() });
					break;
				case CachedField::Kind::kKeywordObj:
					{
						RE::GFxValue obj;
						a_movie->CreateObject(&obj);
						RE::GFxValue t;
						t.SetBoolean(true);
						for (const auto& kw : f.keywords) {
							obj.SetMember(kw.c_str(), t);
						}
						a_entry.SetMember(f.name.c_str(), obj);
					}
					break;
				case CachedField::Kind::kNull:
					{
						const RE::GFxValue v{ RE::GFxValue::ValueType::kNull };
						a_entry.SetMember(f.name.c_str(), v);
					}
					break;
				}
			}
		}

		enum class CacheTarget : std::uint8_t
		{
			kStatic,
			kDynamic
		};

		// NOLINTNEXTLINE(bugprone-exception-escape): move ctor may throw only via RE::GFxValue; fine for this transient carrier
		struct Miss
		{
			RE::GFxValue                                  entry;
			std::uint64_t                                 key{ 0 };
			std::unordered_map<std::string, RE::GFxValue> before;
			CacheTarget                                   target{ CacheTarget::kStatic };
			std::uint64_t                                 token{ 0 };
		};

		/**
		 * Outcome of one cache-classification pass over an entry list: the misses to
		 * hand to I4, hit/miss tallies, and (only when logging) the classify/apply
		 * timing slices.
		 */
		struct ClassifyResult
		{
			std::vector<Miss> misses;
			std::uint32_t     hits = 0;
			std::uint32_t     dynamicMisses = 0;
			std::uint32_t     noIdMisses = 0;
			std::int64_t      classify_us = 0;
			std::int64_t      apply_us = 0;
			// Refresh timing anchor, captured after the cache lock so total_us excludes lock-wait.
			std::chrono::high_resolution_clock::time_point started;
		};

		class ProcessListCache : public RE::GFxFunctionHandler
		{
		public:
			explicit ProcessListCache(RE::GFxValue a_original) :
				_original(std::move(a_original))
			{}

			void Call(Params& a_params) override
			{
				const std::span<RE::GFxValue> args{ a_params.args, a_params.argCount };
				if (args.empty() || !args.front().IsObject()) {
					CallOriginal(a_params, args);
					return;
				}

				const RE::GFxValue& list = args.front();
				RE::GFxValue        entryList;
				if ((!list.GetMember("_entryList", &entryList) || !entryList.IsArray()) &&
					(!list.GetMember("entryList", &entryList) || !entryList.IsArray())) {
					CallOriginal(a_params, args);
					return;
				}

				const char* const   menu = CurrentMenuTag();
				const bool          wantLog = spdlog::should_log(spdlog::level::debug);
				const std::uint32_t count = entryList.GetArraySize();

				if (State().bypass.load(std::memory_order_relaxed)) {
					RunBypass(a_params, args, menu, count, wantLog);
					return;
				}

				const ClassifyResult cr = ClassifyEntries(a_params, entryList, count, wantLog);

				const std::int64_t i4_us = ProcessMisses(a_params, cr.misses);

				const auto         t2 = std::chrono::high_resolution_clock::now();
				const std::int64_t total_us = std::chrono::duration_cast<std::chrono::microseconds>(t2 - cr.started).count();

				RecordTiming(menu, count, cr.hits, total_us, i4_us, false);
				FrameProbe::NoteRefresh(menu, count);

				if (!wantLog) {
					return;
				}
				LogRefresh(menu, count, cr, i4_us, total_us);
			}

		private:
			void RunBypass(Params& a_params, std::span<RE::GFxValue> a_args, const char* a_menu,
				std::uint32_t a_count, bool a_wantLog)
			{
				const auto b0 = std::chrono::high_resolution_clock::now();
				CallOriginal(a_params, a_args);
				const auto b1 = std::chrono::high_resolution_clock::now();
				const auto us = std::chrono::duration_cast<std::chrono::microseconds>(b1 - b0).count();
				RecordTiming(a_menu, a_count, 0, us, us, true);
				FrameProbe::NoteRefresh(a_menu, a_count);
				if (a_wantLog) {
					logger::debug(
						"I4 processList [{}] {} entries | bypass on (raw I4) | total {} us",
						a_menu, a_count, us);
				}
			}

			/**
			 * Classify every entry against the caches, applying cached deltas in place
			 * and collecting the misses I4 must render. Holds the cache lock for the
			 * whole pass and releases it on return, before the caller runs I4 (foreign
			 * code that could re-enter here); ProcessMisses re-locks to write deltas back.
			 */
			static ClassifyResult ClassifyEntries(Params& a_params, RE::GFxValue& a_entryList,
				std::uint32_t a_count, bool a_wantLog)
			{
				ClassifyResult res;
				res.misses.reserve(a_count);

				auto&            hs = State();
				std::scoped_lock lock(hs.cacheMutex);
				res.started = std::chrono::high_resolution_clock::now();

				for (std::uint32_t i = 0; i < a_count; ++i) {
					// c0 before GetElement so per-entry classify_us includes element-fetch cost.
					const auto   c0 = a_wantLog ? std::chrono::high_resolution_clock::now() : std::chrono::high_resolution_clock::time_point{};
					RE::GFxValue entry;
					if (!a_entryList.GetElement(i, &entry) || !entry.IsObject()) {
						continue;
					}
					ClassifyOne(hs, a_params.movie, entry, c0, a_wantLog, res);
				}

				return res;
			}

			/**
			 * Classify a single entry: apply its cached delta as a hit, or record it as
			 * a miss for I4. Caller holds cacheMutex and has confirmed entry is an object.
			 */
			static void ClassifyOne(HookState& a_hs, RE::GFxMovie* a_movie, RE::GFxValue& a_entry,
				std::chrono::high_resolution_clock::time_point a_c0, bool a_wantLog, ClassifyResult& a_res)
			{
				const auto addClassify = [&] {
					if (a_wantLog) {
						a_res.classify_us += std::chrono::duration_cast<std::chrono::microseconds>(
							std::chrono::high_resolution_clock::now() - a_c0)
						                         .count();
					}
				};

				const auto formID = GetFormID(a_entry);
				if (!formID) {
					addClassify();
					++a_res.noIdMisses;
					a_res.misses.push_back({ .entry = a_entry, .key = 0, .before = {}, .target = CacheTarget::kStatic });
					return;
				}

				const bool    dynamic = IsDynamicForm(*formID);
				std::uint64_t key = 0;
				std::uint64_t token = 0;
				if (dynamic) {
					key = *formID;  // session-stable form identity; no content hash
					token = DynamicToken(a_entry);
				} else {
					const bool soulGem = IsSoulGem(a_entry);
					key = MakeCacheKey(*formID, soulGem, soulGem ? GetStatus(a_entry) : 0);
				}

				const Delta* delta = LookupDelta(a_hs, dynamic, key, token);
				addClassify();

				if (delta) {
					const auto a0 = a_wantLog ? std::chrono::high_resolution_clock::now() : std::chrono::high_resolution_clock::time_point{};
					ApplyDelta(a_movie, a_entry, *delta);
					if (a_wantLog) {
						a_res.apply_us += std::chrono::duration_cast<std::chrono::microseconds>(
							std::chrono::high_resolution_clock::now() - a0)
						                      .count();
					}
					++a_res.hits;
					return;
				}

				if (dynamic) {
					++a_res.dynamicMisses;
				}
				a_res.misses.push_back({ .entry = a_entry, .key = key, .before = Snapshot(a_entry), .target = dynamic ? CacheTarget::kDynamic : CacheTarget::kStatic, .token = token });
			}

			/**
			 * A dynamic hit requires the token to match too: a matching formId whose
			 * token differs is a reused 0xFF id now holding a different item, so its
			 * cached delta is stale and must be recomputed. Caller holds cacheMutex.
			 */
			static const Delta* LookupDelta(HookState& a_hs, bool a_dynamic, std::uint64_t a_key, std::uint64_t a_token)
			{
				if (a_dynamic) {
					const auto it = a_hs.dynamicCache.find(a_key);
					if (it != a_hs.dynamicCache.end() && it->second.token == a_token) {
						return &it->second.delta;
					}
					return nullptr;
				}
				const auto it = a_hs.cache.find(a_key);
				if (it != a_hs.cache.end()) {
					return &it->second;
				}
				return nullptr;
			}

			static void LogRefresh(const char* a_menu, std::uint32_t a_count, const ClassifyResult& a_cr,
				std::int64_t a_i4_us, std::int64_t a_total_us)
			{
				std::size_t cacheSize = 0;
				std::size_t dynCacheSize = 0;
				{
					auto&            hs = State();
					std::scoped_lock sizeLock(hs.cacheMutex);
					cacheSize = hs.cache.size();
					dynCacheSize = hs.dynamicCache.size();
				}

				logger::debug(
					"I4 processList [{}] {} entries | {} hits, {} misses ({} dynamic, {} no-id) | "
					"classify {} us, apply {} us, i4 {} us, total {} us | "
					"cache {} | dyncache {} | bypass off",
					a_menu,
					a_count,
					a_cr.hits,
					a_cr.misses.size(),
					a_cr.dynamicMisses,
					a_cr.noIdMisses,
					a_cr.classify_us,
					a_cr.apply_us,
					a_i4_us,
					a_total_us,
					cacheSize,
					dynCacheSize);
			}

			bool InvokeOriginal(Params& a_params, std::span<RE::GFxValue> a_args)
			{
				std::vector<RE::GFxValue> callArgs;
				callArgs.reserve(1 + a_args.size());
				callArgs.push_back(*a_params.thisPtr);
				callArgs.insert(callArgs.end(), a_args.begin(), a_args.end());
				return _original.Invoke("call", a_params.retVal, callArgs.data(), callArgs.size());
			}

			void CallOriginal(Params& a_params, std::span<RE::GFxValue> a_args)
			{
				if (!InvokeOriginal(a_params, a_args)) {
					logger::warn("I4Hook: failed to invoke original processList (passthrough)");
				}
			}

			std::int64_t ProcessMisses(Params& a_params, const std::vector<Miss>& a_misses)
			{
				if (a_misses.empty()) {
					return 0;
				}

				RE::GFxValue missArr;
				a_params.movie->CreateArray(&missArr);
				for (const auto& m : a_misses) {
					missArr.PushBack(m.entry);
				}

				RE::GFxValue tempList;
				a_params.movie->CreateObject(&tempList);
				tempList.SetMember("_entryList", missArr);
				tempList.SetMember("entryList", missArr);

				std::array<RE::GFxValue, 1> listArg{ tempList };
				const auto                  i0 = std::chrono::high_resolution_clock::now();
				const bool                  ok = InvokeOriginal(a_params, listArg);
				const auto                  i1 = std::chrono::high_resolution_clock::now();
				const auto                  i4_us = std::chrono::duration_cast<std::chrono::microseconds>(i1 - i0).count();

				/**
				 * Cache only if I4 ran; an empty delta would later serve the item
				 * icon-less as a "hit".
				 */
				if (!ok) {
					logger::warn("I4Hook: original processList invoke failed; not caching this batch");
					return i4_us;
				}

				auto&            hs = State();
				std::scoped_lock lock(hs.cacheMutex);
				for (const auto& m : a_misses) {
					if (m.key == 0 && m.target == CacheTarget::kStatic) {
						continue;  // no-id miss: not cacheable
					}
					auto delta = BuildDelta(m.before, Snapshot(m.entry));
					if (m.target == CacheTarget::kDynamic) {
						hs.dynamicCache[m.key] = { .delta = std::move(delta), .token = m.token };
					} else {
						hs.cache[m.key] = std::move(delta);
					}
				}
				return i4_us;
			}

			RE::GFxValue _original;
		};
	}

	void Inject(RE::GFxMovieView* a_view, const char* a_setterPath)
	{
		if (!a_view) {
			return;
		}

		RE::GFxValue iconSetter;
		if (!a_view->GetVariable(&iconSetter, a_setterPath)) {
			logger::warn("I4Hook: {} not found", a_setterPath);
			return;
		}

		RE::GFxValue proto;
		if (!iconSetter.GetMember("prototype", &proto)) {
			logger::warn("I4Hook: {}.prototype not found", a_setterPath);
			return;
		}

		RE::GFxValue marker;
		if (proto.GetMember("_fu_wrapped", &marker) && marker.IsBool() && marker.GetBool()) {
			return;
		}

		RE::GFxValue original;
		if (!proto.GetMember("processList", &original) || !original.IsObject()) {
			logger::warn("I4Hook: {}.processList not found (is I4 installed?)", a_setterPath);
			return;
		}

		auto         handler = RE::make_gptr<ProcessListCache>(std::move(original));
		RE::GFxValue hook;
		a_view->CreateFunction(&hook, handler.get());
		if (!proto.SetMember("processList", hook)) {
			logger::warn("I4Hook: failed to replace {}.processList", a_setterPath);
			return;
		}

		RE::GFxValue wrapped;
		wrapped.SetBoolean(true);
		proto.SetMember("_fu_wrapped", wrapped);
		logger::info("I4Hook: wrapped {}.processList", a_setterPath);
	}

	std::size_t ClearCache()
	{
		auto&            hs = State();
		std::scoped_lock lock(hs.cacheMutex);
		const auto       cleared = hs.cache.size() + hs.dynamicCache.size();
		hs.cache.clear();
		hs.dynamicCache.clear();
		return cleared;
	}

	CacheStats GetCacheStats()
	{
		auto&            hs = State();
		std::scoped_lock lock(hs.cacheMutex);
		return { .entries = hs.cache.size(), .restoredThisSession = hs.restoredThisSession, .dynamicEntries = hs.dynamicCache.size() };
	}

	void SetBypass(bool a_on)
	{
		State().bypass.store(a_on, std::memory_order_relaxed);
	}

	void SetDebugLogging(bool a_on)
	{
		const auto level = a_on ? spdlog::level::debug : spdlog::level::info;
		spdlog::set_level(level);
		spdlog::flush_on(level);
	}

	TimingStats GetTimingStats()
	{
		auto&            hs = State();
		std::scoped_lock lock(hs.statsMutex);
		return { .last = hs.lastTiming, .worst = hs.worstTiming };
	}

	void Save(SKSE::SerializationInterface* a_intfc)
	{
		std::vector<CacheEntry> entries;
		std::uint64_t           configHash = ComputeConfigHash();
		{
			auto&            hs = State();
			std::scoped_lock lock(hs.cacheMutex);
			entries.reserve(hs.cache.size());
			for (const auto& [key, delta] : hs.cache) {
				const auto d = DecodeCacheKey(key);
				entries.push_back({ .formID = d.formID, .soulGem = d.soulGem, .status = d.status, .delta = delta });
			}
		}

		if (!a_intfc->OpenRecord(kHeadRecord, kRecordVersion)) {
			logger::warn("I4Hook: failed to open HEAD record");
			return;
		}
		const std::uint32_t schema = kSchemaVersion;
		const std::uint32_t plugin = PluginVersionStamp();
		const std::uint32_t i4 = kI4Version;
		a_intfc->WriteRecordData(schema);
		a_intfc->WriteRecordData(plugin);
		a_intfc->WriteRecordData(i4);
		a_intfc->WriteRecordData(configHash);

		const auto buf = Serial::SerializeEntries(entries);
		if (!a_intfc->OpenRecord(kEntryRecord, kRecordVersion)) {
			logger::warn("I4Hook: failed to open ENTR record");
			return;
		}
		a_intfc->WriteRecordData(buf.data(), static_cast<std::uint32_t>(buf.size()));
		logger::info("I4Hook: saved {} cache entries", entries.size());
	}

	void Load(SKSE::SerializationInterface* a_intfc)
	{
		bool                   headerValid = false;
		bool                   haveEntries = false;
		std::vector<std::byte> entryBuf;

		/**
		 * Reset the session metric at the load boundary: a successful load sets it to
		 * the restored count below, a cold start leaves it 0. Purge does not touch it.
		 */
		{
			auto&            hs = State();
			std::scoped_lock lock(hs.cacheMutex);
			hs.restoredThisSession = 0;
		}

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t length = 0;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type == kHeadRecord) {
				if (version != kRecordVersion) {
					logger::warn("I4Hook: HEAD record version {} unexpected; starting cold", version);
					continue;
				}
				std::uint32_t schema = 0;
				std::uint32_t plugin = 0;
				std::uint32_t i4 = 0;
				std::uint64_t configHash = 0;
				a_intfc->ReadRecordData(schema);
				a_intfc->ReadRecordData(plugin);
				a_intfc->ReadRecordData(i4);
				a_intfc->ReadRecordData(configHash);
				headerValid = schema == kSchemaVersion &&
				              plugin == PluginVersionStamp() &&
				              i4 == kI4Version &&
				              configHash == ComputeConfigHash();
				continue;
			}
			if (type == kEntryRecord) {
				if (version != kRecordVersion) {
					logger::warn("I4Hook: ENTR record version {} unexpected; starting cold", version);
					continue;
				}
				if (length > kMaxEntryBufBytes) {
					logger::warn("I4Hook: ENTR record length {} exceeds cap; starting cold", length);
					continue;
				}
				entryBuf.resize(length);
				if (length > 0) {
					a_intfc->ReadRecordData(entryBuf.data(), length);
				}
				haveEntries = true;
			}
		}

		if (!headerValid || !haveEntries) {
			logger::info("I4Hook: persisted cache invalid or absent; starting cold");
			return;
		}

		const auto decoded = Serial::DeserializeEntries(entryBuf);
		if (!decoded) {
			logger::warn("I4Hook: persisted cache corrupt; starting cold");
			return;
		}

		auto&            hs = State();
		std::scoped_lock lock(hs.cacheMutex);
		hs.cache.clear();
		hs.dynamicCache.clear();
		for (const auto& e : *decoded) {
			hs.cache[MakeCacheKey(e.formID, e.soulGem, e.soulGem ? e.status : 0)] = e.delta;
		}
		hs.restoredThisSession = decoded->size();
		logger::info("I4Hook: restored {} cache entries", decoded->size());
	}
}
