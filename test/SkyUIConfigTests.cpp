#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "SkyUIConfig.h"

using InventoryInjectorImproved::SkyUIConfig::ParseNoIconColors;

TEST_CASE("ParseNoIconColors reads true and false", "[skyuiconfig]")
{
	CHECK(ParseNoIconColors("[Appearance]\nicons.item.noColor = true\n") == std::optional{ true });
	CHECK(ParseNoIconColors("[Appearance]\nicons.item.noColor = false\n") == std::optional{ false });
}

TEST_CASE("ParseNoIconColors is case-insensitive", "[skyuiconfig]")
{
	CHECK(ParseNoIconColors("[Appearance]\nicons.item.noColor = TRUE\n") == std::optional{ true });
}

TEST_CASE("ParseNoIconColors handles CRLF and no trailing newline", "[skyuiconfig]")
{
	CHECK(ParseNoIconColors("[Appearance] \r\nicons.item.noColor = true\r\n") == std::optional{ true });
	CHECK(ParseNoIconColors("[Appearance]\nicons.item.noColor=true") == std::optional{ true });
}

TEST_CASE("ParseNoIconColors ignores a key before the Appearance section", "[skyuiconfig]")
{
	CHECK(ParseNoIconColors("icons.item.noColor = true\n[Appearance]\n") == std::nullopt);
	CHECK(ParseNoIconColors("icons.item.noColor = true\n[Appearance]\nicons.item.noColor = false\n") ==
		  std::optional{ false });
}

TEST_CASE("ParseNoIconColors returns nullopt without section or key", "[skyuiconfig]")
{
	CHECK(ParseNoIconColors("icons.item.noColor = true\n") == std::nullopt);
	CHECK(ParseNoIconColors("[Appearance]\nicons.item.showStolen = true\n") == std::nullopt);
	CHECK(ParseNoIconColors("") == std::nullopt);
}

TEST_CASE("ParseNoIconColors treats an empty value as false", "[skyuiconfig]")
{
	CHECK(ParseNoIconColors("[Appearance]\nicons.item.noColor =   \n") == std::optional{ false });
}

TEST_CASE("ParseNoIconColors does not strip tabs, matching I4", "[skyuiconfig]")
{
	CHECK(ParseNoIconColors("[Appearance]\nicons.item.noColor =\ttrue\n") == std::optional{ false });
}
