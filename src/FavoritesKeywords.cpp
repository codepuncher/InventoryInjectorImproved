#include "PCH.h"

#include "FavoritesHook.h"
#include "FavoritesKeywords.h"
#include "Features.h"
#include "KeywordFill.h"

#include <span>

namespace InventoryInjectorImproved::FavoritesKeywords
{
	namespace
	{
		void FillStep(RE::GFxFunctionHandler::Params& a_params)
		{
			const std::span<RE::GFxValue> args{ a_params.args, a_params.argCount };
			if (!Features::IsEnabled() || args.empty() || !args.front().IsObject()) {
				return;
			}

			RE::GFxValue entryList;
			args.front().GetMember("_entryList", &entryList);
			FillMissingKeywords(a_params.movie, entryList);
		}
	}

	void Install()
	{
		static bool installed = false;
		if (installed) {
			return;
		}
		installed = true;

		FavoritesHook::AddPreStep(FillStep);
	}
}
