#include <catch2/catch_test_macros.hpp>

#include "CacheFingerprint.h"

#include <string>
#include <vector>

using InventoryInjectorImproved::CachedField;
using InventoryInjectorImproved::FingerprintFields;

namespace
{
	CachedField Num(std::string a_name, double a_v)
	{
		CachedField f;
		f.name = std::move(a_name);
		f.kind = CachedField::Kind::kNumber;
		f.number = a_v;
		return f;
	}

	CachedField Str(std::string a_name, std::string a_v)
	{
		CachedField f;
		f.name = std::move(a_name);
		f.kind = CachedField::Kind::kString;
		f.str = std::move(a_v);
		return f;
	}

	CachedField Boolean(std::string a_name, bool a_v)
	{
		CachedField f;
		f.name = std::move(a_name);
		f.kind = CachedField::Kind::kBool;
		f.boolean = a_v;
		return f;
	}

	CachedField Kw(std::string a_name, std::vector<std::string> a_kws)
	{
		CachedField f;
		f.name = std::move(a_name);
		f.kind = CachedField::Kind::kKeywordObj;
		f.keywords = std::move(a_kws);
		return f;
	}
}

TEST_CASE("FingerprintFields is independent of field order", "[fingerprint]")
{
	const auto a = FingerprintFields({ Num("formId", 16), Str("text0", "Iron Sword"), Num("formType", 41) });
	const auto b = FingerprintFields({ Num("formType", 41), Num("formId", 16), Str("text0", "Iron Sword") });
	CHECK(a == b);
}

TEST_CASE("FingerprintFields changes when any value changes", "[fingerprint]")
{
	const auto base = FingerprintFields({ Num("formId", 16), Str("text0", "Iron Sword") });
	CHECK(FingerprintFields({ Num("formId", 17), Str("text0", "Iron Sword") }) != base);
	CHECK(FingerprintFields({ Num("formId", 16), Str("text0", "Steel Sword") }) != base);
}

TEST_CASE("FingerprintFields changes when a field is added or removed", "[fingerprint]")
{
	const auto base = FingerprintFields({ Num("formId", 16) });
	CHECK(FingerprintFields({ Num("formId", 16), Boolean("equipState", true) }) != base);
}

TEST_CASE("FingerprintFields distinguishes kinds carrying the same text", "[fingerprint]")
{
	CHECK(FingerprintFields({ Num("v", 1) }) != FingerprintFields({ Str("v", "1") }));
}

TEST_CASE("FingerprintFields is keyword-order-independent but set-sensitive", "[fingerprint]")
{
	const auto ab = FingerprintFields({ Kw("keywords", { "WeapTypeSword", "MagicDisallowEnchanting" }) });
	const auto ba = FingerprintFields({ Kw("keywords", { "MagicDisallowEnchanting", "WeapTypeSword" }) });
	CHECK(ab == ba);
	const auto one = FingerprintFields({ Kw("keywords", { "WeapTypeSword" }) });
	CHECK(one != ab);
}

TEST_CASE("dynamic reuse token distinguishes items by name+formType", "[fingerprint]")
{
	/**
	 * The dynamic-item reuse guard hashes only the fields I4 never writes: the
	 * display name `text` and `formType`. Same identity -> same token (a hit stays a
	 * hit across refreshes); a reassigned 0xFF id holding a different item -> a
	 * different token, so its stale delta is rejected.
	 */
	const auto poison = FingerprintFields({ Str("text", "Deadly Poison"), Num("formType", 46) });
	CHECK(FingerprintFields({ Str("text", "Deadly Poison"), Num("formType", 46) }) == poison);
	CHECK(FingerprintFields({ Str("text", "Healing Potion"), Num("formType", 46) }) != poison);
	CHECK(FingerprintFields({ Str("text", "Deadly Poison"), Num("formType", 41) }) != poison);
}
