#pragma once

#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
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
		kMemoOff,
		kInvalidate,
	};

	struct ParsedCommand
	{
		Action        action{ Action::kNone };
		std::uint32_t formID{ 0 };
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

		[[nodiscard]] inline std::optional<std::uint32_t> TryParseHexFormID(std::string_view a_token)
		{
			if (a_token.size() > 2 && a_token[0] == '0' && a_token[1] == 'x') {
				a_token.remove_prefix(2);
			}
			if (a_token.empty() || a_token.size() > 8) {
				return std::nullopt;
			}
			std::uint32_t     value = 0;
			const char* const end = a_token.data() + a_token.size();
			const auto [ptr, ec] = std::from_chars(a_token.data(), end, value, 16);
			if (ec != std::errc{} || ptr != end) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] inline std::optional<Action> MatchToggle(
			const std::vector<std::string>& a_tokens, std::string_view a_name, Action a_on, Action a_off)
		{
			if (a_tokens.size() != 3 || a_tokens[1] != a_name) {
				return std::nullopt;
			}
			if (a_tokens[2] == "on") {
				return a_on;
			}
			if (a_tokens[2] == "off") {
				return a_off;
			}
			return std::nullopt;
		}

		[[nodiscard]] inline std::optional<Action> MatchCacheCommand(const std::vector<std::string>& a_tokens)
		{
			if (a_tokens.size() != 3 || a_tokens[1] != "cache") {
				return std::nullopt;
			}
			if (a_tokens[2] == "disable") {
				return Action::kDisableCache;
			}
			if (a_tokens[2] == "enable") {
				return Action::kEnableCache;
			}
			if (a_tokens[2] == "purge") {
				return Action::kPurge;
			}
			return std::nullopt;
		}

		[[nodiscard]] inline std::optional<Action> MatchMemoToggle(const std::vector<std::string>& a_tokens)
		{
			if (a_tokens.size() != 4 || a_tokens[1] != "skyui" || a_tokens[2] != "inventory-dedupe") {
				return std::nullopt;
			}
			if (a_tokens[3] == "enable") {
				return Action::kMemoOn;
			}
			if (a_tokens[3] == "disable") {
				return Action::kMemoOff;
			}
			return std::nullopt;
		}

		[[nodiscard]] inline std::optional<ParsedCommand> MatchInvalidate(const std::vector<std::string>& a_tokens)
		{
			if (a_tokens.size() != 3 || a_tokens[1] != "invalidate") {
				return std::nullopt;
			}
			if (const auto formID = TryParseHexFormID(a_tokens[2])) {
				return ParsedCommand{ .action = Action::kInvalidate, .formID = *formID };
			}
			return std::nullopt;
		}
	}

	/**
	 * Classify a typed console line. First token != "i5" yields kNone (caller
	 * falls through to the original command). Subcommands must match exactly.
	 */
	[[nodiscard]] inline ParsedCommand Classify(std::string_view a_line)
	{
		const auto tokens = detail::Tokenize(a_line);
		if (tokens.empty() || tokens.front() != "i5") {
			return { .action = Action::kNone };
		}
		if (tokens.size() == 2 && tokens[1] == "disable") {
			return { .action = Action::kDisableAll };
		}
		if (tokens.size() == 2 && tokens[1] == "enable") {
			return { .action = Action::kEnableAll };
		}
		if (tokens.size() == 2 && tokens[1] == "status") {
			return { .action = Action::kStatus };
		}
		if (const auto action = detail::MatchToggle(tokens, "debug", Action::kDebugOn, Action::kDebugOff)) {
			return { .action = *action };
		}
		if (const auto action = detail::MatchToggle(tokens, "verify", Action::kVerifyOn, Action::kVerifyOff)) {
			return { .action = *action };
		}
		if (const auto action = detail::MatchCacheCommand(tokens)) {
			return { .action = *action };
		}
		if (const auto parsed = detail::MatchInvalidate(tokens)) {
			return *parsed;
		}
		if (const auto action = detail::MatchMemoToggle(tokens)) {
			return { .action = *action };
		}
		return { .action = Action::kUsage };
	}

	[[nodiscard]] inline Action ParseCommand(std::string_view a_line)
	{
		return Classify(a_line).action;
	}
}
