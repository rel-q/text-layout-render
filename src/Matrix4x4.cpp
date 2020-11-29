//
// Created by bq on 2019-08-20.
//

#include "Matrix4x4.h"
#include <memory>
#include <math.h>
#include <string.h>

static const float EPSILON = 0.0000001f;

const Matrix4x4& Matrix4x4::identity() {
    static Matrix4x4 sIdentity;
    return sIdentity;
}

void Matrix4x4::loadIdentity() {
    data[kScaleX] = 1.0f;
    data[kSkewY] = 0.0f;
    data[2] = 0.0f;
    data[kPerspective0] = 0.0f;

    data[kSkewX] = 0.0f;
    data[kScaleY] = 1.0f;
    data[6] = 0.0f;
    data[kPerspective1] = 0.0f;

    data[8] = 0.0f;
    data[9] = 0.0f;
    data[kScaleZ] = 1.0f;
    data[11] = 0.0f;

    data[kTranslateX] = 0.0f;
    data[kTranslateY] = 0.0f;
    data[kTranslateZ] = 0.0f;
    data[kPerspective2] = 1.0f;

    mType = kTypeIdentity | kTypeRectToRect;
}

static bool isZero(float f) {
    return fabs(f) <= EPSILON;
}

uint8_t Matrix4x4::getType() const {
    if (mType & kTypeUnknown) {
        mType = kTypeIdentity;

        if (data[kPerspective0] != 0.0f || data[kPerspective1] != 0.0f ||
            data[kPerspective2] != 1.0f) {
            mType |= kTypePerspective;
        }

        if (data[kTranslateX] != 0.0f || data[kTranslateY] != 0.0f) {
            mType |= kTypeTranslate;
        }

        float m00 = data[kScaleX];
        float m01 = data[kSkewX];
        float m10 = data[kSkewY];
        float m11 = data[kScaleY];
        float m32 = data[kTranslateZ];

        if (m01 != 0.0f || m10 != 0.0f || m32 != 0.0f) {
            mType |= kTypeAffine;
        }

        if (m00 != 1.0f || m11 != 1.0f) {
            mType |= kTypeScale;
        }

        // The following section determines whether the matrix will preserve
        // rectangles. For instance, a rectangle transformed by a pure
        // translation matrix will result in a rectangle. A rectangle
        // transformed by a 45 degrees rotation matrix is not a rectangle.
        // If the matrix has a perspective component then we already know
        // it doesn't preserve rectangles.
        if (!(mType & kTypePerspective)) {
            if ((isZero(m00) && isZero(m11) && !isZero(m01) && !isZero(m10)) ||
                (isZero(m01) && isZero(m10) && !isZero(m00) && !isZero(m11))) {
                mType |= kTypeRectToRect;
            }
        }
    }
    return mType;
}

uint8_t Matrix4x4::getGeometryType() const {
    return getType() & sGeometryMask;
}

bool Matrix4x4::isSimple() const {
    return getGeometryType() <= (kTypeScale | kTypeTranslate) && (data[kTranslateZ] == 0.0f);
}

bool Matrix4x4::isIdentity() const {
    return getGeometryType() == kTypeIdentity;
}

void Matrix4x4::load(const float* v) {
    memcpy(data, v, sizeof(data));
    mType = kTypeUnknown;
}

void Matrix4x4::loadInverse(const Matrix4x4& v) {
    // Fast case for common translation matrices
    // Reset the matrix
    // Unnamed fields are never written to except by
    // loadIdentity(), they don't need to be reset
    data[kScaleX] = 1.0f;
    data[kSkewX] = 0.0f;

    data[kScaleY] = 1.0f;
    data[kSkewY] = 0.0f;

    data[kScaleZ] = 1.0f;

    data[kPerspective0] = 0.0f;
    data[kPerspective1] = 0.0f;
    data[kPerspective2] = 1.0f;

    // No need to deal with kTranslateZ because isPureTranslate()
    // only returns true when the kTranslateZ component is 0
    data[kTranslateX] = -v.data[kTranslateX];
    data[kTranslateY] = -v.data[kTranslateY];
    data[kTranslateZ] = 0.0f;

    // A "pure translate" matrix can be identity or translation
    mType = v.getType();

}

void Matrix4x4::copyTo(float* v) const {
    memcpy(v, data, sizeof(data));
}

float Matrix4x4::getTranslateX() const {
    return data[kTranslateX];
}

float Matrix4x4::getTranslateY() const {
    return data[kTranslateY];
}

void Matrix4x4::loadTranslate(float x, float y, float z) {
    loadIdentity();

    data[kTranslateX] = x;
    data[kTranslateY] = y;
    data[kTranslateZ] = z;

    mType = kTypeTranslate | kTypeRectToRect;
}

void Matrix4x4::loadScale(float sx, float sy, float sz) {
    loadIdentity();

    data[kScaleX] = sx;
    data[kScaleY] = sy;
    data[kScaleZ] = sz;

    mType = kTypeScale | kTypeRectToRect;
}

void Matrix4x4::loadSkew(float sx, float sy) {
    loadIdentity();

    data[kScaleX] = 1.0f;
    data[kSkewX] = sx;
    data[kTranslateX] = 0.0f;

    data[kSkewY] = sy;
    data[kScaleY] = 1.0f;
    data[kTranslateY] = 0.0f;

    data[kPerspective0] = 0.0f;
    data[kPerspective1] = 0.0f;
    data[kPerspective2] = 1.0f;

    mType = kTypeUnknown;
}

void Matrix4x4::loadRotate(float angle) {
    angle *= float(M_PI / 180.0f);
    float c = cosf(angle);
    float s = sinf(angle);

    loadIdentity();

    data[kScaleX] = c;
    data[kSkewX] = -s;

    data[kSkewY] = s;
    data[kScaleY] = c;

    mType = kTypeUnknown;
}

void Matrix4x4::loadRotate(float angle, float x, float y, float z) {
    data[kPerspective0] = 0.0f;
    data[kPerspective1] = 0.0f;
    data[11] = 0.0f;
    data[kTranslateX] = 0.0f;
    data[kTranslateY] = 0.0f;
    data[kTranslateZ] = 0.0f;
    data[kPerspective2] = 1.0f;

    angle *= float(M_PI / 180.0f);
    float c = cosf(angle);
    float s = sinf(angle);

    const float length = sqrtf(x * x + y * y + z * z);
    float recipLen = 1.0f / length;
    x *= recipLen;
    y *= recipLen;
    z *= recipLen;

    const float nc = 1.0f - c;
    const float xy = x * y;
    const float yz = y * z;
    const float zx = z * x;
    const float xs = x * s;
    const float ys = y * s;
    const float zs = z * s;

    data[kScaleX] = x * x * nc + c;
    data[kSkewX] = xy * nc - zs;
    data[8] = zx * nc + ys;
    data[kSkewY] = xy * nc + zs;
    data[kScaleY] = y * y * nc + c;
    data[9] = yz * nc - xs;
    data[2] = zx * nc - ys;
    data[6] = yz * nc + xs;
    data[kScaleZ] = z * z * nc + c;

    mType = kTypeUnknown;
}

void Matrix4x4::loadMultiply(const Matrix4x4& u, const Matrix4x4& v) {
    for (int i = 0; i < 4; i++) {
        float x = 0;
        float y = 0;
        float z = 0;
        float w = 0;

        for (int j = 0; j < 4; j++) {
            const float e = v.get(i, j);
            x += u.get(j, 0) * e;
            y += u.get(j, 1) * e;
            z += u.get(j, 2) * e;
            w += u.get(j, 3) * e;
        }

        set(i, 0, x);
        set(i, 1, y);
        set(i, 2, z);
        set(i, 3, w);
    }

    mType = kTypeUnknown;
}

void Matrix4x4::loadOrtho(float left, float right, float bottom, float top, float near, float far) {
    loadIdentity();

    data[kScaleX] = 2.0f / (right - left);
    data[kScaleY] = 2.0f / (top - bottom);
    data[kScaleZ] = -2.0f / (far - near);
    data[kTranslateX] = -(right + left) / (right - left);
    data[kTranslateY] = -(top + bottom) / (top - bottom);
    data[kTranslateZ] = -(far + near) / (far - near);

    mType = kTypeTranslate | kTypeScale | kTypeRectToRect;
}

void Matrix4x4::dump(const char* label) const {
    printf("%s[simple=%d, type=0x%x \n", label ? label : "Matrix4x4", isSimple(), getType());
    printf("  %f %f %f %f \n", data[kScaleX], data[kSkewX], data[8], data[kTranslateX]);
    printf("  %f %f %f %f \n", data[kSkewY], data[kScaleY], data[9], data[kTranslateY]);
    printf("  %f %f %f %f \n", data[2], data[6], data[kScaleZ], data[kTranslateZ]);
    printf("  %f %f %f %f \n", data[kPerspective0], data[kPerspective1], data[11], data[kPerspective2]);
    printf("] \n");
}