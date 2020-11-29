/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef FontMetrics_DEFINED
#define FontMetrics_DEFINED


struct FontMetrics {

    /** \enum FontMetricsFlags
     FontMetricsFlags are set in fFlags when underline and strikeout metrics are valid;
     the underline or strikeout metric may be valid and zero.
     Fonts with embedded bitmaps may not have valid underline or strikeout metrics.
     */
    enum FontMetricsFlags {
        kUnderlineThicknessIsValid_Flag = 1 << 0, //!< set if fUnderlineThickness is valid
        kUnderlinePositionIsValid_Flag = 1 << 1, //!< set if fUnderlinePosition is valid
        kStrikeoutThicknessIsValid_Flag = 1 << 2, //!< set if fStrikeoutThickness is valid
        kStrikeoutPositionIsValid_Flag = 1 << 3, //!< set if fStrikeoutPosition is valid
    };

    uint32_t fFlags;              //!< is set to FontMetricsFlags when metrics are valid
    float fTop;                //!< extent above baseline
    float fAscent;             //!< distance to reserve above baseline
    float fDescent;            //!< distance to reserve below baseline
    float fBottom;             //!< extent below baseline
    float fLeading;            //!< distance to add between lines
    float fAvgCharWidth;       //!< average character width
    float fMaxCharWidth;       //!< maximum character width
    float fXMin;               //!< minimum x
    float fXMax;               //!< maximum x
    float fXHeight;            //!< height of lower-case 'x'
    float fCapHeight;          //!< height of an upper-case letter
    float fUnderlineThickness; //!< underline thickness
    float fUnderlinePosition;  //!< underline position relative to baseline
    float fStrikeoutThickness; //!< strikeout thickness
    float fStrikeoutPosition;  //!< strikeout position relative to baseline

};

#endif
