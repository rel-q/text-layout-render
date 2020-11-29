//
// Created by bq on 2019-08-16.
//

#ifndef FONT_DEMO_FONTRENDER_H
#define FONT_DEMO_FONTRENDER_H

#include <string>
#include <vector>
#include "LruCache.h"
#include "Typeface.h"
#include "CacheTexture.h"
#include "TextureState.h"
#include "GLRenderer.h"
#include "FontStyle.h"

#include <minikin/Layout.h>
#include <minikin/FontLanguageListCache.h>
#include <minikin/LineBreaker.h>

class FontRenderer {
public:

    FontRenderer(GLRenderer* renderer);

    ~FontRenderer();

    void layout(minikin::Layout& layout, const std::u16string& text, Typeface* face,
                FontStyle fs, int textSize,int maxWidth, std::vector<float>& pos);

    bool renderPosText(const char* text,
                       const char* font, int x, int y, int width = std::numeric_limits<int>::max());

private:

    struct LineRange {
        LineRange(size_t s, size_t e, size_t eew, size_t ein, bool h)
                : start(s),
                  end(e),
                  end_excluding_whitespace(eew),
                  end_including_newline(ein),
                  hard_break(h) {}
        size_t start, end;
        size_t end_excluding_whitespace;
        size_t end_including_newline;
        bool hard_break;
    };

    void initTextTexture();

    CacheTexture* createCacheTexture(int width, int height, GLenum format,
                                     bool allocate);

    void checkTextureUpdateForCache(std::vector<CacheTexture*>& cacheTextures,
                                    bool& resetPixelStore, GLuint& lastTextureId);

    GlyphInfo* getCachedGlyph(Typeface* face, uint32_t g);

    void finishRender();

    std::vector<CacheTexture*> mACacheTextures;

    bool mUploadTexture;

    CacheTexture* mCurrentCacheTexture;

    LruCache<GlyphKey, GlyphInfo*> mGlyphCache;

    minikin::LineBreaker mBreaker;

    std::vector<double> mLineWidths;

    std::vector<LineRange> mLineRanges;

    TextureState* mTextureState = nullptr;

    GLRenderer* mGLRenderer = nullptr;
};


#endif //FONT_DEMO_FONTRENDER_H
