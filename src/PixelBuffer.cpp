/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "PixelBuffer.h"
#include <memory>


///////////////////////////////////////////////////////////////////////////////
// CPU pixel buffer
///////////////////////////////////////////////////////////////////////////////

class CpuPixelBuffer : public PixelBuffer {
public:
    CpuPixelBuffer(GLenum format, uint32_t width, uint32_t height);

    uint8_t* map(AccessMode mode = kAccessMode_ReadWrite) override;

    uint8_t* getMappedPointer() const override;

    void upload(uint32_t x, uint32_t y, uint32_t width, uint32_t height, int offset) override;

protected:
    void unmap() override;

private:
    std::unique_ptr<uint8_t[]> mBuffer;
};

CpuPixelBuffer::CpuPixelBuffer(GLenum format, uint32_t width, uint32_t height)
        : PixelBuffer(format, width, height), mBuffer(new uint8_t[width * height * formatSize(format)]) {
}

uint8_t* CpuPixelBuffer::map(AccessMode mode) {
    if (mAccessMode == kAccessMode_None) {
        mAccessMode = mode;
    }
    return mBuffer.get();
}

void CpuPixelBuffer::unmap() {
    mAccessMode = kAccessMode_None;
}

uint8_t* CpuPixelBuffer::getMappedPointer() const {
    return mAccessMode == kAccessMode_None ? nullptr : mBuffer.get();
}

void CpuPixelBuffer::upload(uint32_t x, uint32_t y, uint32_t width, uint32_t height, int offset) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height,
                    mFormat, GL_UNSIGNED_BYTE, &mBuffer[offset]);
}

///////////////////////////////////////////////////////////////////////////////
// Factory
///////////////////////////////////////////////////////////////////////////////

PixelBuffer* PixelBuffer::create(GLenum format,
                                 uint32_t width, uint32_t height, BufferType type) {
    return new CpuPixelBuffer(format, width, height);
}
