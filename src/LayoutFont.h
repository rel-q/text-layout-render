//
// Created by bq on 2019-08-20.
//

#ifndef FONT_DEMO_LAYOUTFONT_H
#define FONT_DEMO_LAYOUTFONT_H

#include <minikin/MinikinFont.h>
#include "Typeface.h"

class LayoutFont : public minikin::MinikinFont {
public:
    explicit LayoutFont(Typeface* typeface);

    ~LayoutFont() override;

    float GetHorizontalAdvance(uint32_t glyph_id,
                               const minikin::MinikinPaint& paint) const override;

    void GetBounds(minikin::MinikinRect* bounds,
                   uint32_t glyph_id,
                   const minikin::MinikinPaint& paint) const override;

    hb_face_t* CreateHarfBuzzFace() const override;

    const std::vector<minikin::FontVariation>& GetAxes() const override;

    Typeface* typeface() const {
        return typeface_;
    }

private:
    Typeface* typeface_;
    std::vector<minikin::FontVariation> variations_;
};

#endif //FONT_DEMO_LAYOUTFONT_H
