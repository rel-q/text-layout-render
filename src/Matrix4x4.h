//
// Created by bq on 2019-08-20.
//

#ifndef CANVAS_MATRIX4X4_H
#define CANVAS_MATRIX4X4_H

#include <cstdint>
#include <iostream>
#include <memory>

class Matrix4x4 {
public:
    float data[16];

    enum Entry {
        kScaleX = 0,
        kSkewY = 1,
        kPerspective0 = 3,
        kSkewX = 4,
        kScaleY = 5,
        kPerspective1 = 7,
        kScaleZ = 10,
        kTranslateX = 12,
        kTranslateY = 13,
        kTranslateZ = 14,
        kPerspective2 = 15
    };

    // NOTE: The flags from kTypeIdentity to kTypePerspective
    //       must be kept in sync with the type flags found
    //       in SkMatrix
    enum Type {
        kTypeIdentity = 0,
        kTypeTranslate = 0x1,
        kTypeScale = 0x2,
        kTypeAffine = 0x4,
        kTypePerspective = 0x8,
        kTypeRectToRect = 0x10,
        kTypeUnknown = 0x20,
    };

    static const int sGeometryMask = 0xf;

    Matrix4x4() {
        loadIdentity();
    }

    Matrix4x4(const float* v) {
        load(v);
    }

    float operator[](int index) const {
        return data[index];
    }

    float& operator[](int index) {
        mType = kTypeUnknown;
        return data[index];
    }

    friend bool operator==(const Matrix4x4& a, const Matrix4x4& b) {
        return !memcmp(&a.data[0], &b.data[0], 16 * sizeof(float));
    }

    friend bool operator!=(const Matrix4x4& a, const Matrix4x4& b) {
        return !(a == b);
    }

    Matrix4x4& operator=(const float* v) {
        load(v);
        return *this;
    }

    void loadIdentity();

    void load(const float* v);

    void loadInverse(const Matrix4x4& v);

    void loadTranslate(float x, float y, float z);

    void loadScale(float sx, float sy, float sz);

    void loadSkew(float sx, float sy);

    void loadRotate(float angle);

    void loadRotate(float angle, float x, float y, float z);

    void loadMultiply(const Matrix4x4& u, const Matrix4x4& v);

    void loadOrtho(float left, float right, float bottom, float top, float near, float far);

    void loadOrtho(int width, int height) {
        loadOrtho(0, width, height, 0, -1, 1);
    }

    uint8_t getType() const;

    void multiplyInverse(const Matrix4x4& v) {
        Matrix4x4 inv;
        inv.loadInverse(v);
        multiply(inv);
    }

    void multiply(const Matrix4x4& v) {
        if (!v.isIdentity()) {
            Matrix4x4 u;
            u.loadMultiply(*this, v);
            *this = u;
        }
    }

    void translate(float x, float y, float z = 0) {
        if ((getType() & sGeometryMask) <= kTypeTranslate) {
            data[kTranslateX] += x;
            data[kTranslateY] += y;
            data[kTranslateZ] += z;
            mType |= kTypeUnknown;
        } else {
            // Doing a translation will only affect the translate bit of the type
            // Save the type
            uint8_t type = mType;

            Matrix4x4 u;
            u.loadTranslate(x, y, z);
            multiply(u);

            // Restore the type and fix the translate bit
            mType = type;
            if (data[kTranslateX] != 0.0f || data[kTranslateY] != 0.0f) {
                mType |= kTypeTranslate;
            } else {
                mType &= ~kTypeTranslate;
            }
        }
    }

    void scale(float sx, float sy, float sz = 1.0) {
        Matrix4x4 u;
        u.loadScale(sx, sy, sz);
        multiply(u);
    }

    void skew(float sx, float sy) {
        Matrix4x4 u;
        u.loadSkew(sx, sy);
        multiply(u);
    }

    void rotate(float angle, float x, float y, float z) {
        Matrix4x4 u;
        u.loadRotate(angle, x, y, z);
        multiply(u);
    }

    /**
     * If the matrix is identity or translate and/or scale.
     */
    bool isSimple() const;

    bool isIdentity() const;

    void copyTo(float* v) const;

    float getTranslateX() const;

    float getTranslateY() const;

    void dump(const char* label = nullptr) const;

    friend std::ostream& operator<<(std::ostream& os, const Matrix4x4& matrix) {
        if (matrix.isSimple()) {
            os << "offset " << matrix.getTranslateX() << "x" << matrix.getTranslateY();
            os << ", scale " << matrix[kScaleX] << "x" << matrix[kScaleY];
        } else {
            os << "[" << matrix[0];
            for (int i = 1; i < 16; i++) {
                os << ", " << matrix[i];
            }
            os << "]";
        }
        return os;
    }

    static const Matrix4x4& identity();

    void invalidateType() { mType = kTypeUnknown; }

private:
    mutable uint8_t mType;

    inline float get(int i, int j) const {
        return data[i * 4 + j];
    }

    inline void set(int i, int j, float v) {
        data[i * 4 + j] = v;
    }

    uint8_t getGeometryType() const;

};

typedef Matrix4x4 mat4;


#endif //CANVAS_MATRIX4X4_H
