/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "CacheTexture.h"
#include "PixelBuffer.h"
#include "TextureRect.h"

#define DEBUG_FONT_RENDERER 0

#define TEXTURE_BORDER_SIZE 1
#define CACHE_BLOCK_ROUNDING_SIZE 4

///////////////////////////////////////////////////////////////////////////////
// CacheBlock
///////////////////////////////////////////////////////////////////////////////

/**
 * Insert new block into existing linked list of blocks. Blocks are sorted in increasing-width
 * order, except for the final block (the remainder space at the right, since we fill from the
 * left).
 */
CacheBlock* CacheBlock::insertBlock(CacheBlock* head, CacheBlock* newBlock) {
#if DEBUG_FONT_RENDERER
    printf("insertBlock: this, x, y, w, h = %p, %d, %d, %d, %d\n",
           newBlock, newBlock->mX, newBlock->mY,
           newBlock->mWidth, newBlock->mHeight);
#endif

    CacheBlock* currBlock = head;
    CacheBlock* prevBlock = nullptr;

    while (currBlock && currBlock->mY != TEXTURE_BORDER_SIZE) {
        if (newBlock->mWidth < currBlock->mWidth) {
            newBlock->mNext = currBlock;
            newBlock->mPrev = prevBlock;
            currBlock->mPrev = newBlock;

            if (prevBlock) {
                prevBlock->mNext = newBlock;
                return head;
            } else {
                return newBlock;
            }
        }

        prevBlock = currBlock;
        currBlock = currBlock->mNext;
    }

    // new block larger than all others - insert at end (but before the remainder space, if there)
    newBlock->mNext = currBlock;
    newBlock->mPrev = prevBlock;

    if (currBlock) {
        currBlock->mPrev = newBlock;
    }

    if (prevBlock) {
        prevBlock->mNext = newBlock;
        return head;
    } else {
        return newBlock;
    }
}

CacheBlock* CacheBlock::removeBlock(CacheBlock* head, CacheBlock* blockToRemove) {
#if DEBUG_FONT_RENDERER
    printf("removeBlock: this, x, y, w, h = %p, %d, %d, %d, %d\n",
           blockToRemove, blockToRemove->mX, blockToRemove->mY,
           blockToRemove->mWidth, blockToRemove->mHeight);
#endif

    CacheBlock* newHead = head;
    CacheBlock* nextBlock = blockToRemove->mNext;
    CacheBlock* prevBlock = blockToRemove->mPrev;

    if (prevBlock) {
        prevBlock->mNext = nextBlock;
    } else {
        newHead = nextBlock;
    }

    if (nextBlock) {
        nextBlock->mPrev = prevBlock;
    }

    delete blockToRemove;

    return newHead;
}

///////////////////////////////////////////////////////////////////////////////
// CacheTexture
///////////////////////////////////////////////////////////////////////////////

CacheTexture::CacheTexture(uint16_t width, uint16_t height, GLenum format, uint32_t maxQuadCount)
        : mTexture(), mWidth(width), mHeight(height), mFormat(format),
          mMaxQuadCount(maxQuadCount), mHasUnpackRowLength(true) {
    mTexture.blend = true;

    mCacheBlocks = new CacheBlock(TEXTURE_BORDER_SIZE, TEXTURE_BORDER_SIZE,
                                  getWidth() - TEXTURE_BORDER_SIZE, getHeight() - TEXTURE_BORDER_SIZE);
}

CacheTexture::~CacheTexture() {
    releaseMesh();
    releasePixelBuffer();
    reset();
}

void CacheTexture::reset() {
    // Delete existing cache blocks
    while (mCacheBlocks != nullptr) {
        CacheBlock* tmpBlock = mCacheBlocks;
        mCacheBlocks = mCacheBlocks->mNext;
        delete tmpBlock;
    }
    mNumGlyphs = 0;
    mCurrentQuad = 0;
}

void CacheTexture::init() {
    // reset, then create a new remainder space to start again
    reset();
    mCacheBlocks = new CacheBlock(TEXTURE_BORDER_SIZE, TEXTURE_BORDER_SIZE,
                                  getWidth() - TEXTURE_BORDER_SIZE, getHeight() - TEXTURE_BORDER_SIZE);
}

void CacheTexture::releaseMesh() {
    delete[] mMesh;
}

void CacheTexture::releasePixelBuffer() {
    if (mPixelBuffer) {
        delete mPixelBuffer;
        mPixelBuffer = nullptr;
    }
    mTexture.deleteTexture();
    mDirty = false;
    mCurrentQuad = 0;
}

void CacheTexture::setLinearFiltering(bool linearFiltering) {
    mTexture.setFilter(linearFiltering ? GL_LINEAR : GL_NEAREST);
}

void CacheTexture::allocateMesh() {
    if (!mMesh) {
        mMesh = new TextureVertex[mMaxQuadCount * 4];
    }
}

void CacheTexture::allocatePixelBuffer() {
    if (!mPixelBuffer) {
        mPixelBuffer = PixelBuffer::create(mFormat, getWidth(), getHeight());
    }

    mTexture.setFilter(getLinearFiltering() ? GL_LINEAR : GL_NEAREST);
    mTexture.resize(mWidth, mHeight, mFormat);
    mTexture.setWrap(GL_CLAMP_TO_EDGE);
}

bool CacheTexture::upload() {
    const TextureRect& dirtyRect = mDirtyRect;

    uint32_t x = mHasUnpackRowLength ? dirtyRect.left : 0;
    uint32_t y = dirtyRect.top;
    uint32_t width = mHasUnpackRowLength ? dirtyRect.getWidth() : getWidth();
    uint32_t height = dirtyRect.getHeight();

    // The unpack row length only needs to be specified when a new
    // texture is bound
    if (mHasUnpackRowLength) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, getWidth());
    }

    mPixelBuffer->upload(x, y, width, height);
    setDirty(false);

    printf("CacheTexture %d upload: x, y, width height = %d, %d, %d, %d\n",
           getTextureId(), x, y, width, height);

    return mHasUnpackRowLength;
}

void CacheTexture::setDirty(bool dirty) {
    mDirty = dirty;
    if (!dirty) {
        mDirtyRect.setEmpty();
    }
}

bool CacheTexture::fitBitmap(const GlyphInfo& glyph, uint32_t* retOriginX, uint32_t* retOriginY) {
    switch (glyph.fFormat) {
        case GlyphInfo::Format_A8:
            if (mFormat != GL_RED) {
#if DEBUG_FONT_RENDERER
                printf("fitBitmap: texture format %x is inappropriate for monochromatic glyphs\n",
                       mFormat);
#endif
                return false;
            }
            break;
        default:
#if DEBUG_FONT_RENDERER
            printf("fitBitmap: unknown glyph format %x encountered\n", glyph.fFormat);
#endif
            return false;
    }

    if (glyph.fHeight + TEXTURE_BORDER_SIZE * 2 > getHeight()) {
        return false;
    }

    uint16_t glyphW = glyph.fWidth + TEXTURE_BORDER_SIZE;
    uint16_t glyphH = glyph.fHeight + TEXTURE_BORDER_SIZE;

    // roundedUpW equals glyphW to the next multiple of CACHE_BLOCK_ROUNDING_SIZE.
    // This columns for glyphs that are close but not necessarily exactly the same size. It trades
    // off the loss of a few pixels for some glyphs against the ability to store more glyphs
    // of varying sizes in one block.
    uint16_t roundedUpW = (glyphW + CACHE_BLOCK_ROUNDING_SIZE - TEXTURE_BORDER_SIZE) & -CACHE_BLOCK_ROUNDING_SIZE;

    CacheBlock* cacheBlock = mCacheBlocks;
    while (cacheBlock) {
        // Store glyph in this block iff: it fits the block's remaining space and:
        // it's the remainder space (mY == 0) or there's only enough height for this one glyph
        // or it's within ROUNDING_SIZE of the block width
        if (roundedUpW <= cacheBlock->mWidth && glyphH <= cacheBlock->mHeight &&
            (cacheBlock->mY == TEXTURE_BORDER_SIZE ||
             (cacheBlock->mWidth - roundedUpW < CACHE_BLOCK_ROUNDING_SIZE))) {
            if (cacheBlock->mHeight - glyphH < glyphH) {
                // Only enough space for this glyph - don't bother rounding up the width
                roundedUpW = glyphW;
            }

            *retOriginX = cacheBlock->mX;
            *retOriginY = cacheBlock->mY;

            // If this is the remainder space, create a new cache block for this column. Otherwise,
            // adjust the info about this column.
            if (cacheBlock->mY == TEXTURE_BORDER_SIZE) {
                uint16_t oldX = cacheBlock->mX;
                // Adjust remainder space dimensions
                cacheBlock->mWidth -= roundedUpW;
                cacheBlock->mX += roundedUpW;

                if (getHeight() - glyphH >= glyphH) {
                    // There's enough height left over to create a new CacheBlock
                    CacheBlock* newBlock = new CacheBlock(oldX, glyphH + TEXTURE_BORDER_SIZE,
                                                          roundedUpW, getHeight() - glyphH - TEXTURE_BORDER_SIZE);
#if DEBUG_FONT_RENDERER
                    printf("fitBitmap: Created new block: this, x, y, w, h = %p, %d, %d, %d, %d\n",
                           newBlock, newBlock->mX, newBlock->mY,
                           newBlock->mWidth, newBlock->mHeight);
#endif
                    mCacheBlocks = CacheBlock::insertBlock(mCacheBlocks, newBlock);
                }
            } else {
                // Insert into current column and adjust column dimensions
                cacheBlock->mY += glyphH;
                cacheBlock->mHeight -= glyphH;
#if DEBUG_FONT_RENDERER
                printf("fitBitmap: Added to existing block: this, x, y, w, h = %p, %d, %d, %d, %d\n",
                       cacheBlock, cacheBlock->mX, cacheBlock->mY,
                       cacheBlock->mWidth, cacheBlock->mHeight);
#endif
            }

            if (cacheBlock->mHeight < std::min(glyphH, glyphW)) {
                // If remaining space in this block is too small to be useful, remove it
                mCacheBlocks = CacheBlock::removeBlock(mCacheBlocks, cacheBlock);
            }

            mDirty = true;
            const TextureRect r(*retOriginX - TEXTURE_BORDER_SIZE, *retOriginY - TEXTURE_BORDER_SIZE,
                                *retOriginX + glyphW, *retOriginY + glyphH);
            mDirtyRect.unionWith(r);
            mNumGlyphs++;

            return true;
        }
        cacheBlock = cacheBlock->mNext;
    }
#if DEBUG_FONT_RENDERER
    printf("fitBitmap: returning false for glyph of size %d, %d\n", glyphW, glyphH);
#endif
    return false;
}

uint32_t CacheTexture::calculateFreeMemory() const {
    CacheBlock* cacheBlock = mCacheBlocks;
    uint32_t free = 0;
    // currently only two formats are supported: GL_RED or GL_RGBA;
    uint32_t bpp = mFormat == GL_RGBA ? 4 : 1;
    while (cacheBlock) {
        free += bpp * cacheBlock->mWidth * cacheBlock->mHeight;
        cacheBlock = cacheBlock->mNext;
    }
    return free;
}

