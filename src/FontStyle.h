//
// Created by bq on 2019-08-13.
//

#ifndef FONT_DEMO_FONTSTYLE_H
#define FONT_DEMO_FONTSTYLE_H

template<typename T>
constexpr const T& TMin(const T& a, const T& b) {
    return (a < b) ? a : b;
}

template<typename T>
constexpr const T& TMax(const T& a, const T& b) {
    return (b < a) ? a : b;
}

/** @return value pinned (clamped) between min and max, inclusively.
*/
template<typename T>
static constexpr const T& TPin(const T& value, const T& min, const T& max) {
    return TMax(TMin(value, max), min);
}

class FontStyle {
public:
    enum Weight {
        kInvisible_Weight = 0,
        kThin_Weight = 100,
        kExtraLight_Weight = 200,
        kLight_Weight = 300,
        kNormal_Weight = 400,
        kMedium_Weight = 500,
        kSemiBold_Weight = 600,
        kBold_Weight = 700,
        kExtraBold_Weight = 800,
        kBlack_Weight = 900,
        kExtraBlack_Weight = 1000,
    };

    enum Slant {
        kUpright_Slant,
        kItalic_Slant,
        kOblique_Slant,
    };

    enum Width {
        kUltraCondensed_Width = 1,
        kExtraCondensed_Width = 2,
        kCondensed_Width = 3,
        kSemiCondensed_Width = 4,
        kNormal_Width = 5,
        kSemiExpanded_Width = 6,
        kExpanded_Width = 7,
        kExtraExpanded_Width = 8,
        kUltraExpanded_Width = 9,
    };

    constexpr FontStyle(int weight, int width, Slant slant) : fValue(
            (TPin<int>(weight, kInvisible_Weight, kExtraBlack_Weight)) +
            (TPin<int>(width, kUltraCondensed_Width, kUltraExpanded_Width) << 16) +
            (TPin<int>(slant, kUpright_Slant, kOblique_Slant) << 24)
    ) {}

    constexpr FontStyle() : FontStyle{kNormal_Weight, kNormal_Width, kUpright_Slant} {}

    bool operator==(const FontStyle& rhs) const {
        return fValue == rhs.fValue;
    }

    bool isItalic() const { return slant() != FontStyle::kUpright_Slant; }

    uint32_t value() const { return fValue; }

    int weight() const { return fValue & 0xFFFF; }

    int width() const { return (fValue >> 16) & 0xFF; }

    Slant slant() const { return (Slant) ((fValue >> 24) & 0xFF); }

    static constexpr FontStyle Normal() {
        return {kNormal_Weight, kNormal_Width, kUpright_Slant};
    }

    static constexpr FontStyle Bold() {
        return {kBold_Weight, kNormal_Width, kUpright_Slant};
    }

    static constexpr FontStyle Italic() {
        return {kNormal_Weight, kNormal_Width, kItalic_Slant};
    }

    static constexpr FontStyle BoldItalic() {
        return {kBold_Weight, kNormal_Width, kItalic_Slant};
    }

private:
    uint32_t fValue;
};

#endif //FONT_DEMO_FONTSTYLE_H
