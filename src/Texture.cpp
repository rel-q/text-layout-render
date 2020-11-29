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

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "Texture.h"

void Texture::setWrapST(GLenum wrapS, GLenum wrapT, bool bindTexture, bool force,
                        GLenum renderTarget) {

    if (force || wrapS != mWrapS || wrapT != mWrapT) {
        mWrapS = wrapS;
        mWrapT = wrapT;

        if (bindTexture) {
            glBindTexture(renderTarget, mId);
        }

        glTexParameteri(renderTarget, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(renderTarget, GL_TEXTURE_WRAP_T, wrapT);
    }
}

void Texture::setFilterMinMag(GLenum min, GLenum mag, bool bindTexture, bool force,
                              GLenum renderTarget) {

    if (force || min != mMinFilter || mag != mMagFilter) {
        mMinFilter = min;
        mMagFilter = mag;

        if (bindTexture) {
            glBindTexture(renderTarget, mId);
        }

        if (mipMap && min == GL_LINEAR) min = GL_LINEAR_MIPMAP_LINEAR;

        glTexParameteri(renderTarget, GL_TEXTURE_MIN_FILTER, min);
        glTexParameteri(renderTarget, GL_TEXTURE_MAG_FILTER, mag);
    }
}

void Texture::deleteTexture() {
    glDeleteTextures(1, &mId);
    mId = 0;
}

bool Texture::updateSize(uint32_t width, uint32_t height, GLint format) {
    if (mWidth == width && mHeight == height && mFormat == format) {
        return false;
    }
    mWidth = width;
    mHeight = height;
    mFormat = format;
    return true;
}

void Texture::resetCachedParams() {
    mWrapS = GL_REPEAT;
    mWrapT = GL_REPEAT;
    mMinFilter = GL_NEAREST_MIPMAP_LINEAR;
    mMagFilter = GL_LINEAR;
}

void Texture::upload(GLint internalformat, uint32_t width, uint32_t height,
                     GLenum format, GLenum type, const void* pixels) {
    bool needsAlloc = updateSize(width, height, internalformat);
    if (!mId) {
        glGenTextures(1, &mId);
        needsAlloc = true;
        resetCachedParams();
    }
    glBindTexture(GL_TEXTURE_2D, mId);
    if (needsAlloc) {
        glTexImage2D(GL_TEXTURE_2D, 0, mFormat, mWidth, mHeight, 0,
                     format, type, pixels);
    } else if (pixels) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, mFormat, mWidth, mHeight, 0,
                        format, type, pixels);
    }
}

void Texture::wrap(GLuint id, uint32_t width, uint32_t height, GLint format) {
    mId = id;
    mWidth = width;
    mHeight = height;
    mFormat = format;
}
