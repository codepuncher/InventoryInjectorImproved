#pragma once

#include <cstddef>
#include <cstdint>

namespace RE
{
	class GFxMovieView;
}

namespace SKSE
{
	class SerializationInterface;
}

namespace InventoryInjectorImproved::I4Hook
{
	struct CacheStats
	{
		std::size_t entries{ 0 };
		std::size_t restoredThisSession{ 0 };
		std::size_t dynamicEntries{ 0 };
	};

	struct TimingSample
	{
		bool          valid{ false };
		const char*   menu{ "" };
		std::uint32_t count{ 0 };
		std::uint32_t hits{ 0 };
		std::int64_t  total_us{ 0 };
		std::int64_t  i4_us{ 0 };
		bool          bypass{ false };
	};

	struct TimingStats
	{
		TimingSample last;
		TimingSample worst;
	};

	void        Inject(RE::GFxMovieView* a_view, const char* a_setterPath);
	std::size_t ClearCache();
	CacheStats  GetCacheStats();
	void        SetBypass(bool a_on);
	void        SetDebugLogging(bool a_on);
	TimingStats GetTimingStats();
	void        Save(SKSE::SerializationInterface* a_intfc);
	void        Load(SKSE::SerializationInterface* a_intfc);
}
