//
// Created by bq on 2019-08-16.
//

#include <sstream>
#include <iostream>

#include "FontRenderer.h"
#include "unicode/unistr.h"
#include "FontManager.h"
#include "GlyphInfo.h"
#include "LayoutFont.h"
#include "unicode/ubidi.h"
#include "unicode/utf16.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

#define TEXTURE_BORDER_SIZE 1

static void font_from_string(const std::string& fontString,
                             std::string& fontName,
                             float& textSize,
                             int& fontWeight,
                             bool& bold,
                             bool& italic) {
    if (fontString.empty()) {
        return;
    }
    // 'italic 400 12px Roboto, sans-serif'
    bool isPt = false;
    int fontSizeEnd = fontString.find(std::string("px"));
    if (fontSizeEnd == -1) {
        fontSizeEnd = fontString.find(std::string("pt"));
        isPt = true;
    }
    if (fontSizeEnd == -1) {
        return;
    }

    int fontSizeStart = fontString.rfind(' ', fontSizeEnd);
    if (fontSizeStart == -1)
        fontSizeStart = 0;
    else
        ++fontSizeStart;

    // + 2 for the unit, +1 for the space.
    fontSizeEnd += 3;

    textSize = std::stoi(fontString.substr(fontSizeStart, fontSizeEnd - fontSizeStart - 2).c_str());
    if (isPt)
        textSize *= 1.33;

    // remove fontSize section.
    std::string remainingFontString = fontString;
    remainingFontString.erase(fontSizeStart, fontSizeEnd - fontSizeStart);

    // fetch fontName
    fontName = remainingFontString.substr(fontSizeStart, remainingFontString.length() - fontSizeStart);
    remainingFontString.erase(fontSizeStart, remainingFontString.length() - fontSizeStart);

    // trim
    std::stringstream trimmer;
    trimmer << remainingFontString;
    remainingFontString.clear();
    trimmer >> remainingFontString;

    std::istringstream steam(remainingFontString.c_str());
    std::string token;
    while (getline(steam, token, ' ')) {
        if (token == "bold") {
            bold = true;
        } else if (token == "italic" || token == "oblique") {
            italic = true;
        } else {
            std::string::const_iterator it = token.begin();
            while (it != token.end() && std::isdigit(*it))
                ++it;
            if (!token.empty() && it == token.end()) {
                int weight = std::stoi(token);
                if (weight >= 0 && weight <= 99)
                    fontWeight = weight;
            }
        }
    }
}

CacheTexture* FontRenderer::createCacheTexture(int width, int height, GLenum format,
                                               bool allocate) {
    CacheTexture* mCurrentCacheTexture = new CacheTexture(width, height, format, kMaxNumberOfQuads);
    if (allocate) {
        mTextureState->activateTexture(0);
        mCurrentCacheTexture->allocatePixelBuffer();
        mCurrentCacheTexture->allocateMesh();
    }

    return mCurrentCacheTexture;
}

void FontRenderer::initTextTexture() {
    mUploadTexture = false;
    mACacheTextures.push_back(createCacheTexture(1024, 512, GL_RED, true));
    mCurrentCacheTexture = mACacheTextures[0];
}

FontRenderer::FontRenderer(GLRenderer* renderer) :
        mGlyphCache(LruCache<GlyphKey, GlyphInfo*>::kUnlimitedCapacity),
        mGLRenderer(renderer) {
    mBreaker.setLocale(icu::Locale(), nullptr);
    mTextureState = new TextureState();
    initTextTexture();
}

void clearCacheTextures(std::vector<CacheTexture*>& cacheTextures) {
    for (uint32_t i = 0; i < cacheTextures.size(); i++) {
        delete cacheTextures[i];
    }
    cacheTextures.clear();
}

FontRenderer::~FontRenderer() {
    delete mTextureState;
    clearCacheTextures(mACacheTextures);

    LruCache<GlyphKey, GlyphInfo*>::Iterator it(mGlyphCache);
    while (it.next()) {
        delete it.value();
    }
    mGlyphCache.clear();
}

void FontRenderer::checkTextureUpdateForCache(std::vector<CacheTexture*>& cacheTextures,
                                              bool& resetPixelStore, GLuint& lastTextureId) {
    for (uint32_t i = 0; i < cacheTextures.size(); i++) {
        CacheTexture* cacheTexture = cacheTextures[i];
        if (cacheTexture->isDirty() && cacheTexture->getPixelBuffer()) {
            char prefix[64];
            sprintf(prefix, "FontTexture_%d_%d.png", i, cacheTexture->getTextureId());
            switch (cacheTexture->getFormat()) {
                case GL_RED:
                default:
                    printf("write texture \n");
                    stbi_write_png(prefix, cacheTexture->getWidth(), cacheTexture->getHeight(), 1,
                                   cacheTexture->getPixelBuffer()->getMappedPointer(), cacheTexture->getWidth());
                    break;
            }

            if (cacheTexture->getTextureId() != lastTextureId) {
                lastTextureId = cacheTexture->getTextureId();
                mTextureState->activateTexture(0);
                mTextureState->bindTexture(lastTextureId);
            }

            if (cacheTexture->upload()) {
                resetPixelStore = true;
            }
        }
    }
}

GlyphInfo* FontRenderer::getCachedGlyph(Typeface* face, uint32_t g) {
    GlyphInfo* glyph = new GlyphInfo;
    face->generateImage(g, *glyph);

    std::string file = "t_";
    file += g;
    file += ".png";
    // stbi_write_png(file.c_str(), glyph->fPitch, glyph->fHeight, 1, glyph->fImage, glyph->fPitch);

    uint32_t startX = 0;
    uint32_t startY = 0;
    if (!mCurrentCacheTexture->fitBitmap(*glyph, &startX, &startY)) {
        printf("fit return \n");
        return glyph;
    }
    glyph->fCacheTexture = mCurrentCacheTexture;

    uint32_t endX = startX + glyph->fWidth;
    uint32_t endY = startY + glyph->fHeight;

    uint32_t cacheWidth = mCurrentCacheTexture->getWidth();

    if (!mCurrentCacheTexture->getPixelBuffer()) {
        mTextureState->activateTexture(0);
        // Large-glyph texture memory is allocated only as needed
        mCurrentCacheTexture->allocatePixelBuffer();
    }
    if (!mCurrentCacheTexture->mesh()) {
        mCurrentCacheTexture->allocateMesh();
    }

    uint8_t* cacheBuffer = mCurrentCacheTexture->getPixelBuffer()->map();
    uint8_t* bitmapBuffer = (uint8_t*) glyph->fImage;
    int srcStride = glyph->fPitch;

    // Copy the glyph image, taking the mask format into account
    switch (glyph->fFormat) {
        case GlyphInfo::Format_A8 : {
            uint32_t cacheX = 0, bX = 0, cacheY = 0, bY = 0;
            uint32_t row = (startY - TEXTURE_BORDER_SIZE) * cacheWidth + startX
                           - TEXTURE_BORDER_SIZE;
            // write leading border line
            memset(&cacheBuffer[row], 0, glyph->fWidth + 2 * TEXTURE_BORDER_SIZE);
            // write glyph data
            for (cacheY = startY, bY = 0; cacheY < endY; cacheY++, bY += srcStride) {
                row = cacheY * cacheWidth;
                memcpy(&cacheBuffer[row + startX], &bitmapBuffer[bY], glyph->fWidth);
                cacheBuffer[row + startX - TEXTURE_BORDER_SIZE] = 0;
                cacheBuffer[row + endX + TEXTURE_BORDER_SIZE - 1] = 0;
            }
            // write trailing border line
            row = (endY + TEXTURE_BORDER_SIZE - 1) * cacheWidth + startX - TEXTURE_BORDER_SIZE;
            memset(&cacheBuffer[row], 0, glyph->fWidth + 2 * TEXTURE_BORDER_SIZE);
            break;
        }
        default:
            break;

    }

    uint32_t textureWidth = glyph->fCacheTexture->getWidth();
    uint32_t textureHeight = glyph->fCacheTexture->getHeight();

    glyph->fBitmapMinU = startX / (float) textureWidth;
    glyph->fBitmapMinV = startY / (float) textureHeight;
    glyph->fBitmapMaxU = endX / (float) textureWidth;
    glyph->fBitmapMaxV = endY / (float) textureHeight;

    mUploadTexture = true;

    return glyph;

}

enum class TextDirection {
    rtl,
    ltr,
};


class BidiRun {
public:
    BidiRun(size_t s, size_t e, TextDirection d)
            : start_(s), end_(e), direction_(d) {}

    size_t start() const { return start_; }

    size_t end() const { return end_; }

    TextDirection direction() const { return direction_; }

    bool is_rtl() const { return direction_ == TextDirection::rtl; }

private:
    size_t start_, end_;
    TextDirection direction_;
};

bool computeBidiRuns(const std::u16string& text_, std::vector<BidiRun>* result) {
    if (text_.empty())
        return true;

    auto ubidi_closer = [](UBiDi* b) { ubidi_close(b); };
    std::unique_ptr<UBiDi, decltype(ubidi_closer)> bidi(ubidi_open(),
                                                        ubidi_closer);
    if (!bidi)
        return false;

    UBiDiLevel paraLevel = UBIDI_DEFAULT_LTR;
    UErrorCode status = U_ZERO_ERROR;
    ubidi_setPara(bidi.get(), reinterpret_cast<const UChar*>(text_.data()),
                  text_.size(), paraLevel, nullptr, &status);
    if (!U_SUCCESS(status))
        return false;

    int32_t bidi_run_count = ubidi_countRuns(bidi.get(), &status);
    if (!U_SUCCESS(status))
        return false;

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

        result->emplace_back(BidiRun(bidi_run_start, bidi_run_end, text_direction));
    }
    return true;
}


void FontRenderer::layout(minikin::Layout& layout, const std::u16string& text, Typeface* face,
                          FontStyle fs, int textSize, int maxWidth, std::vector<float>& pos) {

    minikin::FontStyle font(minikin::FontLanguageListCache::kEmptyListId, 0, fs.weight() / 100,
                            fs.slant() == FontStyle::kItalic_Slant);
    minikin::MinikinPaint paint;
    std::vector<std::shared_ptr<minikin::FontFamily>> minikin_families;
    std::vector<minikin::Font> minikin_fonts;
    minikin::Font minikin_font(
            std::make_shared<LayoutFont>(face), font);
    minikin_fonts.emplace_back(std::move(minikin_font));

    std::shared_ptr<minikin::FontFamily> minikin_family = std::make_shared<minikin::FontFamily>(
            std::move(minikin_fonts));
    minikin_families.push_back(minikin_family);
    auto font_collection =
            std::make_shared<minikin::FontCollection>(minikin_families);

    paint.size = textSize;
    // Divide by font size so letter spacing is pixels, not proportional to font
    // size.
    paint.wordSpacing = 2.0f;
    paint.letterSpacing = 0.1f;
    paint.scaleX = 1.0f;
    paint.skewX = 2.f;
    // Prevent spacing rounding in Minikin. This causes jitter when switching
    // between same text content with different runs composing it, however, it
    // also produces more accurate layouts.
    paint.paintFlags |= minikin::LinearTextFlag;

    mLineRanges.clear();
    mLineWidths.clear();
    double max_intrinsic_width_ = 0;

    std::vector<size_t> newline_positions;
    // 新开一行的 LF 和 BK 分割字符判断
    for (size_t i = 0; i < text.size(); ++i) {
        ULineBreak ulb = static_cast<ULineBreak>(
                u_getIntPropertyValue(text[i], UCHAR_LINE_BREAK));
        if (ulb == U_LB_LINE_FEED || ulb == U_LB_MANDATORY_BREAK)
            newline_positions.push_back(i);
    }
    newline_positions.push_back(text.size());


    for (size_t newline_index = 0; newline_index < newline_positions.size();
         ++newline_index) {
        size_t block_start =
                (newline_index > 0) ? newline_positions[newline_index - 1] + 1 : 0;
        size_t block_end = newline_positions[newline_index];
        size_t block_size = block_end - block_start;

        // first line
        if (block_size == 0) {

            mLineRanges.emplace_back(block_start, block_end, block_end,
                                     block_end + 1, true);
            mLineWidths.push_back(0);
            continue;
        }

        mBreaker.setLineWidths(0.0f, 0, maxWidth);
        mBreaker.resize(block_size);
        memcpy(mBreaker.buffer(), text.data() + block_start,
               block_size * sizeof(text[0]));
        mBreaker.setText();

        // Add the runs that include this line to the LineBreaker.
        double block_total_width = 0;
        // 片段宽度
        size_t run_start = 0;
        size_t run_end = block_end - block_start;
        double run_width = mBreaker.addStyleRun(&paint, font_collection, font,
                                                run_start, run_end, false);
        block_total_width += run_width;
        max_intrinsic_width_ = std::max(max_intrinsic_width_, block_total_width);

        size_t breaks_count = mBreaker.computeBreaks();

        const int* breaks = mBreaker.getBreaks();
        for (size_t i = 0; i < breaks_count; ++i) {
            size_t break_start = (i > 0) ? breaks[i - 1] : 0;
            size_t line_start = break_start + block_start;
            size_t line_end = breaks[i] + block_start;
            bool hard_break = i == breaks_count - 1;
            size_t line_end_including_newline =
                    (hard_break && line_end < text.size()) ? line_end + 1 : line_end;
            size_t line_end_excluding_whitespace = line_end;
            while (
                    line_end_excluding_whitespace > line_start &&
                    minikin::isLineEndSpace(text[line_end_excluding_whitespace - 1])) {
                line_end_excluding_whitespace--;
            }
            mLineRanges.emplace_back(line_start, line_end,
                                     line_end_excluding_whitespace,
                                     line_end_including_newline, hard_break);
            mLineWidths.push_back(mBreaker.getWidths()[i]);
        }

        mBreaker.finish();
    }

    printf("line widths size: %lu \n", mLineWidths.size());

    std::vector<BidiRun> bidi_runs;
    if (!computeBidiRuns(text, &bidi_runs)) {
        return;
    }

    for (int i = 0; i < bidi_runs.size(); i++) {
        printf("bidi:[%d] s:[%zu], e:[%zu], d:[%d]\n", i, bidi_runs[i].start(), bidi_runs[i].end(),
               bidi_runs[i].direction());
    }

    double y = 0;
    for (size_t line_number = 0; line_number < mLineRanges.size(); ++line_number) {
        const LineRange& line_range = mLineRanges[line_number];
        size_t line_end_index = line_range.end;
        std::vector<BidiRun> line_runs;
        for (const BidiRun& bidi_run : bidi_runs) {
            if (bidi_run.start() < line_end_index &&
                bidi_run.end() > line_range.start) {
                line_runs.emplace_back(std::max(bidi_run.start(), line_range.start),
                                       std::min(bidi_run.end(), line_end_index),
                                       bidi_run.direction());
            }
            if (line_range.end_excluding_whitespace < line_range.end &&
                bidi_run.start() <= line_range.end &&
                bidi_run.end() > line_end_index) {
                line_runs.emplace_back(std::max(bidi_run.start(), line_end_index),
                                       std::min(bidi_run.end(), line_range.end),
                                       bidi_run.direction());
            }
        }

        for (int i = 0; i < line_runs.size(); i++) {
            printf("line_runs:[%d] s:[%zu], e:[%zu], d:[%d]\n", i, line_runs[i].start(), line_runs[i].end(),
                   line_runs[i].direction());
        }

        for (auto line_run_it = line_runs.begin(); line_run_it != line_runs.end();
             ++line_run_it) {
            const BidiRun& run = *line_run_it;
            size_t text_start = run.start();
            size_t text_end = run.end();
            size_t text_count = run.end() - run.start();
            size_t text_size = text.size();

            const uint16_t* text_ptr = reinterpret_cast<const uint16_t*>(text.data());
            layout.doLayout(text_ptr, text_start, text_count, text_size, run.is_rtl(), font,
                            paint, font_collection);

            int glyph_index = text_start;
            do {
                size_t blob_index = glyph_index - text_start;
                size_t pos_index = glyph_index * 2;
                pos[pos_index] = layout.getX(blob_index);
                pos[pos_index + 1] = y + layout.getY(blob_index);
                glyph_index++;
            } while (glyph_index < text_end);
        }
        printf("line height: %f \n", face->lineHeight());
        y += face->lineHeight(); // Line height
    }

}

bool FontRenderer::renderPosText(const char* text, const char* font, int x, int y, int width) {
    auto icu_text = icu::UnicodeString::fromUTF8(text);
    std::u16string u16string(icu_text.getBuffer(),
                             icu_text.getBuffer() + icu_text.length());

    std::string fontName;
    float textSize = 12;
    int fontWeight = FontStyle::kNormal_Weight;
    bool bold = false;
    bool italic = false;
    font_from_string(font, fontName, textSize, fontWeight, bold, italic);

    FontStyle fs(fontWeight, bold ? FontStyle::kExpanded_Width : FontStyle::kNormal_Width,
                 italic ? FontStyle::kItalic_Slant : FontStyle::kUpright_Slant);
    Typeface* face = FontManager::getInstance()->matchFamilyStyle(fontName.c_str(), fs);
    if (!face) {
        return false;
    }
    face->setSize(textSize);
    std::vector<float> positions;
    positions.resize(u16string.size() * 2);

    minikin::Layout layout;
    this->layout(layout, u16string, face, fs, textSize, width, positions);

    for (size_t i = 0; i < u16string.size(); i++) {
        auto g = u16string.at(i);

        GlyphKey key = {face->id(), fs.value(), (uint) textSize, g};
        GlyphInfo* glyph = mGlyphCache.get(key);
        if (!glyph) {
            glyph = getCachedGlyph(face, g);
            mGlyphCache.put(key, glyph);
        }
        int penX = x + (int) roundf(positions[(i << 1)]);
        int penY = y + (int) roundf(positions[(i << 1) + 1]);

        float width = (float) glyph->fWidth;
        float height = (float) glyph->fHeight;

        float nPenX = penX + glyph->fLeft;
        float nPenY = penY + glyph->fTop + height;

        float u1 = glyph->fBitmapMinU;
        float u2 = glyph->fBitmapMaxU;
        float v1 = glyph->fBitmapMinV;
        float v2 = glyph->fBitmapMaxV;

        mCurrentCacheTexture->addQuad(nPenX, nPenY, u1, v2,
                                      nPenX + width, nPenY, u2, v2,
                                      nPenX + width, nPenY - height, u2, v1,
                                      nPenX, nPenY - height, u1, v1);
    }

    finishRender();
    return true;
}

void FontRenderer::finishRender() {
    if (mCurrentCacheTexture->canDraw()) {
        GLuint lastTextureId = 0;
        bool resetPixelStore = false;
        // Iterate over all the cache textures and see which ones need to be updated
        checkTextureUpdateForCache(mACacheTextures, resetPixelStore, lastTextureId);
        mGLRenderer->render(*mCurrentCacheTexture);

        mCurrentCacheTexture->resetMesh();
        if (resetPixelStore) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        mUploadTexture = false;
    }
}
