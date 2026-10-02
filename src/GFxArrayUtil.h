#pragma once

#include <cstdint>

namespace RE
{
	class GFxValue;
}

namespace InventoryInjectorImproved
{
	/**
	 * SkyUI's entryList can transiently hold AS `null`,
	 * which GFxValue::GetMember won't null-check for.
	 */
	bool TryGetObjectElement(RE::GFxValue& a_array, std::uint32_t a_index, RE::GFxValue& a_out);
}
