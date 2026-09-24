#pragma once

#include "CacheDelta.h"
#include "ConfigHash.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace InventoryInjectorImproved
{
	/**
	 * Order-independent FNV-1a fingerprint of an entry's pre-I4 fields. Identical
	 * field sets hash equal; any change to a field's name, kind, or value changes
	 * the hash. Sound as a cache key because I4's icon delta is a pure function of
	 * the entry members (see the dynamic-item-cache spec).
	 */
	[[nodiscard]] inline std::uint64_t FingerprintFields(std::vector<CachedField> a_fields)
	{
		std::sort(a_fields.begin(), a_fields.end(),
			[](const CachedField& a, const CachedField& b) { return a.name < b.name; });

		std::uint64_t h = detail::kFnvOffset;
		for (const auto& f : a_fields) {
			h = detail::FnvU32(h, static_cast<std::uint32_t>(f.name.size()));
			h = detail::FnvBytes(h, std::as_bytes(std::span{ f.name.data(), f.name.size() }));
			h = detail::FnvU32(h, static_cast<std::uint32_t>(f.kind));
			switch (f.kind) {
			case CachedField::Kind::kNumber:
				{
					const auto bits = std::bit_cast<std::uint64_t>(f.number);
					h = detail::FnvU32(h, static_cast<std::uint32_t>(bits & 0xFFFFFFFFULL));
					h = detail::FnvU32(h, static_cast<std::uint32_t>(bits >> 32));
				}
				break;
			case CachedField::Kind::kBool:
				h = detail::FnvU32(h, f.boolean ? 1U : 0U);
				break;
			case CachedField::Kind::kString:
				h = detail::FnvU32(h, static_cast<std::uint32_t>(f.str.size()));
				h = detail::FnvBytes(h, std::as_bytes(std::span{ f.str.data(), f.str.size() }));
				break;
			case CachedField::Kind::kKeywordObj:
				{
					std::vector<std::string> kws = f.keywords;
					std::sort(kws.begin(), kws.end());
					h = detail::FnvU32(h, static_cast<std::uint32_t>(kws.size()));
					for (const auto& kw : kws) {
						h = detail::FnvU32(h, static_cast<std::uint32_t>(kw.size()));
						h = detail::FnvBytes(h, std::as_bytes(std::span{ kw.data(), kw.size() }));
					}
				}
				break;
			case CachedField::Kind::kNull:
			case CachedField::Kind::kDelete:
				break;
			}
		}
		return h;
	}
}
