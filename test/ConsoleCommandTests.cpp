#include <catch2/catch_test_macros.hpp>

#include "ConsoleCommand.h"

using InventoryInjectorImproved::Console::Action;
using InventoryInjectorImproved::Console::ParseCommand;

TEST_CASE("ParseCommand matches cache purge/status case-insensitively and ignores extra whitespace", "[console]")
{
	CHECK(ParseCommand("i5 cache purge") == Action::kPurge);
	CHECK(ParseCommand("I5 CACHE PURGE") == Action::kPurge);
	CHECK(ParseCommand("i5 purge") == Action::kUsage);
	CHECK(ParseCommand("I5 Status") == Action::kStatus);
	CHECK(ParseCommand("  i5   status ") == Action::kStatus);
}

TEST_CASE("ParseCommand returns kNone for lines outside our namespace", "[console]")
{
	CHECK(ParseCommand("tgm") == Action::kNone);
	CHECK(ParseCommand("player.additem f 1") == Action::kNone);
	CHECK(ParseCommand("") == Action::kNone);
	CHECK(ParseCommand("   ") == Action::kNone);
}

TEST_CASE("ParseCommand returns kUsage for bare, unknown, or over-specified namespace lines", "[console]")
{
	CHECK(ParseCommand("i5") == Action::kUsage);
	CHECK(ParseCommand("i5 bogus") == Action::kUsage);
	CHECK(ParseCommand("i5 purge extra") == Action::kUsage);
	CHECK(ParseCommand("i5 cache purge extra") == Action::kUsage);
	CHECK(ParseCommand("i5 status now") == Action::kUsage);
}

TEST_CASE("ParseCommand handles enable, disable, cache, debug, verify, and SkyUI inventory-dedupe toggles", "[console]")
{
	CHECK(ParseCommand("i5 debug on") == Action::kDebugOn);
	CHECK(ParseCommand("I5 DEBUG OFF") == Action::kDebugOff);
	CHECK(ParseCommand("  i5   disable ") == Action::kDisableAll);
	CHECK(ParseCommand("i5 enable") == Action::kEnableAll);
	CHECK(ParseCommand("i5 cache disable") == Action::kDisableCache);
	CHECK(ParseCommand("I5 CACHE ENABLE") == Action::kEnableCache);
	CHECK(ParseCommand("i5 bypass on") == Action::kUsage);
	CHECK(ParseCommand("i5 bypass off") == Action::kUsage);
	CHECK(ParseCommand("i5 verify on") == Action::kVerifyOn);
	CHECK(ParseCommand("I5 VERIFY OFF") == Action::kVerifyOff);
	CHECK(ParseCommand("i5 skyui inventory-dedupe enable") == Action::kMemoOn);
	CHECK(ParseCommand("I5 SKYUI INVENTORY-DEDUPE DISABLE") == Action::kMemoOff);
}

TEST_CASE("ParseCommand rejects malformed enable/disable/debug/verify/SkyUI inventory-dedupe lines as usage", "[console]")
{
	CHECK(ParseCommand("i5 debug") == Action::kUsage);
	CHECK(ParseCommand("i5 disable now") == Action::kUsage);
	CHECK(ParseCommand("i5 enable now") == Action::kUsage);
	CHECK(ParseCommand("i5 cache") == Action::kUsage);
	CHECK(ParseCommand("i5 cache on") == Action::kUsage);
	CHECK(ParseCommand("i5 cache disable now") == Action::kUsage);
	CHECK(ParseCommand("i5 debug maybe") == Action::kUsage);
	CHECK(ParseCommand("i5 verify") == Action::kUsage);
	CHECK(ParseCommand("i5 verify maybe") == Action::kUsage);
	CHECK(ParseCommand("i5 skyui inventory-dedupe") == Action::kUsage);
	CHECK(ParseCommand("i5 skyui inventory-dedupe enable now") == Action::kUsage);
	CHECK(ParseCommand("i5 memo on") == Action::kUsage);
}
