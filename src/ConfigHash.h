#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace InventoryInjectorImproved
{
	struct ConfigFile
	{
		std::string            name;
		std::vector<std::byte> bytes;
	};

	namespace detail
	{
		inline constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
		inline constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

		inline std::uint64_t FnvBytes(std::uint64_t a_h, std::span<const std::byte> a_bytes)
		{
			for (const auto b : a_bytes) {
				a_h ^= std::to_integer<std::uint8_t>(b);
				a_h *= kFnvPrime;
			}
			return a_h;
		}

		inline std::uint64_t FnvU32(std::uint64_t a_h, std::uint32_t a_v)
		{
			for (int i = 0; i < 4; ++i) {
				a_h ^= static_cast<std::uint8_t>((a_v >> (8 * i)) & 0xFFu);
				a_h *= kFnvPrime;
			}
			return a_h;
		}
	}

	/**
	 * Streaming form of HashConfig, so file contents can be fed in chunks
	 * without holding them in memory. Declaring each file's size up front
	 * keeps the name/content boundary unambiguous.
	 */
	class ConfigHasher
	{
	public:
		void BeginFile(std::string_view a_name, std::uint32_t a_size)
		{
			_h = detail::FnvU32(_h, static_cast<std::uint32_t>(a_name.size()));
			_h = detail::FnvBytes(_h, std::as_bytes(std::span{ a_name.data(), a_name.size() }));
			_h = detail::FnvU32(_h, a_size);
		}

		void Update(std::span<const std::byte> a_bytes) { _h = detail::FnvBytes(_h, a_bytes); }

		[[nodiscard]] std::uint64_t Digest() const { return _h; }

	private:
		std::uint64_t _h{ detail::kFnvOffset };
	};

	/**
	 * Order-sensitive hash over the config-bearing plugins, each contributing its
	 * name and JSON bytes.
	 */
	[[nodiscard]] inline std::uint64_t HashConfig(const std::vector<ConfigFile>& a_files)
	{
		ConfigHasher hasher;
		for (const auto& f : a_files) {
			hasher.BeginFile(f.name, static_cast<std::uint32_t>(f.bytes.size()));
			hasher.Update(f.bytes);
		}
		return hasher.Digest();
	}
}
