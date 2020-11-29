/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIB_TXT_SRC_PAINT_RECORD_H_
#define LIB_TXT_SRC_PAINT_RECORD_H_

#include "text_style.h"
#include "FontMetrics.h"
#include "Typeface.h"

namespace txt {

struct RunBuffer {
    std::vector<uint16_t> glyphs;   //!< storage for glyphs in run
    std::vector<float> pos;
    Typeface* typeface;
};

// PaintRecord holds the layout data after Paragraph::Layout() is called. This
// stores all nessecary offsets, blobs, metrics, and more for Skia to draw the
// text.
class PaintRecord {
public:
    PaintRecord() = delete;

    ~PaintRecord();

    PaintRecord(TextStyle style,
                double offset_x,
                double offset_y,
                std::unique_ptr<RunBuffer> buffer,
                FontMetrics metrics,
                size_t line,
                double x_start,
                double x_end,
                bool is_ghost);


    PaintRecord(PaintRecord&& other);

    PaintRecord& operator=(PaintRecord&& other);

    double offset_x() const { return offset_x_; }

    double offset_y() const { return offset_y_; }

    void SetOffset(double x, double y);

    const FontMetrics& metrics() const { return metrics_; }

    const TextStyle& style() const { return style_; }

    RunBuffer* buffer() const { return run_buffer_.get(); }

    size_t line() const { return line_; }

    double x_start() const { return x_start_; }

    double x_end() const { return x_end_; }

    double GetRunWidth() const { return x_end_ - x_start_; }

    bool isGhost() const { return is_ghost_; }

private:
    TextStyle style_;
    // offset_ is the overall offset of the origin of the SkTextBlob.
    double offset_x_;
    double offset_y_;
    // SkTextBlob stores the glyphs and coordinates to draw them.
    // FontMetrics stores the measurements of the font used.
    FontMetrics metrics_;
    std::unique_ptr<RunBuffer> run_buffer_;
    size_t line_;
    double x_start_ = 0.0f;
    double x_end_ = 0.0f;
    // 'Ghost' runs represent trailing whitespace. 'Ghost' runs should not have
    // decorations painted on them and do not impact layout of visible glyphs.
    bool is_ghost_ = false;

};

}  // namespace txt

#endif  // LIB_TXT_SRC_PAINT_RECORD_H_
