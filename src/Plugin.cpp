#include "PCH.h"

#include "ConsoleHook.h"
#include "FrameProbe.h"
#include "I4Hook.h"
#include "InvalidateListFix.h"
#include "InvalidateMemo.h"
#include "TrampolineBudget.h"

void SetupLog()
{
	auto logsFolder = logger::log_directory();
	if (!logsFolder) {
		util::report_and_fail("SKSE log_directory not provided, logs can't be written");
	}

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	const auto  logName = plugin ? std::string{ plugin->GetName() } + ".log" : "Plugin.log";
	auto        logPath = *logsFolder / logName;

	auto                          fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
	std::vector<spdlog::sink_ptr> sinks{ fileSink };
	if (IsDebuggerPresent()) {
		sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
	}

	auto spdlogger = std::make_shared<spdlog::logger>("global", sinks.begin(), sinks.end());
	spdlog::set_default_logger(std::move(spdlogger));
	spdlog::set_pattern("[%H:%M:%S.%e] [%l] [%s:%#] %v");
#ifdef NDEBUG
	spdlog::set_level(spdlog::level::info);
#else
	spdlog::set_level(spdlog::level::trace);
#endif
	spdlog::flush_on(spdlog::level::info);
}

const char* IconSetterForMenu(const RE::BSFixedString& a_name)
{
	if (a_name == RE::InventoryMenu::MENU_NAME ||
		a_name == RE::ContainerMenu::MENU_NAME ||
		a_name == RE::BarterMenu::MENU_NAME ||
		a_name == RE::GiftMenu::MENU_NAME) {
		return "_global.InventoryIconSetter";
	}
	if (a_name == RE::CraftingMenu::MENU_NAME) {
		return "_global.CraftingIconSetter";
	}
	if (a_name == RE::MagicMenu::MENU_NAME) {
		return "_global.MagicIconSetter";
	}
	return nullptr;
}

class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	RE::BSEventNotifyControl ProcessEvent(
		const RE::MenuOpenCloseEvent*               a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override
	{
		(void)a_source;
		if (!a_event || !a_event->opening) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const char* setterPath = IconSetterForMenu(a_event->menuName);
		if (!setterPath) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const auto menu = ui->GetMenu(a_event->menuName);
		if (!menu || !menu->uiMovie) {
			return RE::BSEventNotifyControl::kContinue;
		}

		InventoryInjectorImproved::I4Hook::Inject(menu->uiMovie.get(), setterPath);
		InventoryInjectorImproved::InvalidateListFix::Install(menu->uiMovie.get());
		InventoryInjectorImproved::InvalidateMemo::Install(menu->uiMovie.get());
		return RE::BSEventNotifyControl::kContinue;
	}
};

void OnDataLoaded()
{
	static MenuSink menuSink;
	if (auto* ui = RE::UI::GetSingleton()) {
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
	}

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	const auto  name = plugin ? plugin->GetName() : "Plugin";
	if (auto* const console = RE::ConsoleLog::GetSingleton()) {
		console->Print("[%s] Loaded successfully!", std::string(name).c_str());
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	if (!plugin) {
		logger::error("Failed to get plugin declaration");
		return false;
	}
	logger::info("{} v{} loaded", plugin->GetName(), plugin->GetVersion());

	const auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		logger::error("Failed to get SKSE messaging interface");
		return false;
	}

	if (!messaging->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
			switch (a_msg->type) {
			case SKSE::MessagingInterface::kDataLoaded:
				OnDataLoaded();
				break;
			default:
				break;
			}
		})) {
		logger::error("Failed to register messaging listener");
		return false;
	}

	const auto* const serialization = SKSE::GetSerializationInterface();
	if (!serialization) {
		logger::error("Failed to get SKSE serialization interface");
		return false;
	}
	serialization->SetUniqueID('IICH');
	serialization->SetSaveCallback([](SKSE::SerializationInterface* a_intfc) {
		InventoryInjectorImproved::I4Hook::Save(a_intfc);
	});
	serialization->SetLoadCallback([](SKSE::SerializationInterface* a_intfc) {
		InventoryInjectorImproved::I4Hook::Load(a_intfc);
	});
	serialization->SetRevertCallback([](SKSE::SerializationInterface*) {
		InventoryInjectorImproved::I4Hook::ClearCache();
	});

	/**
	 * Each AllocTrampoline call replaces this plugin's trampoline and can free the
	 * previous buffer out from under installed hooks, so allocate once for all of them.
	 */
	SKSE::AllocTrampoline(InventoryInjectorImproved::kTrampolineSize);

	InventoryInjectorImproved::ConsoleHook::Install();
	InventoryInjectorImproved::FrameProbe::Install();

	const auto& trampoline = SKSE::GetTrampoline();
	logger::info("trampoline: {} of {} bytes used", trampoline.allocated_size(), trampoline.capacity());

	return true;
}
