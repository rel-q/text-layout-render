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

#include "paint_record.h"

namespace txt {

PaintRecord::~PaintRecord() = default;

PaintRecord::PaintRecord(TextStyle style,
                         double offset_x,
                         double offset_y,
                         std::unique_ptr<RunBuffer> buffer,
                         FontMetrics metrics,
                         size_t line,
                         double x_start,
                         double x_end,
                         bool is_ghost)
        : style_(style),
          offset_x_(offset_x),
          offset_y_(offset_y),
          run_buffer_(std::move(buffer)),
          metrics_(metrics),
          line_(line),
          x_start_(x_start),
          x_end_(x_end),
          is_ghost_(is_ghost) {}

PaintRecord::PaintRecord(PaintRecord&& other) {
    style_ = other.style_;
    run_buffer_ = std::move(other.run_buffer_);
    offset_x_ = other.offset_x_;
    offset_y_ = other.offset_y_;
    metrics_ = other.metrics_;
    line_ = other.line_;
    x_start_ = other.x_start_;
    x_end_ = other.x_end_;
    is_ghost_ = other.is_ghost_;
}

PaintRecord& PaintRecord::operator=(PaintRecord&& other) {
    style_ = other.style_;
    offset_x_ = other.offset_x_;
    offset_y_ = other.offset_y_;
    run_buffer_ = std::move(other.run_buffer_);
    metrics_ = other.metrics_;
    line_ = other.line_;
    x_start_ = other.x_start_;
    x_end_ = other.x_end_;
    is_ghost_ = other.is_ghost_;
    return *this;
}

void PaintRecord::SetOffset(double x, double y) {
    offset_x_ = x;
    offset_y_ = y;
}

}  // namespace txt
