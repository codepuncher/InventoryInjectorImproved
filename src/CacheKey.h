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

	/**
	 * Which Scaleform icon setter produced a cached delta. Setters disagree on
	 * icon rules for the same form (e.g. CraftingIconSetter has no cloak
	 * branch), so a delta from one must never be served as a hit under another.
	 */
	enum class CacheSetter : std::uint8_t
	{
		kInventory = 0,
		kCrafting = 1,
		kMagic = 2
	};

	[[nodiscard]] constexpr std::uint64_t MakeCacheKey(
		std::uint32_t a_formID,
		bool          a_soulGem,
		std::uint32_t a_status,
		CacheSetter   a_setter) noexcept
	{
		std::uint64_t key = a_formID;
		if (a_soulGem) {
			key |= (std::uint64_t{ 1 } << 40) | (static_cast<std::uint64_t>(a_status & 0xFFU) << 32);
		}
		key |= static_cast<std::uint64_t>(a_setter) << 48;
		return key;
	}

	struct DecodedKey
	{
		std::uint32_t formID{ 0 };
		bool          soulGem{ false };
		std::uint32_t status{ 0 };
		CacheSetter   setter{ CacheSetter::kInventory };
	};

	/**
	 * Inverts MakeCacheKey to extract formID, soul-gem flag, status, and setter from a packed key.
	 */
	[[nodiscard]] constexpr DecodedKey DecodeCacheKey(std::uint64_t a_key) noexcept
	{
		return {
			.formID = static_cast<std::uint32_t>(a_key & 0xFFFFFFFFULL),
			.soulGem = ((a_key >> 40) & 0x1ULL) != 0,
			.status = static_cast<std::uint32_t>((a_key >> 32) & 0xFFULL),
			.setter = static_cast<CacheSetter>((a_key >> 48) & 0xFFULL)
		};
	}
}
