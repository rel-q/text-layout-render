cmake_minimum_required(VERSION 3.13)
project(text-render)

set(CMAKE_CXX_STANDARD 14)

set(SKIA_PATH ~/work/github/skia)
set(LIB_DIR "third_party")

find_package(OpenGL REQUIRED)

include_directories(PRIVATE ${LIB_DIR} ${OPENGL_INCLUDE_DIRS} /usr/local/include ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)

# minikin
set(MINIKIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/minikin")
set(MINIKIN_SRC
        ${MINIKIN_DIR}/CmapCoverage.cpp
        ${MINIKIN_DIR}/Emoji.cpp
        ${MINIKIN_DIR}/FontCollection.cpp
        ${MINIKIN_DIR}/FontFamily.cpp
        ${MINIKIN_DIR}/FontLanguage.cpp
        ${MINIKIN_DIR}/FontLanguageListCache.cpp
        ${MINIKIN_DIR}/FontUtils.cpp
        ${MINIKIN_DIR}/GraphemeBreak.cpp
        ${MINIKIN_DIR}/HbFontCache.cpp
        ${MINIKIN_DIR}/Hyphenator.cpp
        ${MINIKIN_DIR}/Layout.cpp
        ${MINIKIN_DIR}/LayoutUtils.cpp
        ${MINIKIN_DIR}/LineBreaker.cpp
        ${MINIKIN_DIR}/Measurement.cpp
        ${MINIKIN_DIR}/MinikinFont.cpp
        ${MINIKIN_DIR}/MinikinInternal.cpp
        ${MINIKIN_DIR}/SparseBitSet.cpp
        ${MINIKIN_DIR}/WordBreaker.cpp
        )

# glfw
find_package(GLFW3 REQUIRED)
message(STATUS "Found GLFW3 in ${GLFW3_INCLUDE_DIRS}")
link_libraries("glfw" "${CMAKE_DL_LIBS}")

# glad
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glad")
add_library("glad" "${GLAD_DIR}/src/glad.c")
include_directories("glad" PRIVATE "${GLAD_DIR}/include")
include_directories(PRIVATE "${GLAD_DIR}/include")
link_libraries("glad" "${CMAKE_DL_LIBS}")

# stb
set(STB_DIR "${LIB_DIR}/stb")
include_directories(PRIVATE "${STB_DIR}")

set(SKIA_INCLUDES
        ${SKIA_PATH}/third_party/externals/harfbuzz/src
        ${SKIA_PATH}/third_party/externals/icu/source/common
        ${SKIA_PATH}/third_party/externals/icu/source/i18n
        )

include_directories(/usr/local/include /usr/local/include/freetype2 ${SKIA_INCLUDES})
link_directories(/usr/local/lib/ ${SKIA_PATH}/out/Debug)
link_libraries(fontconfig)
link_libraries(freetype)
link_libraries(
        harfbuzz
        harfbuzz-icu
        icu
)

add_executable(text-render
        ${MINIKIN_SRC}
        src/main.cpp
        src/paint_record.cc
        src/font_collection.cc
        src/platform.cc
        src/paragraph.cc
        src/paragraph_builder.cc
        src/paragraph_style.cc
        src/styled_runs.cc
        src/text_style.cc
        src/Typeface.cpp
        src/FontManager.cpp
        src/JenkinsHash.cpp
        src/CacheTexture.cpp
        src/PixelBuffer.cpp
        src/Texture.cpp
        src/Util.cpp
        src/Matrix4x4.cpp
        src/TextureState.cpp
        src/TextRenderer.cpp
        src/GLRenderer.cpp
        src/MeshState.cpp
        src/LayoutFont.cpp)