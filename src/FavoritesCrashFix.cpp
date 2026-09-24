#include "PCH.h"

#include "FavoritesCrashFix.h"

#include "SkyUIConfig.h"

#include <algorithm>
#include <optional>
#include <string>

namespace InventoryInjectorImproved::FavoritesCrashFix
{
	namespace
	{
		using ProcessMessageFn = RE::UI_MESSAGE_RESULTS(RE::FavoritesMenu*, RE::UIMessage&);

		constexpr auto kSetterPath = "_global.FavoritesIconSetter";
		constexpr auto kNoIconColors = "_noIconColors";
		constexpr auto kMarker = "_i5_favfix";

		/**
		 * The original FavoritesMenu::ProcessMessage, behind a getter to avoid a mutable global.
		 */
		REL::Relocation<ProcessMessageFn>& OriginalProcessMessage()
		{
			static REL::Relocation<ProcessMessageFn> original;
			return original;
		}

		/**
		 * The MCM override SKI_SettingsManager stores for noColor, if the player has set one.
		 */
		std::optional<bool> ReadMcmOverride()
		{
			auto* const dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return std::nullopt;
			}
			auto* const settingsManager = dataHandler->LookupForm(0x80A, "SkyUI_SE.esp"sv);
			if (!settingsManager) {
				return std::nullopt;
			}
			auto* const vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return std::nullopt;
			}
			auto* const policy = vm->GetObjectHandlePolicy();
			if (!policy) {
				return std::nullopt;
			}
			const auto handle = policy->GetHandleForObject(settingsManager->GetFormType(), settingsManager);
			if (handle == policy->EmptyHandle()) {
				return std::nullopt;
			}

			RE::BSTSmartPointer<RE::BSScript::Object> object;
			if (!vm->FindBoundObject(handle, "SKI_SettingsManager", object) || !object) {
				return std::nullopt;
			}
			const auto* const keysVar = object->GetVariable("_overrideKeys");
			const auto* const valuesVar = object->GetVariable("_overrideValues");
			if (!keysVar || !valuesVar || !keysVar->IsArray() || !valuesVar->IsArray()) {
				return std::nullopt;
			}
			const auto keys = keysVar->GetArray();
			const auto values = valuesVar->GetArray();
			if (!keys || !values) {
				return std::nullopt;
			}

			const auto count = std::min<std::uint32_t>(keys->size(), values->size());
			for (std::uint32_t i = 0; i < count; ++i) {
				const auto& key = (*keys)[i];
				if (!key.IsString() || key.GetString() != "Appearance$icons$item$noColor"sv) {
					continue;
				}
				const auto& value = (*values)[i];
				if (!value.IsString()) {
					return std::nullopt;
				}
				return SkyUIConfig::IEquals(value.GetString(), "true");
			}
			return std::nullopt;
		}

		std::optional<bool> ReadConfigFile()
		{
			RE::BSResourceNiBinaryStream stream{ "Interface/skyui/config.txt" };
			if (!stream.good()) {
				return std::nullopt;
			}
			std::string text(stream.stream->totalSize, '\0');
			if (!stream.read(text.data(), static_cast<std::uint32_t>(text.size()))) {
				return std::nullopt;
			}
			return SkyUIConfig::ParseNoIconColors(text);
		}

		/**
		 * Mirrors I4's own lookup order so Favourites honours the same setting as I4 would.
		 */
		bool ReadNoIconColors()
		{
			if (const auto mcm = ReadMcmOverride()) {
				return *mcm;
			}
			static const auto config = ReadConfigFile();
			return config.value_or(false);
		}

		void PresetNoIconColors(RE::GFxValue* a_setter)
		{
			if (!a_setter || !a_setter->IsObject() || a_setter->HasMember(kNoIconColors)) {
				return;
			}
			RE::GFxValue noIconColors;
			noIconColors.SetBoolean(ReadNoIconColors());
			if (!a_setter->SetMember(kNoIconColors, noIconColors)) {
				logger::error("FavoritesCrashFix: failed to set {}; I4 will run its own lookup", kNoIconColors);
			}
		}

		class ProcessListShim : public RE::GFxFunctionHandler
		{
		public:
			explicit ProcessListShim(RE::GFxValue a_original) :
				_original(std::move(a_original))
			{}

			void Call(Params& a_params) override
			{
				PresetNoIconColors(a_params.thisPtr);
				if (!_original.Invoke("call", a_params.retVal, a_params.argsWithThisRef,
						static_cast<std::size_t>(a_params.argCount) + 1)) {
					logger::warn("FavoritesCrashFix: original processList invoke failed");
				}
			}

		private:
			RE::GFxValue _original;
		};

		void WrapProcessList(RE::GFxMovieView* a_view)
		{
			static const bool i4Loaded = static_cast<bool>(REX::W32::GetModuleHandleW(L"InventoryInjector.dll"));
			if (!a_view || !i4Loaded) {
				return;
			}

			RE::GFxValue setter;
			if (!a_view->GetVariable(&setter, kSetterPath)) {
				logger::debug("FavoritesCrashFix: {} not found", kSetterPath);
				return;
			}
			RE::GFxValue proto;
			if (!setter.GetMember("prototype", &proto) || !proto.IsObject()) {
				logger::debug("FavoritesCrashFix: {}.prototype not found", kSetterPath);
				return;
			}

			RE::GFxValue marker;
			if (proto.GetMember(kMarker, &marker) && marker.IsBool() && marker.GetBool()) {
				return;
			}

			RE::GFxValue original;
			if (!proto.GetMember("processList", &original) || !original.IsObject()) {
				logger::warn("FavoritesCrashFix: {}.prototype.processList not found", kSetterPath);
				return;
			}

			auto         handler = RE::make_gptr<ProcessListShim>(std::move(original));
			RE::GFxValue hook;
			a_view->CreateFunction(&hook, handler.get());
			if (!proto.SetMember("processList", hook)) {
				logger::warn("FavoritesCrashFix: failed to replace {}.prototype.processList", kSetterPath);
				return;
			}

			RE::GFxValue wrapped;
			wrapped.SetBoolean(true);
			if (!proto.SetMember(kMarker, wrapped)) {
				logger::warn("FavoritesCrashFix: failed to set {} marker", kSetterPath);
			}
			logger::info("FavoritesCrashFix: wrapped {}.processList", kSetterPath);
		}

		RE::UI_MESSAGE_RESULTS ProcessMessage(RE::FavoritesMenu* a_menu, RE::UIMessage& a_message)
		{
			if (a_menu && a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
				WrapProcessList(a_menu->uiMovie.get());
			}
			return OriginalProcessMessage()(a_menu, a_message);
		}
	}

	void Install()
	{
		static bool installed = false;
		if (installed) {
			return;
		}
		installed = true;

		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_FavoritesMenu[0] };
		if (!vtbl.address()) {  // unreachable at runtime; without it clang-analyzer models address() as 0 inside write_vfunc
			return;
		}
		OriginalProcessMessage() = vtbl.write_vfunc(0x4, ProcessMessage);
		logger::info("FavoritesCrashFix: hooked FavoritesMenu::ProcessMessage");
	}
}
