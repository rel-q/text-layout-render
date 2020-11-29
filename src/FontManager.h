//
// Created by bq on 2019-08-16.
//

#pragma once

#include <fontconfig/fontconfig.h>
#include <vector>
#include "FontStyle.h"
#include "FontManager.h"
#include "Util.h"
#include "Typeface.h"

class FontStyleSet {
public:
    explicit FontStyleSet(const FontManager* parent, AutoFcFontSet fontSet)
            : fFontMgr(parent), fFontSet(std::move(fontSet)) {}

    ~FontStyleSet() {
        // Hold the lock while unrefing the font set.
        fFontSet.reset();
    }

    int count() { return fFontSet->nfont; }

    void getStyle(int index, FontStyle* style, std::string* styleName);

    Typeface* createTypeface(int index);

    Typeface* matchStyle(const FontStyle& style);

private:
    const FontManager* fFontMgr;
    AutoFcFontSet fFontSet;
};

class FontManager {
public:
    FontManager();

    ~FontManager();

    Typeface* matchFamilyStyle(const char familyName[],
                               const FontStyle& style) const;

    Typeface* matchFamilyStyleCharacter(const char familyName[], const FontStyle&,
                                        const char* bcp47[], int bcp47Count,
                                        int32_t character) const;

    FontStyleSet* matchFamily(const char familyName[]) const;

    static FontManager* getInstance();

private:

    Typeface* createFontFaceFromFcPattern(FcPattern* pattern) const;

    FcConfig* mFcConfig;
    FT_Library mFTLibrary;

    mutable std::vector<Typeface*> mTypefaces;

    friend class FontStyleSet;
};
