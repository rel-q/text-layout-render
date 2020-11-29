//
// Created by bq on 2019-08-16.
//

#ifndef FONT_DEMO_GLYPH_H
#define FONT_DEMO_GLYPH_H

#include <cstddef>
#include <cstdint>
#include "JenkinsHash.h"
#include "Util.h"

class CacheTexture;

struct GlyphKey {
    uint mFontID;
    uint mFontWeight;
    uint mFontStyle;
    uint mFontSize;
    uint16_t mGlyph;

    bool operator==(const GlyphKey& rhs) const {
        return mFontID == rhs.mFontID &&
               mFontWeight == rhs.mFontWeight &&
               mFontStyle == rhs.mFontStyle &&
               mFontSize == rhs.mFontSize &&
               mGlyph == rhs.mGlyph;
    }

    bool operator!=(const GlyphKey& rhs) const {
        return !(rhs == *this);
    }

    hash_t hash() const {
        uint32_t hash = JenkinsHashMix(0, mFontID);
        hash = JenkinsHashMix(hash, hash_type(mFontSize));
        hash = JenkinsHashMix(hash, hash_type(mFontWeight));
        hash = JenkinsHashMix(hash, hash_type(mFontStyle));
        hash = JenkinsHashMix(hash, hash_type(mGlyph));
        return JenkinsHashWhiten(hash);
    }
};

inline hash_t hash_type(const GlyphKey& entry) {
    return entry.hash();
}

class GlyphInfo {
public:
    enum GlyphFormat {
        Format_None,
        Format_A8,
        Format_ARGB
    };

    ~GlyphInfo() {
        delete[] (unsigned char*) fImage;
    }

    uint32_t fFontID;
    void* fImage = nullptr;
    uint32_t fAdvanceX;
    GlyphFormat fFormat = Format_A8;

    float fBitmapMinU;
    float fBitmapMinV;
    float fBitmapMaxU;
    float fBitmapMaxV;

    int fPitch;
    uint32_t fWidth, fHeight;
    int32_t fTop, fLeft;
    CacheTexture* fCacheTexture;
};

#endif //FONT_DEMO_GLYPH_H
