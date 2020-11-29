//
// Created by bq on 2019-08-20.
//

#include <memory>
#include <fontconfig/fontconfig.h>
#include "Util.h"

const char* get_string(FcPattern* pattern, const char object[], const char* missing) {
    FcChar8* value;
    if (FcPatternGetString(pattern, object, 0, &value) != FcResultMatch) {
        return missing;
    }
    return (const char*) value;
}

int get_int(FcPattern* pattern, const char object[], int missing) {
    int value;
    if (FcPatternGetInteger(pattern, object, 0, &value) != FcResultMatch) {
        return missing;
    }
    return value;
}