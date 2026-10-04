#include "PCH.h"

#include "FavoritesCrashFix.h"

#include "FavoritesHook.h"

#include "SkyUIConfig.h"

#include <algorithm>
#include <optional>
#include <string>

namespace InventoryInjectorImproved::FavoritesCrashFix
{
	namespace
	{
		constexpr auto kNoIconColors = "_noIconColors";

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

		void PresetStep(RE::GFxFunctionHandler::Params& a_params)
		{
			PresetNoIconColors(a_params.thisPtr);
		}
	}

	void Install()
	{
		static bool installed = false;
		if (installed) {
			return;
		}
		installed = true;

		if (REL::Module::IsAtLeast(SKSE::RUNTIME_SSE_1_7_99)) {
			logger::info("FavoritesCrashFix: not needed on this runtime");
			return;
		}

		FavoritesHook::AddPreStep(PresetStep);
	}
}
