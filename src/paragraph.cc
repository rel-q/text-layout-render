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

#include "paragraph.h"

#include <hb.h>
#include <algorithm>
#include <limits>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "../minikin/Layout.h"
#include "../minikin/FontLanguageListCache.h"
#include "../minikin/GraphemeBreak.h"
#include "../minikin/HbFontCache.h"
#include "../minikin/LayoutUtils.h"
#include "../minikin/LineBreaker.h"
#include "../minikin/MinikinFont.h"
#include "unicode/ubidi.h"
#include "unicode/utf16.h"
#include "LayoutFont.h"

#include <iostream>

namespace txt {
namespace {

class GlyphTypeface {
public:
    GlyphTypeface(Typeface* typeface, minikin::FontFakery fakery)
            : typeface_(std::move(typeface)),
              fake_bold_(fakery.isFakeBold()),
              fake_italic_(fakery.isFakeItalic()) {}

    bool operator==(GlyphTypeface& other) {
        return other.typeface_ == typeface_ &&
               other.fake_bold_ == fake_bold_ && other.fake_italic_ == fake_italic_;
    }

    bool operator!=(GlyphTypeface& other) { return !(*this == other); }

    Typeface* typeface() const {
        return typeface_;
    }

private:
    Typeface* typeface_;
    bool fake_bold_;
    bool fake_italic_;
};

GlyphTypeface GetGlyphTypeface(const minikin::Layout& layout, size_t index) {
    const LayoutFont* font = static_cast<const LayoutFont*>(layout.getFont(index));
    return GlyphTypeface(font->typeface(), layout.getFakery(index));
}

// Return ranges of text that have the same typeface in the layout.
std::vector<Paragraph::Range<size_t>> GetLayoutTypefaceRuns(
        const minikin::Layout& layout) {
    std::vector<Paragraph::Range<size_t>> result;
    if (layout.nGlyphs() == 0)
        return result;
    size_t run_start = 0;
    GlyphTypeface run_typeface = GetGlyphTypeface(layout, run_start);
    for (size_t i = 1; i < layout.nGlyphs(); ++i) {
        GlyphTypeface typeface = GetGlyphTypeface(layout, i);
        if (typeface != run_typeface) {
            result.emplace_back(run_start, i);
            run_start = i;
            run_typeface = typeface;
        }
    }
    result.emplace_back(run_start, layout.nGlyphs());
    return result;
}

int GetWeight(const FontWeight weight) {
    switch (weight) {
        case FontWeight::w100:
            return 1;
        case FontWeight::w200:
            return 2;
        case FontWeight::w300:
            return 3;
        case FontWeight::w400:  // Normal.
            return 4;
        case FontWeight::w500:
            return 5;
        case FontWeight::w600:
            return 6;
        case FontWeight::w700:  // Bold.
            return 7;
        case FontWeight::w800:
            return 8;
        case FontWeight::w900:
            return 9;
        default:
            return -1;
    }
}

int GetWeight(const TextStyle& style) {
    return GetWeight(style.font_weight);
}

bool GetItalic(const TextStyle& style) {
    switch (style.font_style) {
        case FontItalic::italic:
            return true;
        case FontItalic::normal:
        default:
            return false;
    }
}

minikin::FontStyle GetMinikinFontStyle(const TextStyle& style) {
    uint32_t language_list_id =
            style.locale.empty()
            ? minikin::FontLanguageListCache::kEmptyListId
            : minikin::FontStyle::registerLanguageList(style.locale);
    return minikin::FontStyle(language_list_id, 0, GetWeight(style),
                              GetItalic(style));
}

void GetFontAndMinikinPaint(const TextStyle& style,
                            minikin::FontStyle* font,
                            minikin::MinikinPaint* paint) {
    *font = GetMinikinFontStyle(style);
    paint->size = style.font_size;
    // Divide by font size so letter spacing is pixels, not proportional to font
    // size.
    paint->letterSpacing = style.letter_spacing / style.font_size;
    paint->wordSpacing = style.word_spacing;
    paint->scaleX = 1.0f;
    // Prevent spacing rounding in Minikin. This causes jitter when switching
    // between same text content with different runs composing it, however, it
    // also produces more accurate layouts.
    paint->paintFlags |= minikin::LinearTextFlag;
}

void FindWords(const std::vector<uint16_t>& text,
               size_t start,
               size_t end,
               std::vector<Paragraph::Range<size_t>>* words) {
    bool in_word = false;
    size_t word_start;
    for (size_t i = start; i < end; ++i) {
        bool is_space = minikin::isWordSpace(text[i]);
        if (!in_word && !is_space) {
            word_start = i;
            in_word = true;
        } else if (in_word && is_space) {
            words->emplace_back(word_start, i);
            in_word = false;
        }
    }
    if (in_word)
        words->emplace_back(word_start, end);
}

}  // namespace

static const float kDoubleDecorationSpacing = 3.0f;

Paragraph::GlyphPosition::GlyphPosition(double x_start,
                                        double x_advance,
                                        size_t code_unit_index,
                                        size_t code_unit_width)
        : code_units(code_unit_index, code_unit_index + code_unit_width),
          x_pos(x_start, x_start + x_advance) {}

void Paragraph::GlyphPosition::Shift(double delta) {
    x_pos.Shift(delta);
}

Paragraph::GlyphLine::GlyphLine(std::vector<GlyphPosition>&& p, size_t tcu)
        : positions(std::move(p)), total_code_units(tcu) {}

Paragraph::CodeUnitRun::CodeUnitRun(std::vector<GlyphPosition>&& p,
                                    Range<size_t> cu,
                                    Range<double> x,
                                    size_t line,
                                    const FontMetrics& metrics,
                                    TextDirection dir)
        : positions(std::move(p)),
          code_units(cu),
          x_pos(x),
          line_number(line),
          font_metrics(metrics),
          direction(dir) {}

void Paragraph::CodeUnitRun::Shift(double delta) {
    x_pos.Shift(delta);
    for (GlyphPosition& position : positions)
        position.Shift(delta);
}

Paragraph::Paragraph() {
    breaker_.setLocale(icu::Locale(), nullptr);
}

Paragraph::~Paragraph() = default;

void Paragraph::SetText(std::vector<uint16_t> text, StyledRuns runs) {
    needs_layout_ = true;
    if (text.size() == 0)
        return;
    text_ = std::move(text);
    runs_ = std::move(runs);
}

bool Paragraph::ComputeLineBreaks() {
    line_ranges_.clear();
    line_widths_.clear();
    max_intrinsic_width_ = 0;

    std::vector<size_t> newline_positions;
    // 新开一行的 LF 和 BK 分割字符判断
    for (size_t i = 0; i < text_.size(); ++i) {
        ULineBreak ulb = static_cast<ULineBreak>(
                u_getIntPropertyValue(text_[i], UCHAR_LINE_BREAK));
        if (ulb == U_LB_LINE_FEED || ulb == U_LB_MANDATORY_BREAK)
            newline_positions.push_back(i);
    }
    newline_positions.push_back(text_.size());

    size_t run_index = 0;
    for (size_t newline_index = 0; newline_index < newline_positions.size();
         ++newline_index) {
        size_t block_start =
                (newline_index > 0) ? newline_positions[newline_index - 1] + 1 : 0;
        size_t block_end = newline_positions[newline_index];
        size_t block_size = block_end - block_start;

        // first line
        if (block_size == 0) {
            line_ranges_.emplace_back(block_start, block_end, block_end,
                                      block_end + 1, true);
            line_widths_.push_back(0);
            continue;
        }

        breaker_.setLineWidths(0.0f, 0, width_);
        breaker_.setJustified(paragraph_style_.text_align == TextAlign::justify);
        breaker_.setStrategy(paragraph_style_.break_strategy);
        breaker_.resize(block_size);
        memcpy(breaker_.buffer(), text_.data() + block_start,
               block_size * sizeof(text_[0]));
        breaker_.setText();

        // Add the runs that include this line to the LineBreaker.
        double block_total_width = 0;
        while (run_index < runs_.size()) {
            StyledRuns::Run run = runs_.GetRun(run_index);
            if (run.start >= block_end)
                break;  // style run 全部在当前 line 之前，跳出
            if (run.end < block_start) {
                run_index++; // 继续下一个 style run
                continue;
            }

            minikin::FontStyle font;
            minikin::MinikinPaint paint;
            GetFontAndMinikinPaint(run.style, &font, &paint);
            std::shared_ptr<minikin::FontCollection> collection =
                    GetMinikinFontCollectionForStyle(run.style);
            if (collection == nullptr) {
                std::cerr << "Could not find font collection for families \""
                          << (run.style.font_families.empty()
                              ? ""
                              : run.style.font_families[0])
                          << "\".";
                return false;
            }
            // 最小片段
            size_t run_start = std::max(run.start, block_start) - block_start;
            size_t run_end = std::min(run.end, block_end) - block_start;
            bool isRtl = (paragraph_style_.text_direction == TextDirection::rtl);
            // 片段宽度
            double run_width = breaker_.addStyleRun(&paint, collection, font,
                                                    run_start, run_end, isRtl);
            block_total_width += run_width;

            if (run.end > block_end)
                break; // style run 当前 line 之后跳出
            run_index++;
        }

        max_intrinsic_width_ = std::max(max_intrinsic_width_, block_total_width);

        size_t breaks_count = breaker_.computeBreaks();

        const int* breaks = breaker_.getBreaks();
        for (size_t i = 0; i < breaks_count; ++i) {
            size_t break_start = (i > 0) ? breaks[i - 1] : 0;
            size_t line_start = break_start + block_start;
            size_t line_end = breaks[i] + block_start;
            bool hard_break = i == breaks_count - 1;
            size_t line_end_including_newline =
                    (hard_break && line_end < text_.size()) ? line_end + 1 : line_end;
            size_t line_end_excluding_whitespace = line_end;
            while (
                    line_end_excluding_whitespace > line_start &&
                    minikin::isLineEndSpace(text_[line_end_excluding_whitespace - 1])) {
                line_end_excluding_whitespace--;
            }
            line_ranges_.emplace_back(line_start, line_end,
                                      line_end_excluding_whitespace,
                                      line_end_including_newline, hard_break);
            line_widths_.push_back(breaker_.getWidths()[i]);
        }

        breaker_.finish();
    }

    return true;
}

bool Paragraph::ComputeBidiRuns(std::vector<BidiRun>* result) {
    if (text_.empty())
        return true;

    auto ubidi_closer = [](UBiDi* b) { ubidi_close(b); };
    std::unique_ptr<UBiDi, decltype(ubidi_closer)> bidi(ubidi_open(),
                                                        ubidi_closer);
    if (!bidi)
        return false;

    UBiDiLevel paraLevel = (paragraph_style_.text_direction == TextDirection::rtl)
                           ? UBIDI_RTL
                           : UBIDI_LTR;
    UErrorCode status = U_ZERO_ERROR;
    ubidi_setPara(bidi.get(), reinterpret_cast<const UChar*>(text_.data()),
                  text_.size(), paraLevel, nullptr, &status);
    if (!U_SUCCESS(status))
        return false;

    int32_t bidi_run_count = ubidi_countRuns(bidi.get(), &status);
    if (!U_SUCCESS(status))
        return false;

    // Build a map of styled runs indexed by start position.
    std::map<size_t, StyledRuns::Run> styled_run_map;
    for (size_t i = 0; i < runs_.size(); ++i) {
        StyledRuns::Run run = runs_.GetRun(i);
        styled_run_map.emplace(std::make_pair(run.start, run));
    }

    for (int32_t bidi_run_index = 0; bidi_run_index < bidi_run_count;
         ++bidi_run_index) {
        int32_t bidi_run_start, bidi_run_length;
        UBiDiDirection direction = ubidi_getVisualRun(
                bidi.get(), bidi_run_index, &bidi_run_start, &bidi_run_length);
        if (!U_SUCCESS(status))
            return false;

        // Exclude the leading bidi control character if present.
        UChar32 first_char;
        U16_GET(text_.data(), 0, bidi_run_start, static_cast<int>(text_.size()),
                first_char);
        if (u_hasBinaryProperty(first_char, UCHAR_BIDI_CONTROL)) {
            bidi_run_start++;
            bidi_run_length--;
        }
        if (bidi_run_length == 0)
            continue;

        // Exclude the trailing bidi control character if present.
        UChar32 last_char;
        U16_GET(text_.data(), 0, bidi_run_start + bidi_run_length - 1,
                static_cast<int>(text_.size()), last_char);
        if (u_hasBinaryProperty(last_char, UCHAR_BIDI_CONTROL)) {
            bidi_run_length--;
        }
        if (bidi_run_length == 0)
            continue;

        size_t bidi_run_end = bidi_run_start + bidi_run_length;
        TextDirection text_direction =
                direction == UBIDI_RTL ? TextDirection::rtl : TextDirection::ltr;

        // Break this bidi run into chunks based on text style.
        std::vector<BidiRun> chunks;
        size_t chunk_start = bidi_run_start;
        while (chunk_start < bidi_run_end) {
            auto styled_run_iter = styled_run_map.upper_bound(chunk_start);
            styled_run_iter--;
            const StyledRuns::Run& styled_run = styled_run_iter->second;
            size_t chunk_end = std::min(bidi_run_end, styled_run.end);
            chunks.emplace_back(chunk_start, chunk_end, text_direction,
                                styled_run.style);
            chunk_start = chunk_end;
        }

        if (text_direction == TextDirection::ltr) {
            result->insert(result->end(), chunks.begin(), chunks.end());
        } else {
            result->insert(result->end(), chunks.rbegin(), chunks.rend());
        }
    }

    return true;
}

void Paragraph::ComputeStrut(StrutMetrics* strut) {
    strut->ascent = 0;
    strut->descent = 0;
    strut->leading = 0;
    strut->half_leading = 0;
    strut->line_height = 0;
    strut->force_strut = false;

    // Font size must be positive.
    bool valid_strut =
            paragraph_style_.strut_enabled && paragraph_style_.strut_font_size >= 0;
    if (!valid_strut) {
        return;
    }
    // force_strut makes all lines have exactly the strut metrics, and ignores all
    // actual metrics. We only force the strut if the strut is non-zero and valid.
    strut->force_strut = paragraph_style_.force_strut_height && valid_strut;
    minikin::FontStyle minikin_font_style(
            0, GetWeight(paragraph_style_.strut_font_weight),
            paragraph_style_.strut_font_style == FontItalic::italic);

    std::shared_ptr<minikin::FontCollection> collection =
            font_collection_->GetMinikinFontCollectionForFamilies(
                    paragraph_style_.strut_font_families, "");
    if (!collection) {
        return;
    }
    minikin::FakedFont faked_font = collection->baseFontFaked(minikin_font_style);

    if (faked_font.font != nullptr) {
        LayoutFont* font = static_cast<LayoutFont*>(faked_font.font);
        Typeface* typeface = font->typeface();
        typeface->setSize(paragraph_style_.strut_font_size);

        strut->ascent = paragraph_style_.strut_height * -typeface->ascent();
        strut->descent = paragraph_style_.strut_height * typeface->descent();
        strut->leading =
                // Use font's leading if there is no user specified strut leading.
                paragraph_style_.strut_leading < 0
                ? typeface->leading()
                : (paragraph_style_.strut_leading *
                   (typeface->descent() - typeface->ascent()));
        strut->half_leading = strut->leading / 2;
        strut->line_height = strut->ascent + strut->descent + strut->leading;
    }
}

void Paragraph::Layout(double width, bool force) {
    // Do not allow calling layout multiple times without changing anything.
    if (!needs_layout_ && width == width_ && !force) {
        return;
    }
    needs_layout_ = false;

    width_ = floor(width);

    if (!ComputeLineBreaks())
        return;

    std::vector<BidiRun> bidi_runs;
    if (!ComputeBidiRuns(&bidi_runs))
        return;

    line_heights_.clear();
    line_baselines_.clear();
    glyph_lines_.clear();
    code_unit_runs_.clear();
    line_max_spacings_.clear();
    line_max_descent_.clear();
    line_max_ascent_.clear();
    max_right_ = FLT_MIN;
    min_left_ = FLT_MAX;
    records_.clear();

    minikin::Layout layout;
    double y_offset = 0;
    double prev_max_descent = 0;
    double max_word_width = 0;

    // Compute strut minimums according to paragraph_style_.
    StrutMetrics strut;
    ComputeStrut(&strut);

    // Paragraph bounds tracking.
    size_t line_limit = std::min(paragraph_style_.max_lines, line_ranges_.size());
    did_exceed_max_lines_ = (line_ranges_.size() > paragraph_style_.max_lines);

    for (size_t line_number = 0; line_number < line_limit; ++line_number) {
        const LineRange& line_range = line_ranges_[line_number];

        // Break the line into words if justification should be applied.
        std::vector<Range<size_t>> words;
        double word_gap_width = 0;
        size_t word_index = 0;
        bool justify_line =
                (paragraph_style_.text_align == TextAlign::justify &&
                 line_number != line_limit - 1 && !line_range.hard_break);
        FindWords(text_, line_range.start, line_range.end, &words);
        if (justify_line) {
            if (words.size() > 1) {
                word_gap_width =
                        (width_ - line_widths_[line_number]) / (words.size() - 1);
            }
        }

        // Exclude trailing whitespace from justified lines so the last visible
        // character in the line will be flush with the right margin.
        size_t line_end_index =
                (paragraph_style_.effective_align() == TextAlign::right ||
                 paragraph_style_.effective_align() == TextAlign::center ||
                 paragraph_style_.effective_align() == TextAlign::justify)
                ? line_range.end_excluding_whitespace
                : line_range.end;

        // Find the runs comprising this line.
        std::vector<BidiRun> line_runs;
        for (const BidiRun& bidi_run : bidi_runs) {
            if (bidi_run.start() < line_end_index &&
                bidi_run.end() > line_range.start) {
                line_runs.emplace_back(std::max(bidi_run.start(), line_range.start),
                                       std::min(bidi_run.end(), line_end_index),
                                       bidi_run.direction(), bidi_run.style());
            }
            // A "ghost" run is a run that does not impact the layout, breaking,
            // alignment, width, etc but is still "visible" though getRectsForRange.
            // For example, trailing whitespace on centered text can be scrolled
            // through with the caret but will not wrap the line.
            //
            // Here, we add an additional run for the whitespace, but dont
            // let it impact metrics. After layout of the whitespace run, we do not
            // add its width into the x-offset adjustment, effectively nullifying its
            // impact on the layout.
            if (paragraph_style_.ellipsis.empty() &&
                line_range.end_excluding_whitespace < line_range.end &&
                bidi_run.start() <= line_range.end &&
                bidi_run.end() > line_end_index) {
                line_runs.emplace_back(std::max(bidi_run.start(), line_end_index),
                                       std::min(bidi_run.end(), line_range.end),
                                       bidi_run.direction(), bidi_run.style(), true);
            }
        }

        bool line_runs_all_rtl =
                line_runs.size() &&
                std::accumulate(
                        line_runs.begin(), line_runs.end(), true,
                        [](const bool a, const BidiRun& b) { return a && b.is_rtl(); });
        if (line_runs_all_rtl) {
            std::reverse(words.begin(), words.end());
        }

        std::vector<GlyphPosition> line_glyph_positions;
        std::vector<CodeUnitRun> line_code_unit_runs;
        double run_x_offset = 0;
        double justify_x_offset = 0;
        std::vector<PaintRecord> paint_records;

        for (auto line_run_it = line_runs.begin(); line_run_it != line_runs.end();
             ++line_run_it) {
            const BidiRun& run = *line_run_it;
            minikin::FontStyle minikin_font;
            minikin::MinikinPaint minikin_paint;
            GetFontAndMinikinPaint(run.style(), &minikin_font, &minikin_paint);

            std::shared_ptr<minikin::FontCollection> minikin_font_collection =
                    GetMinikinFontCollectionForStyle(run.style());

            // Layout this run.
            uint16_t* text_ptr = text_.data();
            size_t text_start = run.start();
            size_t text_count = run.end() - run.start();
            size_t text_size = text_.size();

            layout.doLayout(text_ptr, text_start, text_count, text_size, run.is_rtl(),
                            minikin_font, minikin_paint, minikin_font_collection);

            if (layout.nGlyphs() == 0)
                continue;

            std::vector<float> layout_advances(text_count);
            layout.getAdvances(layout_advances.data());

            // Break the layout into blobs that share the same SkPaint parameters.
            std::vector<Range<size_t>> glyph_blobs = GetLayoutTypefaceRuns(layout);


            double word_start_position = std::numeric_limits<double>::quiet_NaN();

            // Build a text blob from each group of glyphs.
            for (const Range<size_t>& glyph_blob : glyph_blobs) {
                std::vector<GlyphPosition> glyph_positions;

                Typeface* typeface = GetGlyphTypeface(layout, glyph_blob.start).typeface();
                typeface->setSize(run.style().font_size);

                std::unique_ptr<RunBuffer> blob_buffer = std::make_unique<RunBuffer>();
                blob_buffer->typeface = typeface;
                blob_buffer->glyphs.resize(glyph_blob.end - glyph_blob.start);
                blob_buffer->pos.resize((glyph_blob.end - glyph_blob.start) * 2);

                double justify_x_offset_delta = 0;

                for (size_t glyph_index = glyph_blob.start;
                     glyph_index < glyph_blob.end;) {
                    size_t cluster_start_glyph_index = glyph_index;
                    uint32_t cluster = layout.getGlyphCluster(cluster_start_glyph_index);
                    double glyph_x_offset;

                    // Add all the glyphs in this cluster to the text blob.
                    do {
                        size_t blob_index = glyph_index - glyph_blob.start;
                        blob_buffer->glyphs[blob_index] = layout.getGlyphId(glyph_index);

                        size_t pos_index = blob_index * 2;
                        blob_buffer->pos[pos_index] =
                                layout.getX(glyph_index) + justify_x_offset_delta;
                        blob_buffer->pos[pos_index + 1] = layout.getY(glyph_index);

                        if (glyph_index == cluster_start_glyph_index)
                            glyph_x_offset = blob_buffer->pos[pos_index];

                        glyph_index++;
                    } while (glyph_index < glyph_blob.end &&
                             layout.getGlyphCluster(glyph_index) == cluster);

                    Range<int32_t> glyph_code_units(cluster, 0);
                    std::vector<size_t> grapheme_code_unit_counts;
                    if (run.is_rtl()) {
                        if (cluster_start_glyph_index > 0) {
                            glyph_code_units.end =
                                    layout.getGlyphCluster(cluster_start_glyph_index - 1);
                        } else {
                            glyph_code_units.end = text_count;
                        }
                        grapheme_code_unit_counts.push_back(glyph_code_units.width());
                    } else {
                        if (glyph_index < layout.nGlyphs()) {
                            glyph_code_units.end = layout.getGlyphCluster(glyph_index);
                        } else {
                            glyph_code_units.end = text_count;
                        }

                        // The glyph may be a ligature.  Determine how many graphemes are
                        // joined into this glyph and how many input code units map to
                        // each grapheme.
                        size_t code_unit_count = 1;
                        for (int32_t offset = glyph_code_units.start + 1;
                             offset < glyph_code_units.end; ++offset) {
                            if (minikin::GraphemeBreak::isGraphemeBreak(
                                    layout_advances.data(), text_ptr, text_start, text_count,
                                    offset)) {
                                grapheme_code_unit_counts.push_back(code_unit_count);
                                code_unit_count = 1;
                            } else {
                                code_unit_count++;
                            }
                        }
                        grapheme_code_unit_counts.push_back(code_unit_count);
                    }
                    float glyph_advance = layout.getCharAdvance(glyph_code_units.start);
                    float grapheme_advance =
                            glyph_advance / grapheme_code_unit_counts.size();

                    glyph_positions.emplace_back(run_x_offset + glyph_x_offset,
                                                 grapheme_advance,
                                                 run.start() + glyph_code_units.start,
                                                 grapheme_code_unit_counts[0]);

                    // Compute positions for the additional graphemes in the ligature.
                    for (size_t i = 1; i < grapheme_code_unit_counts.size(); ++i) {
                        glyph_positions.emplace_back(
                                glyph_positions.back().x_pos.end, grapheme_advance,
                                glyph_positions.back().code_units.start +
                                grapheme_code_unit_counts[i - 1],
                                grapheme_code_unit_counts[i]);
                    }

                    bool at_word_start = false;
                    bool at_word_end = false;
                    if (word_index < words.size()) {
                        at_word_start =
                                words[word_index].start == run.start() + glyph_code_units.start;
                        at_word_end =
                                words[word_index].end == run.start() + glyph_code_units.end;
                        if (line_runs_all_rtl) {
                            std::swap(at_word_start, at_word_end);
                        }
                    }

                    if (at_word_start) {
                        word_start_position = run_x_offset + glyph_x_offset;
                    }

                    if (at_word_end) {
                        if (justify_line) {
                            justify_x_offset_delta += word_gap_width;
                        }
                        word_index++;

                        if (!isnan(word_start_position)) {
                            double word_width =
                                    glyph_positions.back().x_pos.end - word_start_position;
                            max_word_width = std::max(word_width, max_word_width);
                            word_start_position = std::numeric_limits<double>::quiet_NaN();
                        }
                    }
                }  // for each in glyph_blob

                if (glyph_positions.empty())
                    continue;
                FontMetrics metrics;
                typeface->getMetrics(&metrics);

                Range<double> record_x_pos(
                        glyph_positions.front().x_pos.start - run_x_offset,
                        glyph_positions.back().x_pos.end - run_x_offset);
                paint_records.emplace_back(
                        run.style(), run_x_offset + justify_x_offset, 0,
                        std::move(blob_buffer),
                        metrics, line_number, record_x_pos.start,
                        record_x_pos.end, run.is_ghost());
                justify_x_offset += justify_x_offset_delta;

                line_glyph_positions.insert(line_glyph_positions.end(),
                                            glyph_positions.begin(),
                                            glyph_positions.end());

                // Add a record of glyph positions sorted by code unit index.
                std::vector<GlyphPosition> code_unit_positions(glyph_positions);
                std::sort(code_unit_positions.begin(), code_unit_positions.end(),
                          [](const GlyphPosition& a, const GlyphPosition& b) {
                              return a.code_units.start < b.code_units.start;
                          });
                line_code_unit_runs.emplace_back(
                        std::move(code_unit_positions),
                        Range<size_t>(run.start(), run.end()),
                        Range<double>(glyph_positions.front().x_pos.start,
                                      glyph_positions.back().x_pos.end),
                        line_number, metrics, run.direction());

                min_left_ = std::min(min_left_, glyph_positions.front().x_pos.start);
                max_right_ = std::max(max_right_, glyph_positions.back().x_pos.end);
            }  // for each in glyph_blobs

            // Do not increase x offset for trailing ghost runs as it should not
            // impact the layout of visible glyphs. We do keep the record though so
            // GetRectsForRange() can find metrics for trailing spaces.
            if (!run.is_ghost()) {
                run_x_offset += layout.getAdvance();
            }
        }  // for each in line_runs

        // Adjust the glyph positions based on the alignment of the line.
        double line_x_offset = GetLineXOffset(run_x_offset);
        if (line_x_offset) {
            for (CodeUnitRun& code_unit_run : line_code_unit_runs) {
                code_unit_run.Shift(line_x_offset);
            }
            for (GlyphPosition& position : line_glyph_positions) {
                position.Shift(line_x_offset);
            }
        }

        size_t next_line_start = (line_number < line_ranges_.size() - 1)
                                 ? line_ranges_[line_number + 1].start
                                 : text_.size();
        glyph_lines_.emplace_back(std::move(line_glyph_positions),
                                  next_line_start - line_range.start);
        code_unit_runs_.insert(code_unit_runs_.end(), line_code_unit_runs.begin(),
                               line_code_unit_runs.end());

        // Calculate the amount to advance in the y direction. This is done by
        // computing the maximum ascent and descent with respect to the strut.
        double max_ascent = strut.ascent + strut.half_leading;
        double max_descent = strut.descent + strut.half_leading;
        float max_unscaled_ascent = 0;
        auto update_line_metrics = [&](const FontMetrics& metrics,
                                       const TextStyle& style) {
            if (!strut.force_strut) {
                double ascent =
                        (-metrics.fAscent + metrics.fLeading / 2) * style.height;
                max_ascent = std::max(ascent, max_ascent);

                double descent =
                        (metrics.fDescent + metrics.fLeading / 2) * style.height;
                max_descent = std::max(descent, max_descent);
            }

            max_unscaled_ascent = std::max(-metrics.fAscent, max_unscaled_ascent);
        };
        for (const PaintRecord& paint_record : paint_records) {
            update_line_metrics(paint_record.metrics(), paint_record.style());
        }

        // If no fonts were actually rendered, then compute a baseline based on the
        // font of the paragraph style.
        if (paint_records.empty()) {
            FontMetrics metrics;
            TextStyle style(paragraph_style_.GetTextStyle());
            Typeface* typeface = GetDefaultTypeface(style);
            typeface->setSize(style.font_size);
            typeface->getMetrics(&metrics);
            update_line_metrics(metrics, style);
        }

        // Calculate the baselines. This is only done on the first line.
        if (line_number == 0) {
            alphabetic_baseline_ = max_ascent;
            // TODO(garyq): Ideographic baseline is currently bottom of EM
            // box, which is not correct. This should be obtained from metrics.
            // Skia currently does not support various baselines.
            ideographic_baseline_ = (max_ascent + max_descent);
        }

        line_heights_.push_back((line_heights_.empty() ? 0 : line_heights_.back()) +
                                round(max_ascent + max_descent));
        line_baselines_.push_back(line_heights_.back() - max_descent);
        y_offset += round(max_ascent + prev_max_descent);
        prev_max_descent = max_descent;

        // The max line spacing and ascent have been multiplied by -1 to make math
        // in GetRectsForRange more logical/readable.
        line_max_spacings_.push_back(max_ascent);
        line_max_descent_.push_back(max_descent);
        line_max_ascent_.push_back(max_unscaled_ascent);

        for (PaintRecord& paint_record : paint_records) {
            paint_record.SetOffset(
                    paint_record.offset_x() + line_x_offset, y_offset);
            records_.emplace_back(std::move(paint_record));
        }
    }  // for each line_number

    if (paragraph_style_.max_lines == 1 ||
        (paragraph_style_.unlimited_lines() && paragraph_style_.ellipsized())) {
        min_intrinsic_width_ = max_intrinsic_width_;
    } else {
        min_intrinsic_width_ = std::min(max_word_width, max_intrinsic_width_);
    }

    std::sort(code_unit_runs_.begin(), code_unit_runs_.end(),
              [](const CodeUnitRun& a, const CodeUnitRun& b) {
                  return a.code_units.start < b.code_units.start;
              });
}

double Paragraph::GetLineXOffset(double line_total_advance) {
    if (isinf(width_))
        return 0;

    TextAlign align = paragraph_style_.effective_align();

    if (align == TextAlign::right) {
        return width_ - line_total_advance;
    } else if (align == TextAlign::center) {
        return (width_ - line_total_advance) / 2;
    } else {
        return 0;
    }
}

const ParagraphStyle& Paragraph::GetParagraphStyle() const {
    return paragraph_style_;
}

double Paragraph::GetAlphabeticBaseline() const {
    // Currently -fAscent
    return alphabetic_baseline_;
}

double Paragraph::GetIdeographicBaseline() const {
    // TODO(garyq): Currently -fAscent + fUnderlinePosition. Verify this.
    return ideographic_baseline_;
}

double Paragraph::GetMaxIntrinsicWidth() const {
    return max_intrinsic_width_;
}

double Paragraph::GetMinIntrinsicWidth() const {
    return min_intrinsic_width_;
}

size_t Paragraph::TextSize() const {
    return text_.size();
}

double Paragraph::GetHeight() const {
    return line_heights_.size() ? line_heights_.back() : 0;
}

double Paragraph::GetMaxWidth() const {
    return width_;
}

void Paragraph::SetParagraphStyle(const ParagraphStyle& style) {
    needs_layout_ = true;
    paragraph_style_ = style;
}

void Paragraph::SetFontCollection(
        std::shared_ptr<FontCollection> font_collection) {
    font_collection_ = std::move(font_collection);
}

std::shared_ptr<minikin::FontCollection>
Paragraph::GetMinikinFontCollectionForStyle(const TextStyle& style) {
    std::string locale;
    if (!style.locale.empty()) {
        uint32_t language_list_id =
                minikin::FontStyle::registerLanguageList(style.locale);
        const minikin::FontLanguages& langs =
                minikin::FontLanguageListCache::getById(language_list_id);
        if (langs.size()) {
            locale = langs[0].getString();
        }
    }

    return font_collection_->GetMinikinFontCollectionForFamilies(
            style.font_families, locale);
}

Typeface* Paragraph::GetDefaultTypeface(const TextStyle& style) {
    std::shared_ptr<minikin::FontCollection> collection =
            GetMinikinFontCollectionForStyle(style);
    if (!collection) {
        return nullptr;
    }
    minikin::FakedFont faked_font =
            collection->baseFontFaked(GetMinikinFontStyle(style));
    return static_cast<LayoutFont*>(faked_font.font)->typeface();
}

Paragraph::PositionWithAffinity Paragraph::GetGlyphPositionAtCoordinate(
        double dx,
        double dy) const {
    if (line_heights_.empty())
        return PositionWithAffinity(0, DOWNSTREAM);

    size_t y_index;
    for (y_index = 0; y_index < line_heights_.size() - 1; ++y_index) {
        if (dy < line_heights_[y_index])
            break;
    }

    const std::vector<GlyphPosition>& line_glyph_position =
            glyph_lines_[y_index].positions;
    if (line_glyph_position.empty()) {
        int line_start_index =
                std::accumulate(glyph_lines_.begin(), glyph_lines_.begin() + y_index, 0,
                                [](const int a, const GlyphLine& b) {
                                    return a + static_cast<int>(b.total_code_units);
                                });
        return PositionWithAffinity(line_start_index, DOWNSTREAM);
    }

    size_t x_index;
    const GlyphPosition* gp = nullptr;
    for (x_index = 0; x_index < line_glyph_position.size(); ++x_index) {
        double glyph_end = (x_index < line_glyph_position.size() - 1)
                           ? line_glyph_position[x_index + 1].x_pos.start
                           : line_glyph_position[x_index].x_pos.end;
        if (dx < glyph_end) {
            gp = &line_glyph_position[x_index];
            break;
        }
    }

    if (gp == nullptr) {
        const GlyphPosition& last_glyph = line_glyph_position.back();
        return PositionWithAffinity(last_glyph.code_units.end, UPSTREAM);
    }

    // Find the direction of the run that contains this glyph.
    TextDirection direction = TextDirection::ltr;
    for (const CodeUnitRun& run : code_unit_runs_) {
        if (gp->code_units.start >= run.code_units.start &&
            gp->code_units.end <= run.code_units.end) {
            direction = run.direction;
            break;
        }
    }

    double glyph_center = (gp->x_pos.start + gp->x_pos.end) / 2;
    if ((direction == TextDirection::ltr && dx < glyph_center) ||
        (direction == TextDirection::rtl && dx >= glyph_center)) {
        return PositionWithAffinity(gp->code_units.start, DOWNSTREAM);
    } else {
        return PositionWithAffinity(gp->code_units.end, UPSTREAM);
    }
}

Paragraph::Range<size_t> Paragraph::GetWordBoundary(size_t offset) const {
    if (text_.size() == 0)
        return Range<size_t>(0, 0);

    if (!word_breaker_) {
        UErrorCode status = U_ZERO_ERROR;
        word_breaker_.reset(
                icu::BreakIterator::createWordInstance(icu::Locale(), status));
        if (!U_SUCCESS(status))
            return Range<size_t>(0, 0);
    }

    word_breaker_->setText(icu::UnicodeString(false, text_.data(), text_.size()));

    int32_t prev_boundary = word_breaker_->preceding(offset + 1);
    int32_t next_boundary = word_breaker_->next();
    if (prev_boundary == icu::BreakIterator::DONE)
        prev_boundary = offset;
    if (next_boundary == icu::BreakIterator::DONE)
        next_boundary = offset;
    return Range<size_t>(prev_boundary, next_boundary);
}

size_t Paragraph::GetLineCount() const {
    return line_heights_.size();
}

bool Paragraph::DidExceedMaxLines() const {
    return did_exceed_max_lines_;
}

void Paragraph::SetDirty(bool dirty) {
    needs_layout_ = dirty;
}

void Paragraph::Paint(TextRenderer* renderer, double x, double y) {
    for (const PaintRecord& record : records_) {
        double offset_x = x + record.offset_x();
        double offset_y = y + record.offset_y();
        renderer->drawTextBlob(record.buffer(), offset_x, offset_y, record.style());
    }
}

}  // namespace txt
