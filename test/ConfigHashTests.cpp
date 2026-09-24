#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ConfigHash.h"

using InventoryInjectorImproved::ConfigFile;
using InventoryInjectorImproved::HashConfig;

namespace
{
	std::vector<std::byte> Bytes(std::string_view a_s)
	{
		std::vector<std::byte> v;
		for (const char c : a_s) {
			v.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(c)));
		}
		return v;
	}
}

TEST_CASE("HashConfig is deterministic", "[confighash]")
{
	const std::vector<ConfigFile> files{
		{ "Skyrim.esm", Bytes("{rules:1}") },
		{ "COIN.esp", Bytes("{rules:2}") }
	};
	CHECK(HashConfig(files) == HashConfig(files));
}

TEST_CASE("HashConfig is order-sensitive", "[confighash]")
{
	const ConfigFile a{ "A.esp", Bytes("aaa") };
	const ConfigFile b{ "B.esp", Bytes("bbb") };
	CHECK(HashConfig({ a, b }) != HashConfig({ b, a }));
}

TEST_CASE("HashConfig reacts to content changes", "[confighash]")
{
	CHECK(HashConfig({ { "A.esp", Bytes("v1") } }) != HashConfig({ { "A.esp", Bytes("v2") } }));
}

TEST_CASE("HashConfig name/content boundary is unambiguous", "[confighash]")
{
	// {("a","bc")} must differ from {("ab","c")} despite identical concatenation.
	CHECK(HashConfig({ { "a", Bytes("bc") } }) != HashConfig({ { "ab", Bytes("c") } }));
}

TEST_CASE("HashConfig of no config-bearing plugins is stable", "[confighash]")
{
	CHECK(HashConfig({}) == HashConfig({}));
}
