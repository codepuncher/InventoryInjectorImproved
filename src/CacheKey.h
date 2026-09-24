#pragma once

#include <cstdint>

namespace InventoryInjectorImproved
{
	/**
	 * 0xFF high byte marks a runtime form; the engine reuses those IDs, so they're unstable keys (0xFE/ESL is stable).
	 */
	[[nodiscard]] constexpr bool IsDynamicForm(std::uint32_t a_formID) noexcept
	{
		return (a_formID >> 24) == 0xFFU;
	}

	[[nodiscard]] constexpr std::uint64_t MakeCacheKey(
		std::uint32_t a_formID,
		bool          a_soulGem,
		std::uint32_t a_status) noexcept
	{
		std::uint64_t key = a_formID;
		if (a_soulGem) {
			key |= (std::uint64_t{ 1 } << 40) | (static_cast<std::uint64_t>(a_status & 0xFFU) << 32);
		}
		return key;
	}

	struct DecodedKey
	{
		std::uint32_t formID{ 0 };
		bool          soulGem{ false };
		std::uint32_t status{ 0 };
	};

	/**
	 * Inverts MakeCacheKey to extract formID, soul-gem flag, and status from a packed key.
	 */
	[[nodiscard]] constexpr DecodedKey DecodeCacheKey(std::uint64_t a_key) noexcept
	{
		return {
			.formID = static_cast<std::uint32_t>(a_key & 0xFFFFFFFFULL),
			.soulGem = ((a_key >> 40) & 0x1ULL) != 0,
			.status = static_cast<std::uint32_t>((a_key >> 32) & 0xFFULL)
		};
	}
}
