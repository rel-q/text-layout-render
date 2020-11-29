//
// Created by bq on 2019-08-20.
//

#include <hb-ft.h>
#include <hb-font.hh>
#include "LayoutFont.h"
#include "minikin/MinikinFont.h"

LayoutFont::LayoutFont(Typeface* typeface)
        : MinikinFont(typeface->id()), typeface_(typeface) {}

LayoutFont::~LayoutFont() {
    typeface_ = nullptr;
};


float LayoutFont::GetHorizontalAdvance(uint32_t glyph_id,
                                       const minikin::MinikinPaint& paint) const {
    return 0;
}

void LayoutFont::GetBounds(minikin::MinikinRect* bounds,
                           uint32_t glyph_id,
                           const minikin::MinikinPaint& paint) const {
}

hb_face_t* LayoutFont::CreateHarfBuzzFace() const {
    // if create hb_font_t with this way, we cannot control the layout fully
    // if we have some special requirement in the future, maybe we shall change it.
    return hb_ft_face_create(typeface_->mFace, nullptr);
}

const std::vector<minikin::FontVariation>& LayoutFont::GetAxes() const {
    return variations_;
}
