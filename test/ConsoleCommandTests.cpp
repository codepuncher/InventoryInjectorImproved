#include <catch2/catch_test_macros.hpp>

#include "ConsoleCommand.h"

using InventoryInjectorImproved::Console::Action;
using InventoryInjectorImproved::Console::Classify;
using InventoryInjectorImproved::Console::ParseCommand;

TEST_CASE("ParseCommand matches purge/status case-insensitively and ignores extra whitespace", "[console]")
{
	CHECK(ParseCommand("i5 purge") == Action::kPurge);
	CHECK(ParseCommand("I5 PURGE") == Action::kPurge);
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
	CHECK(ParseCommand("i5 status now") == Action::kUsage);
}

TEST_CASE("ParseCommand handles debug, bypass, verify, and memo toggles", "[console]")
{
	CHECK(ParseCommand("i5 debug on") == Action::kDebugOn);
	CHECK(ParseCommand("I5 DEBUG OFF") == Action::kDebugOff);
	CHECK(ParseCommand("  i5   bypass   on ") == Action::kBypassOn);
	CHECK(ParseCommand("i5 bypass off") == Action::kBypassOff);
	CHECK(ParseCommand("i5 verify on") == Action::kVerifyOn);
	CHECK(ParseCommand("I5 VERIFY OFF") == Action::kVerifyOff);
	CHECK(ParseCommand("i5 memo on") == Action::kMemoOn);
	CHECK(ParseCommand("I5 MEMO OFF") == Action::kMemoOff);
}

TEST_CASE("ParseCommand rejects malformed debug/bypass/verify/memo lines as usage", "[console]")
{
	CHECK(ParseCommand("i5 debug") == Action::kUsage);
	CHECK(ParseCommand("i5 bypass") == Action::kUsage);
	CHECK(ParseCommand("i5 debug maybe") == Action::kUsage);
	CHECK(ParseCommand("i5 bypass on now") == Action::kUsage);
	CHECK(ParseCommand("i5 verify") == Action::kUsage);
	CHECK(ParseCommand("i5 verify maybe") == Action::kUsage);
	CHECK(ParseCommand("i5 memo") == Action::kUsage);
	CHECK(ParseCommand("i5 memo maybe") == Action::kUsage);
}

TEST_CASE("ParseCommand recognizes invalidate with a valid hex formID", "[console]")
{
	CHECK(ParseCommand("i5 invalidate 14") == Action::kInvalidate);
	CHECK(ParseCommand("I5 INVALIDATE 0001A02F") == Action::kInvalidate);
	CHECK(ParseCommand("  i5   invalidate   0x14  ") == Action::kInvalidate);
}

TEST_CASE("ParseCommand rejects malformed invalidate lines as usage", "[console]")
{
	CHECK(ParseCommand("i5 invalidate") == Action::kUsage);
	CHECK(ParseCommand("i5 invalidate zz") == Action::kUsage);
	CHECK(ParseCommand("i5 invalidate 14 extra") == Action::kUsage);
	CHECK(ParseCommand("i5 invalidate 1234567890") == Action::kUsage);
}

TEST_CASE("Classify extracts the formID from a valid invalidate line", "[console]")
{
	CHECK(Classify("i5 invalidate 14").formID == 0x14U);
	CHECK(Classify("I5 INVALIDATE 0001A02F").formID == 0x0001A02FU);
	CHECK(Classify("i5 invalidate 0x14").formID == 0x14U);
}
