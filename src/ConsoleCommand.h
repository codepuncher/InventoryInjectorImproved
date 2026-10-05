#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace InventoryInjectorImproved::Console
{
	enum class Action
	{
		kNone,  // not our namespace; caller runs the original command
		kUsage,
		kDisableAll,
		kEnableAll,
		kStatus,
		kDebugOn,
		kDebugOff,
		kVerifyOn,
		kVerifyOff,
		kDisableCache,
		kEnableCache,
		kPurge,
		kMemoOn,
		kMemoOff
	};

	namespace detail
	{
		/**
		 * Lowercase + whitespace-split a line into tokens (portable; no platform CRT).
		 */
		inline std::vector<std::string> Tokenize(std::string_view a_line)
		{
			std::vector<std::string> tokens;
			std::string              current;
			for (const char c : a_line) {
				if (std::isspace(static_cast<unsigned char>(c))) {
					if (!current.empty()) {
						tokens.push_back(std::move(current));
						current.clear();
					}
					continue;
				}
				current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			}
			if (!current.empty()) {
				tokens.push_back(std::move(current));
			}
			return tokens;
		}
	}

	/**
	 * Classify a typed console line. First token != "i5" yields kNone (caller
	 * falls through to the original command). Subcommands must match exactly.
	 */
	[[nodiscard]] inline Action ParseCommand(std::string_view a_line)
	{
		const auto tokens = detail::Tokenize(a_line);
		if (tokens.empty() || tokens.front() != "i5") {
			return Action::kNone;
		}
		if (tokens.size() == 2 && tokens[1] == "disable") {
			return Action::kDisableAll;
		}
		if (tokens.size() == 2 && tokens[1] == "enable") {
			return Action::kEnableAll;
		}
		if (tokens.size() == 2 && tokens[1] == "status") {
			return Action::kStatus;
		}
		if (tokens.size() == 3 && tokens[1] == "debug") {
			if (tokens[2] == "on") {
				return Action::kDebugOn;
			}
			if (tokens[2] == "off") {
				return Action::kDebugOff;
			}
		}
		if (tokens.size() == 3 && tokens[1] == "verify") {
			if (tokens[2] == "on") {
				return Action::kVerifyOn;
			}
			if (tokens[2] == "off") {
				return Action::kVerifyOff;
			}
		}
		if (tokens.size() == 3 && tokens[1] == "cache" && tokens[2] == "disable") {
			return Action::kDisableCache;
		}
		if (tokens.size() == 3 && tokens[1] == "cache" && tokens[2] == "enable") {
			return Action::kEnableCache;
		}
		if (tokens.size() == 3 && tokens[1] == "cache" && tokens[2] == "purge") {
			return Action::kPurge;
		}
		if (tokens.size() == 4 && tokens[1] == "skyui" && tokens[2] == "inventory-dedupe") {
			if (tokens[3] == "enable") {
				return Action::kMemoOn;
			}
			if (tokens[3] == "disable") {
				return Action::kMemoOff;
			}
		}
		return Action::kUsage;
	}
}
