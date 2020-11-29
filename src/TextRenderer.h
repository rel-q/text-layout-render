//
// Created by bq on 2019-08-20.
//

#ifndef FONT_DEMO_TEXTRENDER_H
#define FONT_DEMO_TEXTRENDER_H

#include <string>
#include <vector>
#include "LruCache.h"
#include "Typeface.h"
#include "CacheTexture.h"
#include "TextureState.h"
#include "GLRenderer.h"
#include "paint_record.h"

class TextRenderer {
public:

    TextRenderer(GLRenderer* renderer);

    ~TextRenderer();

    void drawTextBlob(txt::RunBuffer* buffer, double x, double y, const txt::TextStyle& style);

private:

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

    TextureState* mTextureState = nullptr;

    GLRenderer* mGLRenderer = nullptr;
};

#endif //FONT_DEMO_TEXTRENDER_H
