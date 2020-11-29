/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "font_collection.h"

#include <algorithm>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include "platform.h"
#include "Typeface.h"
#include "LayoutFont.h"

namespace txt {

namespace {

const std::shared_ptr<minikin::FontFamily> g_null_family;

}  // anonymous namespace

FontCollection::FamilyKey::FamilyKey(const std::vector<std::string>& families,
                                     const std::string& loc) {
    locale = loc;

    std::stringstream stream;
    for_each(families.begin(), families.end(),
             [&stream](const std::string& str) { stream << str << ','; });
    font_families = stream.str();
}

bool FontCollection::FamilyKey::operator==(
        const FontCollection::FamilyKey& other) const {
    return font_families == other.font_families && locale == other.locale;
}

size_t FontCollection::FamilyKey::Hasher::operator()(
        const FontCollection::FamilyKey& key) const {
    return std::hash<std::string>()(key.font_families) ^
           std::hash<std::string>()(key.locale);
}

class TxtFallbackFontProvider
        : public minikin::FontCollection::FallbackFontProvider {
public:
    TxtFallbackFontProvider(std::shared_ptr<FontCollection> font_collection)
            : font_collection_(font_collection) {}

    virtual const std::shared_ptr<minikin::FontFamily>& matchFallbackFont(
            uint32_t ch,
            std::string locale) {
        std::shared_ptr<FontCollection> fc = font_collection_.lock();
        if (fc) {
            return fc->MatchFallbackFont(ch, locale);
        } else {
            return g_null_family;
        }
    }

private:
    std::weak_ptr<FontCollection> font_collection_;
};

FontCollection::FontCollection() : enable_font_fallback_(true) { SetupDefaultFontManager(); }

FontCollection::~FontCollection() = default;


void FontCollection::DisableFontFallback() {
    enable_font_fallback_ = false;
}

std::shared_ptr<minikin::FontCollection>
FontCollection::GetMinikinFontCollectionForFamilies(
        const std::vector<std::string>& font_families,
        const std::string& locale) {
    // Look inside the font collections cache first.
    FamilyKey family_key(font_families, locale);
    auto cached = font_collections_cache_.find(family_key);
    if (cached != font_collections_cache_.end()) {
        return cached->second;
    }

    std::vector<std::shared_ptr<minikin::FontFamily>> minikin_families;

    // Search for all user provided font families.
    for (size_t fallback_index = 0; fallback_index < font_families.size();
         fallback_index++) {
        std::shared_ptr<minikin::FontFamily> minikin_family =
                FindFontFamilyInManagers(font_families[fallback_index]);
        if (minikin_family != nullptr) {
            minikin_families.push_back(minikin_family);
        }
    }
    // Search for default font family if no user font families were found.
    if (minikin_families.empty()) {
        const auto default_font_family = GetDefaultFontFamily();
        std::shared_ptr<minikin::FontFamily> minikin_family =
                FindFontFamilyInManagers(default_font_family);
        if (minikin_family != nullptr) {
            minikin_families.push_back(minikin_family);
        }
    }
    // Default font family also not found. We fail to get a FontCollection.
    if (minikin_families.empty()) {
        return nullptr;
    }
    if (enable_font_fallback_) {
        for (std::string fallback_family : fallback_fonts_for_locale_[locale]) {
            auto it = fallback_fonts_.find(fallback_family);
            if (it != fallback_fonts_.end()) {
                minikin_families.push_back(it->second);
            }
        }
    }
    // Create the minikin font collection.
    auto font_collection =
            std::make_shared<minikin::FontCollection>(std::move(minikin_families));
    if (enable_font_fallback_) {
        font_collection->set_fallback_font_provider(
                std::make_unique<TxtFallbackFontProvider>(shared_from_this()));
    }

    // Cache the font collection for future queries.
    font_collections_cache_[family_key] = font_collection;

    return font_collection;
}

std::shared_ptr<minikin::FontFamily> FontCollection::FindFontFamilyInManagers(
        const std::string& family_name) {
    // Search for the font family in each font font_manager_.
    std::shared_ptr<minikin::FontFamily> minikin_family =
            CreateMinikinFontFamily(font_manager_, family_name);
    return minikin_family;
}

std::shared_ptr<minikin::FontFamily> FontCollection::CreateMinikinFontFamily(
        const FontManager* manager,
        const std::string& family_name) {


    FontStyleSet* fss = manager->matchFamily(family_name.c_str());
    if (fss == nullptr || fss->count() == 0) {
        return nullptr;
    }

    std::vector<minikin::Font> minikin_fonts;
    // Add fonts to the Minikin font family.
    for (int i = 0; i < fss->count(); ++i) {
        // Create the typeface.
        Typeface* typeface = fss->createTypeface(i);
        if (typeface == nullptr) {
            continue;
        }

        minikin::Font minikin_font(
                std::make_shared<LayoutFont>(typeface),
                minikin::FontStyle{typeface->fontStyle().weight() / 100,
                                   typeface->fontStyle().isItalic()});

        minikin_fonts.emplace_back(std::move(minikin_font));
    }

    return std::make_shared<minikin::FontFamily>(std::move(minikin_fonts));
}

const std::shared_ptr<minikin::FontFamily>& FontCollection::MatchFallbackFont(
        uint32_t ch,
        std::string locale) {
    // Check if the ch's matched font has been cached. We cache the results of
    // this method as repeated matchFamilyStyleCharacter calls can become
    // extremely laggy when typing a large number of complex emojis.
    auto lookup = fallback_match_cache_.find(ch);
    if (lookup != fallback_match_cache_.end()) {
        return *lookup->second;
    }
    const std::shared_ptr<minikin::FontFamily>* match =
            &DoMatchFallbackFont(ch, locale);
    fallback_match_cache_.insert(std::make_pair(ch, match));
    return *match;
}

const std::shared_ptr<minikin::FontFamily>& FontCollection::DoMatchFallbackFont(
        uint32_t ch,
        std::string locale) {
    std::vector<const char*> bcp47;
    if (!locale.empty()) {
        bcp47.push_back(locale.c_str());
    }

    Typeface* typeface = font_manager_->matchFamilyStyleCharacter(
            0, FontStyle(), bcp47.data(), bcp47.size(), ch);
    if (!typeface) {
        return g_null_family;
    }

    std::string family_name = typeface->familyName();

    fallback_fonts_for_locale_[locale].insert(family_name);

    return GetFallbackFontFamily(font_manager_, family_name);

}

const std::shared_ptr<minikin::FontFamily>&
FontCollection::GetFallbackFontFamily(const FontManager* manager,
                                      const std::string& family_name) {
    auto fallback_it = fallback_fonts_.find(family_name);
    if (fallback_it != fallback_fonts_.end()) {
        return fallback_it->second;
    }

    std::shared_ptr<minikin::FontFamily> minikin_family =
            CreateMinikinFontFamily(manager, family_name);
    if (!minikin_family)
        return g_null_family;

    auto insert_it =
            fallback_fonts_.insert(std::make_pair(family_name, minikin_family));

    // Clear the cache to force creation of new font collections that will
    // include this fallback font.
    font_collections_cache_.clear();

    return insert_it.first->second;
}

void FontCollection::ClearFontFamilyCache() {
    font_collections_cache_.clear();
}

void FontCollection::SetupDefaultFontManager() {
    font_manager_ = FontManager::getInstance();
}

}  // namespace txt
