#pragma once

#include "RE/G/GFxFunctionHandler.h"

namespace InventoryInjectorImproved::FavoritesHook
{
	using PreStep = void (*)(RE::GFxFunctionHandler::Params& a_params);

	void AddPreStep(PreStep a_step);
}
