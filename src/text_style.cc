/*
 * Copyright 2017 Google, Inc.
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

#include "text_style.h"
#include "platform.h"

namespace txt {

TextStyle::TextStyle()
        : font_families(std::vector<std::string>(1, GetDefaultFontFamily())) {}

bool TextStyle::equals(const TextStyle& other) const {
    if (decoration_thickness_multiplier != other.decoration_thickness_multiplier)
        return false;
    if (font_weight != other.font_weight)
        return false;
    if (font_style != other.font_style)
        return false;
    if (letter_spacing != other.letter_spacing)
        return false;
    if (word_spacing != other.word_spacing)
        return false;
    if (height != other.height)
        return false;
    if (locale != other.locale)
        return false;
    for (size_t font_index = 0; font_index < font_families.size(); ++font_index) {
        if (font_families[font_index] != other.font_families[font_index])
            return false;
    }

    return true;
}

}  // namespace txt
