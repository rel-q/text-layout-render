//
// Created by bq on 2019-08-16.
//

#include <freetype/ftoutln.h>
#include "Typeface.h"
#include "GlyphInfo.h"
#include "stb_image_write.h"


#define TRUNC(x)    ((x) >> 6)
#define ROUND(x)    (((x)+32) & -64)

Typeface::Typeface(FontStyle style, FcPattern* pattern) :
        mFontStyle(style), mPattern(pattern), mID(getGenerationID()) {
    mPath = get_string(pattern, FC_FILE, nullptr);
    mFamilyName = get_string(pattern, FC_FAMILY, nullptr);
}

Typeface::~Typeface() {
    FcPatternDestroy(mPattern);
    FT_Done_Face(mFace);
}

unsigned int Typeface::getGenerationID() {

    static std::atomic<uint32_t> nextID{2};
    uint32_t id;
    do {
        id = nextID.fetch_add(2);
    } while (id == 0);
    return id;
}

void Typeface::setSize(uint size) {
    if (mSize != size) {
        // printf("FT_Set_Char_Size : %d\n", size);
        FT_Set_Pixel_Sizes(mFace, size, 0);
        mSize = size;
        mMetrics = mFace->size->metrics;
    }
}

double Typeface::ascent() {
    return (double) mMetrics.ascender / 64;
}

double Typeface::descent() {
    return -(double) mMetrics.descender / 64;
}

double Typeface::leading() {
    return (double) (mMetrics.height - mMetrics.ascender + mMetrics.descender) / 64;
}

double Typeface::lineHeight() {
    return (double) (mMetrics.height) / 64;
}

void Typeface::init() {
    FT_Select_Charmap(mFace, FT_ENCODING_UNICODE);
    mMatrix.xx = 0x10000;
    mMatrix.yy = 0x10000;
    mMatrix.xy = 0;
    mMatrix.yx = 0;
    mTransform = false;
    if (FT_IS_SCALABLE(mFace)) {
        bool fakeItalic = get_int(mPattern, FC_SLANT, FC_SLANT_ROMAN) != FC_SLANT_ROMAN &&
                          !(mFace->style_flags & FT_STYLE_FLAG_ITALIC);
        if (fakeItalic) {
            mMatrix.xy = 0x10000 * 3 / 10;
            mTransform = true;
        }
    }

    mMetrics = mFace->size->metrics;
    mLoadGlyphFlags = FT_LOAD_NO_BITMAP;
    mGlyphFormat = GlyphInfo::Format_A8;
}

static void calculateTransform(FT_Matrix& matrix, int& left, int& right, int& top, int& bottom) {
    int l, r, t, b;
    FT_Vector vector;
    vector.x = left;
    vector.y = top;
    FT_Vector_Transform(&vector, &matrix);
    l = r = vector.x;
    t = b = vector.y;
    vector.x = right;
    vector.y = top;
    FT_Vector_Transform(&vector, &matrix);
    if (l > vector.x) l = vector.x;
    if (r < vector.x) r = vector.x;
    if (t < vector.y) t = vector.y;
    if (b > vector.y) b = vector.y;
    vector.x = right;
    vector.y = bottom;
    FT_Vector_Transform(&vector, &matrix);
    if (l > vector.x) l = vector.x;
    if (r < vector.x) r = vector.x;
    if (t < vector.y) t = vector.y;
    if (b > vector.y) b = vector.y;
    vector.x = left;
    vector.y = bottom;
    FT_Vector_Transform(&vector, &matrix);
    if (l > vector.x) l = vector.x;
    if (r < vector.x) r = vector.x;
    if (t < vector.y) t = vector.y;
    if (b > vector.y) b = vector.y;
    left = l;
    right = r;
    top = t;
    bottom = b;
}

void Typeface::generateImage(const uint32_t glyph, GlyphInfo& glyphInfo) {
    // uint32_t index = FT_Get_Char_Index(mFace, glyph);
    // if (!index) {
    //     printf("FT_Get_Char_Index error: %d\n", glyph);
    //     return;
    // }
    FT_Error err = FT_Load_Glyph(mFace, glyph, mLoadGlyphFlags);
    if (err) {
        printf("FT_Load_Glyph error: %d\n", err);
        return;
    }
    switch (mFace->glyph->format) {
        case FT_GLYPH_FORMAT_OUTLINE: {
            FT_GlyphSlot slot = mFace->glyph;
            FT_BBox bbox;
            FT_Outline_Get_CBox(&slot->outline, &bbox);
            bbox.xMin &= ~63;
            bbox.yMin &= ~63;
            bbox.xMax = (bbox.xMax + 63) & ~63;
            bbox.yMax = (bbox.yMax + 63) & ~63;
            glyphInfo.fWidth = (uint32_t) TRUNC(bbox.xMax - bbox.xMin);
            glyphInfo.fHeight = (uint32_t) TRUNC(bbox.yMax - bbox.yMin);
            glyphInfo.fTop = (int32_t) -TRUNC(bbox.yMax);
            glyphInfo.fLeft = (int32_t) TRUNC(bbox.xMin);
            int pitch = (glyphInfo.fWidth + 3) & ~3;
            int glyph_buffer_size = pitch * glyphInfo.fHeight;
            glyphInfo.fFontID = mID;
            glyphInfo.fImage = new unsigned char[glyph_buffer_size];
            glyphInfo.fPitch = pitch;
            glyphInfo.fAdvanceX = (uint32_t) TRUNC(ROUND(slot->advance.x));

            FT_Bitmap bitmap;
            bitmap.rows = glyphInfo.fHeight;
            bitmap.width = glyphInfo.fWidth;
            bitmap.pitch = pitch;
            bitmap.buffer = reinterpret_cast<uint8_t*>(glyphInfo.fImage);
            memset(bitmap.buffer, 0, bitmap.rows * bitmap.pitch);
            bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;

            FT_Outline_Translate(&slot->outline, -(bbox.xMin & ~63), -(bbox.yMin & ~63));
            FT_Outline_Get_Bitmap(slot->library, &slot->outline, &bitmap);

            // stbi_write_png(std::to_string(glyph).c_str(), glyphInfo.fWidth, glyphInfo.fHeight, 1, bitmap.buffer, glyphInfo.fPitch);

            // printf("FT_Load_Glyph bitmap: l=%d, t=%d \n", glyphInfo.fLeft, glyphInfo.fTop);
        }
        default:
            break;
    }
}

void Typeface::getMetrics(FontMetrics* metrics) {
    metrics->fAscent = -ascent();
    metrics->fDescent = -descent();
    metrics->fLeading = leading();
}
