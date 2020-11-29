//
// Created by bq on 2019-08-16.
//

#include <ft2build.h>
#include <freetype/freetype.h>
#include <map>
#include <iostream>
#include <unistd.h>

#include "Typeface.h"
#include "LruCache.h"
#include "FontManager.h"
#include "Util.h"

static FontManager instance;

FontManager::FontManager() {
    FT_Init_FreeType(&mFTLibrary);
    mFcConfig = FcInitLoadConfigAndFonts();
}

FontManager* FontManager::getInstance() {
    return &instance;
}

FontManager::~FontManager() {
    for (auto t: mTypefaces) {
        delete t;
    }
    mTypefaces.clear();
    FT_Done_FreeType(mFTLibrary);
}

template<typename T>
const bool findFamily(std::vector<T>& vec, const T& ele) {
    if (std::find(vec.begin(), vec.end(), ele) != vec.end())
        return true;
    return false;
}

static int map_range(float value,
                     float old_min, float old_max,
                     float new_min, float new_max) {
    assert(old_min < old_max);
    assert(new_min <= new_max);
    return new_min + ((value - old_min) * (new_max - new_min) / (old_max - old_min));
}

struct MapRanges {
    float old_val;
    float new_val;
};

static float map_ranges(float val, MapRanges const ranges[], int rangesCount) {
    // -Inf to [0]
    if (val < ranges[0].old_val) {
        return ranges[0].new_val;
    }

    // Linear from [i] to [i+1]
    for (int i = 0; i < rangesCount - 1; ++i) {
        if (val < ranges[i + 1].old_val) {
            return map_range(val, ranges[i].old_val, ranges[i + 1].old_val,
                             ranges[i].new_val, ranges[i + 1].new_val);
        }
    }

    // From [n] to +Inf
    // if (fcweight < Inf)
    return ranges[rangesCount - 1].new_val;
}

#define MaxS32FitsInFloat    2147483520
#define MinS32FitsInFloat    -MaxS32FitsInFloat

static inline int float_saturate2int(float x) {
    x = TMin<float>(x, MaxS32FitsInFloat);
    x = TMax<float>(x, MinS32FitsInFloat);
    return (int) x;
}

#define ScalarRoundToInt(x)  float_saturate2int(floorf((x) + 0.5f))

static FontStyle fontstyle_from_fcpattern(FcPattern* pattern) {

    // FcWeightToOpenType was buggy until 2.12.4
    static constexpr MapRanges weightRanges[] = {
            {FC_WEIGHT_THIN,       FontStyle::kThin_Weight},
            {FC_WEIGHT_EXTRALIGHT, FontStyle::kExtraLight_Weight},
            {FC_WEIGHT_LIGHT,      FontStyle::kLight_Weight},
            {FC_WEIGHT_DEMILIGHT,  350},
            {FC_WEIGHT_BOOK,       380},
            {FC_WEIGHT_REGULAR,    FontStyle::kNormal_Weight},
            {FC_WEIGHT_MEDIUM,     FontStyle::kMedium_Weight},
            {FC_WEIGHT_DEMIBOLD,   FontStyle::kSemiBold_Weight},
            {FC_WEIGHT_BOLD,       FontStyle::kBold_Weight},
            {FC_WEIGHT_EXTRABOLD,  FontStyle::kExtraBold_Weight},
            {FC_WEIGHT_BLACK,      FontStyle::kBlack_Weight},
            {FC_WEIGHT_EXTRABLACK, FontStyle::kExtraBlack_Weight},
    };
    float weight = map_ranges(get_int(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR),
                              weightRanges, ARRAY_COUNT(weightRanges));

    static constexpr MapRanges widthRanges[] = {
            {FC_WIDTH_ULTRACONDENSED, FontStyle::kUltraCondensed_Width},
            {FC_WIDTH_EXTRACONDENSED, FontStyle::kExtraCondensed_Width},
            {FC_WIDTH_CONDENSED,      FontStyle::kCondensed_Width},
            {FC_WIDTH_SEMICONDENSED,  FontStyle::kSemiCondensed_Width},
            {FC_WIDTH_NORMAL,         FontStyle::kNormal_Width},
            {FC_WIDTH_SEMIEXPANDED,   FontStyle::kSemiExpanded_Width},
            {FC_WIDTH_EXPANDED,       FontStyle::kExpanded_Width},
            {FC_WIDTH_EXTRAEXPANDED,  FontStyle::kExtraExpanded_Width},
            {FC_WIDTH_ULTRAEXPANDED,  FontStyle::kUltraExpanded_Width},
    };
    float width = map_ranges(get_int(pattern, FC_WIDTH, FC_WIDTH_NORMAL),
                             widthRanges, ARRAY_COUNT(widthRanges));

    FontStyle::Slant slant = FontStyle::kUpright_Slant;
    switch (get_int(pattern, FC_SLANT, FC_SLANT_ROMAN)) {
        case FC_SLANT_ROMAN:
            slant = FontStyle::kUpright_Slant;
            break;
        case FC_SLANT_ITALIC :
            slant = FontStyle::kItalic_Slant;
            break;
        case FC_SLANT_OBLIQUE:
            slant = FontStyle::kOblique_Slant;
            break;
        default:
            break;
    }

    return FontStyle(ScalarRoundToInt(weight), ScalarRoundToInt(width), slant);
}

static void fcpattern_from_fontstyle(FontStyle style, FcPattern* pattern) {

    // FcWeightFromOpenType was buggy until 2.12.4
    static constexpr MapRanges weightRanges[] = {
            {FontStyle::kThin_Weight,       FC_WEIGHT_THIN},
            {FontStyle::kExtraLight_Weight, FC_WEIGHT_EXTRALIGHT},
            {FontStyle::kLight_Weight,      FC_WEIGHT_LIGHT},
            {350,                           FC_WEIGHT_DEMILIGHT},
            {380,                           FC_WEIGHT_BOOK},
            {FontStyle::kNormal_Weight,     FC_WEIGHT_REGULAR},
            {FontStyle::kMedium_Weight,     FC_WEIGHT_MEDIUM},
            {FontStyle::kSemiBold_Weight,   FC_WEIGHT_DEMIBOLD},
            {FontStyle::kBold_Weight,       FC_WEIGHT_BOLD},
            {FontStyle::kExtraBold_Weight,  FC_WEIGHT_EXTRABOLD},
            {FontStyle::kBlack_Weight,      FC_WEIGHT_BLACK},
            {FontStyle::kExtraBlack_Weight, FC_WEIGHT_EXTRABLACK},
    };
    int weight = map_ranges(style.weight(), weightRanges, ARRAY_COUNT(weightRanges));

    static constexpr MapRanges widthRanges[] = {
            {FontStyle::kUltraCondensed_Width, FC_WIDTH_ULTRACONDENSED},
            {FontStyle::kExtraCondensed_Width, FC_WIDTH_EXTRACONDENSED},
            {FontStyle::kCondensed_Width,      FC_WIDTH_CONDENSED},
            {FontStyle::kSemiCondensed_Width,  FC_WIDTH_SEMICONDENSED},
            {FontStyle::kNormal_Width,         FC_WIDTH_NORMAL},
            {FontStyle::kSemiExpanded_Width,   FC_WIDTH_SEMIEXPANDED},
            {FontStyle::kExpanded_Width,       FC_WIDTH_EXPANDED},
            {FontStyle::kExtraExpanded_Width,  FC_WIDTH_EXTRAEXPANDED},
            {FontStyle::kUltraExpanded_Width,  FC_WIDTH_ULTRAEXPANDED},
    };
    int width = map_ranges(style.width(), widthRanges, ARRAY_COUNT(widthRanges));

    int slant = FC_SLANT_ROMAN;
    switch (style.slant()) {
        case FontStyle::kUpright_Slant:
            slant = FC_SLANT_ROMAN;
            break;
        case FontStyle::kItalic_Slant :
            slant = FC_SLANT_ITALIC;
            break;
        case FontStyle::kOblique_Slant:
            slant = FC_SLANT_OBLIQUE;
            break;
        default:
            assert(false);
            break;
    }

    FcPatternAddInteger(pattern, FC_WEIGHT, weight);
    FcPatternAddInteger(pattern, FC_WIDTH, width);
    FcPatternAddInteger(pattern, FC_SLANT, slant);
}

static bool FontAccessible(FcPattern* font) {
    const char* filename = get_string(font, FC_FILE, nullptr);
    if (nullptr == filename) {
        return false;
    }
    return (0 == access(filename, R_OK));
}

enum WeakReturn {
    kIsWeak_WeakReturn,
    kIsStrong_WeakReturn,
    kNoId_WeakReturn
};

static WeakReturn is_weak(FcPattern* pattern, const char object[], int id) {
    FcResult result;

    // Create a copy of the pattern with only the value 'pattern'['object'['id']] in it.
    // Internally, FontConfig pattern objects are linked lists, so faster to remove from head.
    AutoFcObjectSet requestedObjectOnly(FcObjectSetBuild(object, nullptr));
    AutoFcPattern minimal(FcPatternFilter(pattern, requestedObjectOnly));
    FcBool hasId = true;
    for (int i = 0; hasId && i < id; ++i) {
        hasId = FcPatternRemove(minimal, object, 0);
    }
    if (!hasId) {
        return kNoId_WeakReturn;
    }
    FcValue value;
    result = FcPatternGet(minimal, object, 0, &value);
    if (result != FcResultMatch) {
        return kNoId_WeakReturn;
    }
    while (hasId) {
        hasId = FcPatternRemove(minimal, object, 1);
    }

    // Create a font set with two patterns.
    // 1. the same 'object' as minimal and a lang object with only 'nomatchlang'.
    // 2. a different 'object' from minimal and a lang object with only 'matchlang'.
    AutoFcFontSet fontSet;

    AutoFcLangSet strongLangSet;
    FcLangSetAdd(strongLangSet, (const FcChar8*) "nomatchlang");
    AutoFcPattern strong(FcPatternDuplicate(minimal));
    FcPatternAddLangSet(strong, FC_LANG, strongLangSet);

    AutoFcLangSet weakLangSet;
    FcLangSetAdd(weakLangSet, (const FcChar8*) "matchlang");
    AutoFcPattern weak;
    FcPatternAddString(weak, object, (const FcChar8*) "nomatchstring");
    FcPatternAddLangSet(weak, FC_LANG, weakLangSet);

    FcFontSetAdd(fontSet, strong.release());
    FcFontSetAdd(fontSet, weak.release());

    // Add 'matchlang' to the copy of the pattern.
    FcPatternAddLangSet(minimal, FC_LANG, weakLangSet);

    // Run a match against the copy of the pattern.
    // If the 'id' was weak, then we should match the pattern with 'matchlang'.
    // If the 'id' was strong, then we should match the pattern with 'nomatchlang'.

    // Note that this mFcConfig is only used for FcFontRenderPrepare, which we don't even want.
    // However, there appears to be no way to match/sort without it.
    AutoFcConfig mFcConfig;
    FcFontSet* fontSets[1] = {fontSet};
    AutoFcPattern match(FcFontSetMatch(mFcConfig, fontSets, ARRAY_COUNT(fontSets),
                                       minimal, &result));

    FcLangSet* matchLangSet;
    FcPatternGetLangSet(match, FC_LANG, 0, &matchLangSet);
    return FcLangEqual == FcLangSetHasLang(matchLangSet, (const FcChar8*) "matchlang")
           ? kIsWeak_WeakReturn : kIsStrong_WeakReturn;
}

static void remove_weak(FcPattern* pattern, const char object[]) {

    AutoFcObjectSet requestedObjectOnly(FcObjectSetBuild(object, nullptr));
    AutoFcPattern minimal(FcPatternFilter(pattern, requestedObjectOnly));

    int lastStrongId = -1;
    int numIds;
    WeakReturn result;
    for (int id = 0;; ++id) {
        result = is_weak(minimal, object, 0);
        if (kNoId_WeakReturn == result) {
            numIds = id;
            break;
        }
        if (kIsStrong_WeakReturn == result) {
            lastStrongId = id;
        }
        assert(FcPatternRemove(minimal, object, 0));
    }

    // If they were all weak, then leave the pattern alone.
    if (lastStrongId < 0) {
        return;
    }

    // Remove everything after the last strong.
    for (int id = lastStrongId + 1; id < numIds; ++id) {
        assert(FcPatternRemove(pattern, object, lastStrongId + 1));
    }
}

static bool AnyMatching(FcPattern* font, FcPattern* pattern, const char* object) {
    FcChar8* fontString;
    FcChar8* patternString;
    FcResult result;
    // Set an arbitrary limit on the number of pattern object values to consider.
    // TODO: re-write this to avoid N*M
    static const int maxId = 16;
    for (int patternId = 0; patternId < maxId; ++patternId) {
        result = FcPatternGetString(pattern, object, patternId, &patternString);
        if (FcResultNoId == result) {
            break;
        }
        if (FcResultMatch != result) {
            continue;
        }
        for (int fontId = 0; fontId < maxId; ++fontId) {
            result = FcPatternGetString(font, object, fontId, &fontString);
            if (FcResultNoId == result) {
                break;
            }
            if (FcResultMatch != result) {
                continue;
            }
            if (0 == FcStrCmpIgnoreCase(patternString, fontString)) {
                return true;
            }
        }
    }
    return false;
}

Typeface* FontManager::createFontFaceFromFcPattern(FcPattern* pattern) const {
    FcPatternReference(pattern);
    auto it = std::find_if(
            mTypefaces.begin(), mTypefaces.end(),
            [&pattern](const Typeface* t) {
                return FcTrue == FcPatternEqual(t->mPattern, pattern);
            });
    if (it != mTypefaces.end()) {
        return *it;
    }
    FontStyle style = fontstyle_from_fcpattern(pattern);
    const char* filename = get_string(pattern, FC_FILE, nullptr);
    std::cout << "create face: " << filename << std::endl;
    const char* path = get_string(pattern, FC_FILE, nullptr);
    Typeface* tf = new Typeface(style, pattern);
    FT_Error err = FT_New_Face(mFTLibrary, path, 0, &tf->mFace);
    if (err) {
        printf("FT_New_Face error, filePath: %s, code: %d\n", path, err);
        delete tf;
        return nullptr;
    }
    tf->init();
    mTypefaces.push_back(tf);
    return tf;
}

Typeface* FontManager::matchFamilyStyle(const char familyName[],
                                        const FontStyle& style) const {

    AutoFcPattern pattern;

    FcPatternAddString(pattern, FC_FAMILY, (FcChar8*) familyName);
    fcpattern_from_fontstyle(style, pattern);
    FcConfigSubstitute(mFcConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    // We really want to match strong (prefered) and same (acceptable) only here.
    // If a family name was specified, assume that any weak matches after the last strong match
    // are weak (default) and ignore them.
    // The reason for is that after substitution the pattern for 'sans-serif' looks like
    // "wwwwwwwwwwwwwwswww" where there are many weak but preferred names, followed by defaults.
    // So it is possible to have weakly matching but preferred names.
    // In aliases, bindings are weak by default, so this is easy and common.
    // If no family name was specified, we'll probably only get weak matches, but that's ok.
    FcPattern* matchPattern;
    AutoFcPattern strongPattern(nullptr);
    if (familyName) {
        strongPattern.reset(FcPatternDuplicate(pattern));
        remove_weak(strongPattern, FC_FAMILY);
        matchPattern = strongPattern;
    } else {
        matchPattern = pattern;
    }

    FcResult result;
    AutoFcPattern font(FcFontMatch(mFcConfig, pattern, &result));

    if (nullptr == font || !FontAccessible(font) || !AnyMatching(font, matchPattern, FC_FAMILY)) {
        return nullptr;
    }

    return createFontFaceFromFcPattern(font);
}

FontStyleSet* FontManager::matchFamily(const char* familyName) const {
    if (!familyName) {
        return nullptr;
    }

    AutoFcPattern pattern;
    FcPatternAddString(pattern, FC_FAMILY, (FcChar8*) familyName);
    FcConfigSubstitute(mFcConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcPattern* matchPattern;
    AutoFcPattern strongPattern(nullptr);
    strongPattern.reset(FcPatternDuplicate(pattern));
    remove_weak(strongPattern, FC_FAMILY);
    matchPattern = strongPattern;

    AutoFcFontSet matches;
    // TODO: Some families have 'duplicates' due to symbolic links.
    // The patterns are exactly the same except for the FC_FILE.
    // It should be possible to collapse these patterns by normalizing.
    static const FcSetName fcNameSet[] = {FcSetSystem, FcSetApplication};
    for (int setIndex = 0; setIndex < (int) ARRAY_COUNT(fcNameSet); ++setIndex) {
        // Return value of FcConfigGetFonts must not be destroyed.
        FcFontSet* allFonts(FcConfigGetFonts(mFcConfig, fcNameSet[setIndex]));
        if (nullptr == allFonts) {
            continue;
        }

        for (int fontIndex = 0; fontIndex < allFonts->nfont; ++fontIndex) {
            FcPattern* font = allFonts->fonts[fontIndex];
            if (FontAccessible(font) && AnyMatching(font, matchPattern, FC_FAMILY)) {
                FcFontSetAdd(matches, FcFontRenderPrepare(mFcConfig, pattern, font));
            }
        }
    }

    return new FontStyleSet(this, std::move(matches));
}

static bool FontContainsCharacter(FcPattern* font, uint32_t character) {
    FcResult result;
    FcCharSet* matchCharSet;
    for (int charSetId = 0;; ++charSetId) {
        result = FcPatternGetCharSet(font, FC_CHARSET, charSetId, &matchCharSet);
        if (FcResultNoId == result) {
            break;
        }
        if (FcResultMatch != result) {
            continue;
        }
        if (FcCharSetHasChar(matchCharSet, character)) {
            return true;
        }
    }
    return false;
}

Typeface*
FontManager::matchFamilyStyleCharacter(const char* familyName, const FontStyle& style, const char** bcp47,
                                       int bcp47Count,
                                       int32_t character) const {
    AutoFcPattern pattern;
    if (familyName) {
        FcValue familyNameValue;
        familyNameValue.type = FcTypeString;
        familyNameValue.u.s = reinterpret_cast<const FcChar8*>(familyName);
        FcPatternAddWeak(pattern, FC_FAMILY, familyNameValue, FcFalse);
    }
    fcpattern_from_fontstyle(style, pattern);

    AutoFcCharSet charSet;
    FcCharSetAddChar(charSet, character);
    FcPatternAddCharSet(pattern, FC_CHARSET, charSet);

    if (bcp47Count > 0) {
        AutoFcLangSet langSet;
        for (int i = bcp47Count; i-- > 0;) {
            FcLangSetAdd(langSet, (const FcChar8*) bcp47[i]);
        }
        FcPatternAddLangSet(pattern, FC_LANG, langSet);
    }

    FcConfigSubstitute(mFcConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    AutoFcPattern font(FcFontMatch(mFcConfig, pattern, &result));
    if (nullptr == font || !FontAccessible(font) || !FontContainsCharacter(font, character)) {
        return nullptr;
    }

    return createFontFaceFromFcPattern(font);
}


Typeface* FontStyleSet::matchStyle(const FontStyle& style) {

    AutoFcPattern pattern;
    fcpattern_from_fontstyle(style, pattern);
    FcConfigSubstitute(fFontMgr->mFcConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcFontSet* fontSets[1] = {fFontSet};
    AutoFcPattern match(FcFontSetMatch(fFontMgr->mFcConfig,
                                       fontSets, ARRAY_COUNT(fontSets),
                                       pattern, &result));
    if (nullptr == match) {
        return nullptr;
    }

    return fFontMgr->createFontFaceFromFcPattern(match);
}

Typeface* FontStyleSet::createTypeface(int index) {
    FcPattern* match = fFontSet->fonts[index];
    return fFontMgr->createFontFaceFromFcPattern(match);
}

void FontStyleSet::getStyle(int index, FontStyle* style, std::string* styleName) {
    if (index < 0 || fFontSet->nfont <= index) {
        return;
    }

    if (style) {
        *style = fontstyle_from_fcpattern(fFontSet->fonts[index]);
    }
    if (styleName) {
        *styleName = get_string(fFontSet->fonts[index], FC_STYLE);
    }
}
