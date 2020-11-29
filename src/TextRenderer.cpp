//
// Created by bq on 2019-08-20.
//

#include <sstream>
#include <iostream>

#include "TextRenderer.h"
#include "unicode/unistr.h"
#include "FontManager.h"
#include "GlyphInfo.h"
#include "LayoutFont.h"
#include "unicode/utf16.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

#define TEXTURE_BORDER_SIZE 1


CacheTexture* TextRenderer::createCacheTexture(int width, int height, GLenum format,
                                               bool allocate) {
    CacheTexture* mCurrentCacheTexture = new CacheTexture(width, height, format, kMaxNumberOfQuads);
    if (allocate) {
        mTextureState->activateTexture(0);
        mCurrentCacheTexture->allocatePixelBuffer();
        mCurrentCacheTexture->allocateMesh();
    }

    return mCurrentCacheTexture;
}

void TextRenderer::initTextTexture() {
    mUploadTexture = false;
    mACacheTextures.push_back(createCacheTexture(1024, 512, GL_RED, true));
    mCurrentCacheTexture = mACacheTextures[0];
}

TextRenderer::TextRenderer(GLRenderer* renderer) :
        mGlyphCache(LruCache<GlyphKey, GlyphInfo*>::kUnlimitedCapacity),
        mGLRenderer(renderer) {
    mTextureState = new TextureState();
    initTextTexture();
}

void clearCacheTextures(std::vector<CacheTexture*>& cacheTextures) {
    for (uint32_t i = 0; i < cacheTextures.size(); i++) {
        delete cacheTextures[i];
    }
    cacheTextures.clear();
}

TextRenderer::~TextRenderer() {
    delete mTextureState;
    clearCacheTextures(mACacheTextures);

    LruCache<GlyphKey, GlyphInfo*>::Iterator it(mGlyphCache);
    while (it.next()) {
        delete it.value();
    }
    mGlyphCache.clear();
}

GlyphInfo* TextRenderer::getCachedGlyph(Typeface* face, uint32_t g) {
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

void TextRenderer::checkTextureUpdateForCache(std::vector<CacheTexture*>& cacheTextures,
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


void TextRenderer::finishRender() {
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

void TextRenderer::drawTextBlob(txt::RunBuffer* buffer, double x, double y, const txt::TextStyle& style) {

    for (size_t i = 0; i < buffer->glyphs.size(); i++) {
        auto g = buffer->glyphs.at(i);
        buffer->typeface->setSize(style.font_size);

        GlyphKey key = {buffer->typeface->id(), (uint) style.font_weight, (uint) style.font_style,
                        (uint) style.font_size, g};
        GlyphInfo* glyph = mGlyphCache.get(key);
        if (!glyph) {
            glyph = getCachedGlyph(buffer->typeface, g);
            mGlyphCache.put(key, glyph);
        }
        int penX = x + (int) roundf(buffer->pos[(i << 1)]);
        int penY = y + (int) roundf(buffer->pos[(i << 1) + 1]);

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
}