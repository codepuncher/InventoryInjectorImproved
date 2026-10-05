#include "PCH.h"

#include "ConsoleHook.h"

#include "ConsoleCommand.h"
#include "Features.h"
#include "I4Hook.h"
#include "InvalidateListFix.h"
#include "InvalidateMemo.h"  // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl

#include <bit>
#include <string>

namespace InventoryInjectorImproved::ConsoleHook
{
	namespace
	{
		using CompileAndRunFn = void(RE::Script*, RE::ScriptCompiler*, RE::COMPILER_NAME, RE::TESObjectREFR*);

		/**
		 * The trampolined original CompileAndRun, behind a getter to avoid a mutable global.
		 */
		REL::Relocation<CompileAndRunFn>& OriginalCompileAndRun()
		{
			static REL::Relocation<CompileAndRunFn> original;
			return original;
		}

		void Print(const std::string& a_msg)
		{
			auto* const console = RE::ConsoleLog::GetSingleton();
			if (!console) {
				return;
			}
			console->Print("%s", a_msg.c_str());
		}

		std::string FormatSample(const char* a_label, const I4Hook::TimingSample& a_s)
		{
			std::string s = std::string(" | ") + a_label + " [" + a_s.menu + "] " +
			                std::to_string(a_s.count) + " entries, " + std::to_string(a_s.hits) +
			                " hits, total " + std::to_string(a_s.total_us) + "us (i4 " +
			                std::to_string(a_s.i4_us) + "us)";
			if (a_s.bypass) {
				s += " (cache disabled)";
			}
			return s;
		}

		std::string FeatureState(const char* a_name, bool a_on)
		{
			return std::string(a_name) + (a_on ? " on" : " off");
		}

		std::string StatusLine(const I4Hook::CacheStats& a_stats)
		{
			std::string line = "I5 cache: " + std::to_string(a_stats.entries) + " entries (" +
			                   (a_stats.entries > 0 ? "warm" : "cold") + "); restored " +
			                   std::to_string(a_stats.restoredThisSession) + " from SKSE co-save this session";
			line += "; " + std::to_string(a_stats.dynamicEntries) + " session-only (not saved)";
			const bool features = Features::IsEnabled();
			line += "; features: " + FeatureState("icon cache", features && I4Hook::CacheEnabled()) +
			        ", " + FeatureState("alchemy icon fix", features) +
			        ", " + FeatureState("Faster SkyUI list refresh", features) +
			        ", " + FeatureState("Skip redundant SkyUI refreshes", features && InvalidateMemo::IsEnabled());

			const auto timing = I4Hook::GetTimingStats();
			if (!timing.last.valid) {
				return line + " | no timing yet (i5 debug on, then open a menu)";
			}
			line += FormatSample("last", timing.last);
			if (timing.worst.valid) {
				line += FormatSample("worst", timing.worst);
			}
			return line;
		}

		void CompileAndRun(RE::Script* a_script, RE::ScriptCompiler* a_compiler,
			RE::COMPILER_NAME a_name, RE::TESObjectREFR* a_targetRef)
		{
			const auto action = a_script ? Console::ParseCommand(a_script->GetCommand()) : Console::Action::kNone;
			switch (action) {
			case Console::Action::kDisableAll:
				Features::SetEnabled(false);
				Print("I5: all features disabled");
				return;
			case Console::Action::kEnableAll:
				Features::SetEnabled(true);
				Print("I5: all features enabled");
				return;
			case Console::Action::kStatus:
				Print(StatusLine(I4Hook::GetCacheStats()));
				return;
			case Console::Action::kDebugOn:
				I4Hook::SetDebugLogging(true);
				Print("I5: debug logging ON (per-call timing -> InventoryInjectorImproved.log)");
				return;
			case Console::Action::kDebugOff:
				I4Hook::SetDebugLogging(false);
				Print("I5: debug logging OFF");
				return;
			case Console::Action::kVerifyOn:
				InvalidateListFix::SetVerify(true);
				Print("I5: InvalidateListData verify ON (asserting O(N+C) flags match vanilla -> InventoryInjectorImproved.log)");
				return;
			case Console::Action::kVerifyOff:
				InvalidateListFix::SetVerify(false);
				Print("I5: InvalidateListData verify OFF (fast path)");
				return;
			case Console::Action::kDisableCache:
				I4Hook::SetBypass(true);
				Print("I5: icon cache disabled");
				return;
			case Console::Action::kEnableCache:
				I4Hook::SetBypass(false);
				Print("I5: icon cache enabled");
				return;
			case Console::Action::kPurge:
				Print("I5: icon cache purged (" + std::to_string(I4Hook::ClearCache()) + " entries cleared)");
				return;
			case Console::Action::kMemoOn:
				InvalidateMemo::SetEnabled(true);
				Print("I5: Skip redundant SkyUI refreshes enabled");
				return;
			case Console::Action::kMemoOff:
				InvalidateMemo::SetEnabled(false);
				Print("I5: Skip redundant SkyUI refreshes disabled");
				return;
			case Console::Action::kUsage:
				Print("I5: usage - i5 disable | enable | status | debug on|off | verify on|off | cache enable|disable|purge | skyui inventory-dedupe enable|disable");
				return;
			case Console::Action::kNone:
				break;
			}
			OriginalCompileAndRun()(a_script, a_compiler, a_name, a_targetRef);
		}
	}

	void Install()
	{
		static bool installed = false;
		if (installed) {
			return;
		}
		installed = true;

		const REL::Relocation<std::uintptr_t> hookPoint{ REL::RelocationID(52065, 52952),
			REL::VariantOffset(0xE2, 0x52, 0xE2) };
		const auto                            address = hookPoint.address();
		if (*std::bit_cast<const std::uint8_t*>(address) != 0xE8) {
			logger::warn("ConsoleHook: patch site is not a call on this runtime, i5 console commands unavailable");
			return;
		}
		OriginalCompileAndRun() = SKSE::GetTrampoline().write_call<5>(address, CompileAndRun);
		logger::info("ConsoleHook: installed i5 console commands");
	}
}
