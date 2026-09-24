#include "PCH.h"

#include "ConsoleHook.h"

#include "ConsoleCommand.h"
#include "I4Hook.h"
#include "InvalidateListFix.h"
#include "InvalidateMemo.h"

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
				s += " (raw I4 bypass)";
			}
			return s;
		}

		std::string StatusLine(const I4Hook::CacheStats& a_stats)
		{
			std::string line = "I5 cache: " + std::to_string(a_stats.entries) + " entries (" +
			                   (a_stats.entries > 0 ? "warm" : "cold") + "); restored " +
			                   std::to_string(a_stats.restoredThisSession) + " from co-save this session";
			line += "; " + std::to_string(a_stats.dynamicEntries) + " dynamic (in-session)";

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
			case Console::Action::kPurge:
				Print("I5: icon cache purged (" + std::to_string(I4Hook::ClearCache()) + " entries cleared)");
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
			case Console::Action::kBypassOn:
				I4Hook::SetBypass(true);
				Print("I5: cache bypass ON (raw I4, not cached)");
				return;
			case Console::Action::kBypassOff:
				I4Hook::SetBypass(false);
				Print("I5: cache bypass OFF (cached)");
				return;
			case Console::Action::kVerifyOn:
				InvalidateListFix::SetVerify(true);
				Print("I5: InvalidateListData verify ON (asserting O(N+C) flags match vanilla -> InventoryInjectorImproved.log)");
				return;
			case Console::Action::kVerifyOff:
				InvalidateListFix::SetVerify(false);
				Print("I5: InvalidateListData verify OFF (fast path)");
				return;
			case Console::Action::kMemoOn:
				InvalidateMemo::SetEnabled(true);
				Print("I5: invalidate memo ON (skips redundant itemList reprocess)");
				return;
			case Console::Action::kMemoOff:
				InvalidateMemo::SetEnabled(false);
				Print("I5: invalidate memo OFF (every InvalidateData runs)");
				return;
			case Console::Action::kUsage:
				Print("I5: usage - i5 purge | status | debug on|off | bypass on|off | verify on|off | memo on|off");
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
		OriginalCompileAndRun() = SKSE::GetTrampoline().write_call<5>(hookPoint.address(), CompileAndRun);
		logger::info("ConsoleHook: installed i5 console commands");
	}
}
