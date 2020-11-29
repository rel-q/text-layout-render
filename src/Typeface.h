//
// Created by bq on 2019-08-16.
//

#ifndef FONT_DEMO_TYPEFACE_H
#define FONT_DEMO_TYPEFACE_H

#include <string>
#include <ft2build.h>
#include <freetype/freetype.h>
#include "Util.h"
#include "GlyphInfo.h"
#include "FontStyle.h"
#include "FontMetrics.h"

class FontManager;

class Typeface {
public:

    Typeface(FontStyle style, FcPattern* pattern);

    ~Typeface();

    void generateImage(const uint32_t glyph, GlyphInfo& glyphInfo);

    uint32_t id() const { return mID; };

    void setSize(uint size);

    void init();

    double ascent();

    double descent();

    double leading();

    double lineHeight();

    FontStyle fontStyle() const {
        return mFontStyle;
    }

    std::string familyName() const {
        return mFamilyName;
    }

    std::string fontPath() const {
        return mPath;
    }

    void getMetrics(FontMetrics* metrics);

    FT_Face mFace;

private:

    unsigned int getGenerationID();

    uint32_t mID;
    int mSize;
    FT_Matrix mMatrix;
    FcPattern* mPattern;
    std::string mPath;
    bool mTransform;
    GlyphInfo::GlyphFormat mGlyphFormat;
    FT_Size_Metrics mMetrics;
    unsigned int mLoadGlyphFlags;
    FontStyle mFontStyle;

    friend class FontManager;

    std::string mFamilyName;
};


#endif //FONT_DEMO_TYPEFACE_H
