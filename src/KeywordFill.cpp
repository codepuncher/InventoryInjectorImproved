#include "PCH.h"

#include "CacheKey.h"
#include "GFxArrayUtil.h"
#include "KeywordFill.h"

#include <cmath>
#include <unordered_map>  // NOLINT(readability-duplicate-include): already included by PCH.h

namespace InventoryInjectorImproved
{
	namespace
	{
		RE::BGSKeywordForm* FindKeywordForm(RE::FormID a_formId)
		{
			static std::unordered_map<RE::FormID, RE::BGSKeywordForm*> cache;

			const bool dynamic = IsDynamicForm(a_formId);
			if (dynamic) {
				return skyrim_cast<RE::BGSKeywordForm*>(RE::TESForm::LookupByID(a_formId));
			}

			if (const auto it = cache.find(a_formId); it != cache.end()) {
				return it->second;
			}
			auto* const keywordForm = skyrim_cast<RE::BGSKeywordForm*>(RE::TESForm::LookupByID(a_formId));
			cache.emplace(a_formId, keywordForm);
			return keywordForm;
		}

		void FillOne(RE::GFxMovie* a_movie, RE::GFxValue& a_entry)
		{
			if (a_entry.HasMember("keywords")) {
				return;
			}

			RE::GFxValue formId;
			a_entry.GetMember("formId", &formId);

			const double formIdValue = formId.GetNumber();
			const auto   maxFormId = static_cast<double>(~RE::FormID{ 0 });
			if (!std::isfinite(formIdValue) || formIdValue < 0.0 || formIdValue > maxFormId) {
				return;
			}

			auto* const keywordForm = FindKeywordForm(static_cast<RE::FormID>(formIdValue));
			if (!keywordForm) {
				return;
			}

			RE::GFxValue keywords;
			a_movie->CreateObject(&keywords);

			for (std::uint32_t i = 0; i < keywordForm->GetNumKeywords(); i++) {
				auto* const keyword = keywordForm->GetKeywordAt(i).value_or(nullptr);
				if (!keyword) {
					continue;
				}

				const auto* const editorId = keyword->GetFormEditorID();
				if (!editorId || *editorId == '\0') {
					continue;
				}

				keywords.SetMember(editorId, true);
			}

			a_entry.SetMember("keywords", keywords);
		}
	}

	void FillMissingKeywords(RE::GFxMovie* a_movie, RE::GFxValue& a_entryList)
	{
		if (!a_movie || !a_entryList.IsArray()) {
			return;
		}

		const auto count = a_entryList.GetArraySize();
		for (std::uint32_t i = 0; i < count; i++) {
			RE::GFxValue entry;
			if (TryGetObjectElement(a_entryList, i, entry)) {
				FillOne(a_movie, entry);
			}
		}
	}
}
