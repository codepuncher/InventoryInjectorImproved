#include "PCH.h"

#include "FavoritesHook.h"

#include <vector>

namespace InventoryInjectorImproved::FavoritesHook
{
	namespace
	{
		using ProcessMessageFn = RE::UI_MESSAGE_RESULTS(RE::FavoritesMenu*, RE::UIMessage&);

		constexpr auto kSetterPath = "_global.FavoritesIconSetter";
		constexpr auto kProcessListWrapped = "_i5_favProcessListWrapped";

		std::vector<PreStep>& PreSteps()
		{
			static std::vector<PreStep> steps;
			return steps;
		}

		REL::Relocation<ProcessMessageFn>& OriginalProcessMessage()
		{
			static REL::Relocation<ProcessMessageFn> original;
			return original;
		}

		class ProcessListShim : public RE::GFxFunctionHandler
		{
		public:
			explicit ProcessListShim(RE::GFxValue a_original) :
				_original(std::move(a_original))
			{}

			void Call(Params& a_params) override
			{
				for (const auto step : PreSteps()) {
					step(a_params);
				}

				if (!_original.Invoke("call", a_params.retVal, a_params.argsWithThisRef,
						static_cast<std::size_t>(a_params.argCount) + 1)) {
					logger::warn("Favorites: original processList invoke failed");
				}
			}

		private:
			RE::GFxValue _original;
		};

		void WrapProcessList(RE::GFxMovieView* a_view)
		{
			if (!a_view) {
				return;
			}

			RE::GFxValue setter;
			if (!a_view->GetVariable(&setter, kSetterPath)) {
				logger::debug("Favorites: {} not found", kSetterPath);
				return;
			}
			RE::GFxValue proto;
			if (!setter.GetMember("prototype", &proto) || !proto.IsObject()) {
				logger::debug("Favorites: {}.prototype not found", kSetterPath);
				return;
			}

			RE::GFxValue alreadyWrapped;
			if (proto.GetMember(kProcessListWrapped, &alreadyWrapped) && alreadyWrapped.IsBool() && alreadyWrapped.GetBool()) {
				return;
			}

			RE::GFxValue original;
			if (!proto.GetMember("processList", &original) || !original.IsObject()) {
				logger::warn("Favorites: {}.prototype.processList not found", kSetterPath);
				return;
			}

			auto         handler = RE::make_gptr<ProcessListShim>(std::move(original));
			RE::GFxValue hook;
			a_view->CreateFunction(&hook, handler.get());
			if (!proto.SetMember("processList", hook)) {
				logger::warn("Favorites: failed to replace {}.prototype.processList", kSetterPath);
				return;
			}

			RE::GFxValue wrapped;
			wrapped.SetBoolean(true);
			if (!proto.SetMember(kProcessListWrapped, wrapped)) {
				logger::warn("Favorites: failed to set {} wrapped flag", kSetterPath);
			}
			logger::info("Favorites: wrapped {}.processList", kSetterPath);
		}

		RE::UI_MESSAGE_RESULTS ProcessMessage(RE::FavoritesMenu* a_menu, RE::UIMessage& a_message)
		{
			if (a_menu && a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
				WrapProcessList(a_menu->uiMovie.get());
			}
			return OriginalProcessMessage()(a_menu, a_message);
		}
	}

	void AddPreStep(PreStep a_step)
	{
		PreSteps().push_back(a_step);

		static bool installed = false;
		if (installed) {
			return;
		}
		installed = true;

		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_FavoritesMenu[0] };
		if (!vtbl.address()) {
			return;
		}
		OriginalProcessMessage() = vtbl.write_vfunc(0x4, ProcessMessage);
		logger::info("Favorites: hooked FavoritesMenu::ProcessMessage");
	}
}
