#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "CacheDelta.h"

namespace InventoryInjectorImproved::Serial
{
	namespace detail
	{
		inline void PutU8(std::vector<std::byte>& a_out, std::uint8_t a_v)
		{
			a_out.push_back(static_cast<std::byte>(a_v));
		}

		inline void PutU32(std::vector<std::byte>& a_out, std::uint32_t a_v)
		{
			for (int i = 0; i < 4; ++i) {
				a_out.push_back(static_cast<std::byte>((a_v >> (8 * i)) & 0xFFu));
			}
		}

		inline void PutU64(std::vector<std::byte>& a_out, std::uint64_t a_v)
		{
			for (int i = 0; i < 8; ++i) {
				a_out.push_back(static_cast<std::byte>((a_v >> (8 * i)) & 0xFFu));
			}
		}

		inline void PutDouble(std::vector<std::byte>& a_out, double a_v)
		{
			std::uint64_t bits = 0;
			std::memcpy(&bits, &a_v, sizeof(bits));
			PutU64(a_out, bits);
		}

		inline void PutStr(std::vector<std::byte>& a_out, const std::string& a_s)
		{
			PutU32(a_out, static_cast<std::uint32_t>(a_s.size()));
			for (const char c : a_s) {
				a_out.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(c)));
			}
		}

		struct Reader
		{
			std::span<const std::byte> data;
			std::size_t                pos{ 0 };

			bool U8(std::uint8_t& a_out)
			{
				if (data.size() - pos < 1) {
					return false;
				}
				a_out = std::to_integer<std::uint8_t>(data[pos++]);
				return true;
			}

			bool U32(std::uint32_t& a_out)
			{
				if (data.size() - pos < 4) {
					return false;
				}
				a_out = 0;
				for (int i = 0; i < 4; ++i) {
					a_out |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[pos++])) << (8 * i);
				}
				return true;
			}

			bool U64(std::uint64_t& a_out)
			{
				if (data.size() - pos < 8) {
					return false;
				}
				a_out = 0;
				for (int i = 0; i < 8; ++i) {
					a_out |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(data[pos++])) << (8 * i);
				}
				return true;
			}

			bool Dbl(double& a_out)
			{
				std::uint64_t bits = 0;
				if (!U64(bits)) {
					return false;
				}
				std::memcpy(&a_out, &bits, sizeof(a_out));
				return true;
			}

			bool Str(std::string& a_out)
			{
				std::uint32_t n = 0;
				if (!U32(n)) {
					return false;
				}
				if (data.size() - pos < n) {
					return false;
				}
				a_out.assign(reinterpret_cast<const char*>(data.data() + pos), n);
				pos += n;
				return true;
			}
		};
	}

	inline std::vector<std::byte> SerializeEntries(const std::vector<CacheEntry>& a_entries)
	{
		using namespace detail;
		std::vector<std::byte> out;
		PutU32(out, static_cast<std::uint32_t>(a_entries.size()));
		for (const auto& e : a_entries) {
			PutU32(out, e.formID);
			PutU8(out, e.soulGem ? 1 : 0);
			PutU32(out, e.status);
			PutU32(out, static_cast<std::uint32_t>(e.delta.size()));
			for (const auto& f : e.delta) {
				PutU8(out, static_cast<std::uint8_t>(f.kind));
				PutStr(out, f.name);
				switch (f.kind) {
				case CachedField::Kind::kNumber:
					PutDouble(out, f.number);
					break;
				case CachedField::Kind::kBool:
					PutU8(out, f.boolean ? 1 : 0);
					break;
				case CachedField::Kind::kString:
					PutStr(out, f.str);
					break;
				case CachedField::Kind::kKeywordObj:
					PutU32(out, static_cast<std::uint32_t>(f.keywords.size()));
					for (const auto& kw : f.keywords) {
						PutStr(out, kw);
					}
					break;
				case CachedField::Kind::kDelete:
				case CachedField::Kind::kNull:
					break;
				}
			}
		}
		return out;
	}

	inline std::optional<std::vector<CacheEntry>> DeserializeEntries(std::span<const std::byte> a_bytes)
	{
		using namespace detail;
		// Minimum on-wire bytes per element; bounds reserve() against a corrupt count.
		constexpr std::size_t kMinEntryBytes = 13;   // u32 formID + u8 soulGem + u32 status + u32 fieldCount
		constexpr std::size_t kMinFieldBytes = 5;    // u8 kind + u32 name-length prefix
		constexpr std::size_t kMinKeywordBytes = 4;  // u32 length prefix (empty keyword)
		Reader                r{ a_bytes };
		std::uint32_t         count = 0;
		if (!r.U32(count)) {
			return std::nullopt;
		}

		std::vector<CacheEntry> entries;
		entries.reserve(std::min<std::size_t>(count, (a_bytes.size() - r.pos) / kMinEntryBytes));
		for (std::uint32_t i = 0; i < count; ++i) {
			CacheEntry    e;
			std::uint8_t  soulGem = 0;
			std::uint32_t fieldCount = 0;
			if (!r.U32(e.formID) || !r.U8(soulGem) || !r.U32(e.status) || !r.U32(fieldCount)) {
				return std::nullopt;
			}
			e.soulGem = soulGem != 0;

			e.delta.reserve(std::min<std::size_t>(fieldCount, (a_bytes.size() - r.pos) / kMinFieldBytes));
			for (std::uint32_t j = 0; j < fieldCount; ++j) {
				std::uint8_t kindByte = 0;
				CachedField  f;
				if (!r.U8(kindByte) || !r.Str(f.name)) {
					return std::nullopt;
				}
				if (kindByte > static_cast<std::uint8_t>(CachedField::Kind::kKeywordObj)) {
					return std::nullopt;
				}
				f.kind = static_cast<CachedField::Kind>(kindByte);
				switch (f.kind) {
				case CachedField::Kind::kNumber:
					if (!r.Dbl(f.number)) {
						return std::nullopt;
					}
					break;
				case CachedField::Kind::kBool:
					{
						std::uint8_t b = 0;
						if (!r.U8(b)) {
							return std::nullopt;
						}
						f.boolean = b != 0;
					}
					break;
				case CachedField::Kind::kString:
					if (!r.Str(f.str)) {
						return std::nullopt;
					}
					break;
				case CachedField::Kind::kKeywordObj:
					{
						std::uint32_t kwCount = 0;
						if (!r.U32(kwCount)) {
							return std::nullopt;
						}
						f.keywords.reserve(std::min<std::size_t>(kwCount, (a_bytes.size() - r.pos) / kMinKeywordBytes));
						for (std::uint32_t k = 0; k < kwCount; ++k) {
							std::string kw;
							if (!r.Str(kw)) {
								return std::nullopt;
							}
							f.keywords.push_back(std::move(kw));
						}
					}
					break;
				case CachedField::Kind::kDelete:
				case CachedField::Kind::kNull:
					break;
				}
				e.delta.push_back(std::move(f));
			}
			entries.push_back(std::move(e));
		}
		if (r.pos != a_bytes.size()) {
			return std::nullopt;
		}
		return entries;
	}
}
