#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

namespace InventoryInjectorImproved::SkyUIConfig
{
	inline bool IEquals(std::string_view a_lhs, std::string_view a_rhs)
	{
		return std::ranges::equal(a_lhs, a_rhs, [](unsigned char a_l, unsigned char a_r) {
			return std::tolower(a_l) == std::tolower(a_r);
		});
	}

	/**
	 * Read `icons.item.noColor` from the text of SkyUI's `interface/skyui/config.txt` with I4's
	 * lookup and trimming: the first occurrence of the key after `[Appearance]`, the value after
	 * `=` with leading spaces/`=` and trailing spaces/CR/LF removed. Empty when the section, key
	 * or `=` is missing.
	 */
	inline std::optional<bool> ParseNoIconColors(std::string_view a_text)
	{
		const auto section = a_text.find("[Appearance]");
		if (section == std::string_view::npos) {
			return std::nullopt;
		}
		const auto key = a_text.find("icons.item.noColor", section);
		if (key == std::string_view::npos) {
			return std::nullopt;
		}
		const auto eq = a_text.find('=', key);
		if (eq == std::string_view::npos) {
			return std::nullopt;
		}

		auto       value = a_text.substr(eq);
		const auto eol = value.find('\n');
		if (eol != std::string_view::npos) {
			value = value.substr(0, eol);
		}

		const auto first = value.find_first_not_of(" =");
		if (first == std::string_view::npos) {
			return false;
		}
		value = value.substr(first);
		value = value.substr(0, value.find_last_not_of(" \r\n") + 1);
		return IEquals(value, "true");
	}
}
