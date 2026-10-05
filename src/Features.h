#pragma once

#include <atomic>

namespace InventoryInjectorImproved::Features
{
	namespace detail
	{
		inline std::atomic<bool>& EnabledFlag()
		{
			static std::atomic<bool> flag{ true };
			return flag;
		}
	}

	[[nodiscard]] inline bool IsEnabled()
	{
		return detail::EnabledFlag().load(std::memory_order_relaxed);
	}

	inline void SetEnabled(bool a_on)
	{
		detail::EnabledFlag().store(a_on, std::memory_order_relaxed);
	}
}
