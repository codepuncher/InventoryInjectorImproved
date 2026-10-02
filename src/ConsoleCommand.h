#pragma once

#include <cctype>
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
		kPurge,
		kStatus,
		kUsage,
		kDebugOn,
		kDebugOff,
		kBypassOn,
		kBypassOff,
		kVerifyOn,
		kVerifyOff,
		kMemoOn,
		kMemoOff,
		kInvalidate
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
			std::uint32_t value = 0;
			for (const char c : a_token) {
				if (!std::isxdigit(static_cast<unsigned char>(c))) {
					return std::nullopt;
				}
				const auto digit = static_cast<std::uint32_t>(std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : c - 'a' + 10);
				value = (value << 4) | digit;
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
	}

	struct ParsedCommand
	{
		Action        action{ Action::kNone };
		std::uint32_t formID{ 0 };
	};

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
		if (tokens.size() == 2 && tokens[1] == "purge") {
			return { .action = Action::kPurge };
		}
		if (tokens.size() == 2 && tokens[1] == "status") {
			return { .action = Action::kStatus };
		}
		if (const auto action = detail::MatchToggle(tokens, "debug", Action::kDebugOn, Action::kDebugOff)) {
			return { .action = *action };
		}
		if (const auto action = detail::MatchToggle(tokens, "bypass", Action::kBypassOn, Action::kBypassOff)) {
			return { .action = *action };
		}
		if (const auto action = detail::MatchToggle(tokens, "verify", Action::kVerifyOn, Action::kVerifyOff)) {
			return { .action = *action };
		}
		if (const auto action = detail::MatchToggle(tokens, "memo", Action::kMemoOn, Action::kMemoOff)) {
			return { .action = *action };
		}
		if (tokens.size() == 3 && tokens[1] == "invalidate") {
			if (const auto formID = detail::TryParseHexFormID(tokens[2])) {
				return { .action = Action::kInvalidate, .formID = *formID };
			}
		}
		return { .action = Action::kUsage };
	}

	[[nodiscard]] inline Action ParseCommand(std::string_view a_line)
	{
		return Classify(a_line).action;
	}
}
