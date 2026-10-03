#include "PCH.h"

#include "GFxArrayUtil.h"

namespace InventoryInjectorImproved
{
	bool TryGetObjectElement(RE::GFxValue& a_array, std::uint32_t a_index, RE::GFxValue& a_out)
	{
		return a_array.GetElement(a_index, &a_out) && a_out.IsObject();
	}
}
