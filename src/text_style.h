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

#ifndef LIB_TXT_SRC_TEXT_STYLE_H_
#define LIB_TXT_SRC_TEXT_STYLE_H_

#include <string>
#include <vector>

namespace txt {
enum class FontItalic {
    normal,
    italic,
};
enum class FontWeight {
    w100,  // Thin
    w200,  // Extra-Light
    w300,  // Light
    w400,  // Normal/Regular
    w500,  // Medium
    w600,  // Semi-bold
    w700,  // Bold
    w800,  // Extra-Bold
    w900,  // Black
};

class TextStyle {
public:
    // Does not make sense to draw a transparent object, so we use it as a default
    // value to indicate no decoration color was set.
    // Thickness is applied as a multiplier to the default thickness of the font.
    double decoration_thickness_multiplier = 1.0;
    FontWeight font_weight = FontWeight::w400;
    FontItalic font_style = FontItalic::normal;
    // An ordered list of fonts in order of priority. The first font is more
    // highly preferred than the last font.
    std::vector<std::string> font_families;
    double font_size = 14.0;
    double letter_spacing = 0.0;
    double word_spacing = 0.0;
    double height = 1.0;
    std::string locale;

    TextStyle();

    bool equals(const TextStyle& other) const;
};

}  // namespace txt

#endif  // LIB_TXT_SRC_TEXT_STYLE_H_
