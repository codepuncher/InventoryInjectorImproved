#include "PCH.h"

#include "PluginAPI.h"

#include "I4Hook.h"
#include "I5API.h"

namespace InventoryInjectorImproved::PluginAPI
{
	namespace
	{
		class APIImpl final : public API::IAPI
		{
		public:
			[[nodiscard]] std::uint32_t GetVersion() const noexcept override
			{
				return API::kVersion;
			}

			void InvalidateItem(std::uint32_t a_formID) noexcept override
			{
				I4Hook::InvalidateFormID(a_formID);
			}
		};

		APIImpl& Instance()
		{
			static APIImpl instance;
			return instance;
		}
	}

	void BroadcastAPI(const SKSE::MessagingInterface* a_messaging)
	{
		if (!a_messaging->Dispatch(API::kMessage_GetAPI, &Instance(), sizeof(void*), nullptr)) {
			logger::error("PluginAPI: failed to dispatch IAPI");
		}
	}
}
