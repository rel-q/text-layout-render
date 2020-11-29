//
// Created by bq on 2019-08-20.
//


#pragma once

#include <memory>
#include <fontconfig/fontconfig.h>

template<typename T, size_t N>
char (& ArrayCountHelper(T (& array)[N]))[N];

#define ARRAY_COUNT(array) (sizeof(ArrayCountHelper(array)))

template<typename R, typename T, R (* P)(T*)>
struct FunctionWrapper {
    R operator()(T* t) { return P(t); }
};

template<typename T, T* (* C)(), void (* D)(T*)>
class AutoFc : public std::unique_ptr<T, FunctionWrapper<void, T, D>> {
public:
    AutoFc() : std::unique_ptr<T, FunctionWrapper<void, T, D>>(C()) {
    }

    explicit AutoFc(T* obj) : std::unique_ptr<T, FunctionWrapper<void, T, D>>(obj) {}

    operator T*() const { return this->get(); }
};

typedef AutoFc<FcCharSet, FcCharSetCreate, FcCharSetDestroy> AutoFcCharSet;
typedef AutoFc<FcConfig, FcConfigCreate, FcConfigDestroy> AutoFcConfig;
typedef AutoFc<FcFontSet, FcFontSetCreate, FcFontSetDestroy> AutoFcFontSet;
typedef AutoFc<FcLangSet, FcLangSetCreate, FcLangSetDestroy> AutoFcLangSet;
typedef AutoFc<FcObjectSet, FcObjectSetCreate, FcObjectSetDestroy> AutoFcObjectSet;
typedef AutoFc<FcPattern, FcPatternCreate, FcPatternDestroy> AutoFcPattern;


const char* get_string(FcPattern* pattern, const char object[], const char* missing = "");

int get_int(FcPattern* pattern, const char object[], int missing);
