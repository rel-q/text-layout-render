//
// Created by bq on 2019-08-20.
//

#include <iostream>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <vector>
#include <unistd.h>

#include "FontStyle.h"
#include "Util.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Program.h"
#include "Matrix4x4.h"

#define STB_IMAGE_IMPLEMENTATION

#include <stb/stb_image.h>

#include "GLRenderer.h"
#include "paragraph_builder.h"
#include "paint_record.h"
#include "TextRenderer.h"

// settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 600;
std::string inputChars;

void window_pos_callback(GLFWwindow* window, int xpos, int ypos) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    printf("win size %d-%d \n", width, height);
    glViewport(0, 0, width, height);
}

void window_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void character_callback(GLFWwindow* window, unsigned int codepoint) {
    inputChars += (char) codepoint;
}


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
//    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "CANVAS", nullptr, nullptr);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwWindowHint(GLFW_SAMPLES, 16);
    glfwSetCharCallback(window, character_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetWindowPosCallback(window, window_pos_callback);
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int width = SCR_WIDTH, height = SCR_HEIGHT;

    int pixWidth, pixHeight;
    glfwGetFramebufferSize(window, &pixWidth, &pixHeight);

    Program program("../res/vs.glsl",
                    "../res/fs.glsl");

    program.use();
    program.setInt("ourTexture", 0);

    mat4 ortho;
    ortho.loadOrtho(width, height);

    GLRenderer renderer;

    TextRenderer tr(&renderer);

    double lastTime = glfwGetTime();
    double deltaTime = 0;
    std::shared_ptr<txt::FontCollection> fCollection = std::make_shared<txt::FontCollection>();
    // fCollection->DisableFontFallback();
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.f, 0.f, 0.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        program.use();

        mat4 transform;
        double time = glfwGetTime();
        transform.translate(50, 100);
        program.setMat4("projection", ortho);
        program.setMat4("transform", transform);
        deltaTime += time - lastTime;
        if (deltaTime >= 1) {
            deltaTime = 0;
            printf("fps %f \n", 1.0 / (time - lastTime));
        }

        txt::ParagraphStyle style;
        style.max_lines = 13;
        auto paragraphBuilder = std::make_unique<txt::ParagraphBuilder>(
                style, fCollection);

        txt::TextStyle ts;
        ts.font_size = 50;
        ts.word_spacing = 20;
        // ts.letter_spacing = 5;
        // ts.height = 1.5;
        // ts.font_families.emplace_back("Verdana");
        // ts.font_families.emplace_back("Helvetica");
        // ts.font_families.emplace_back("STHeiti");
        paragraphBuilder->AddText("好");
        ts.font_style = txt::FontItalic::italic;
        paragraphBuilder->PushStyle(ts);
        paragraphBuilder->AddText("Hello World ParagraphBuilder\n");
        paragraphBuilder->Pop();
        paragraphBuilder->AddText("好12345\n");
        // ts.font_size = 50;
        // ts.font_weight = txt::FontWeight::w100;
        // ts.font_style = txt::FontItalic::italic;
        // paragraphBuilder->PushStyle(ts);
        // paragraphBuilder->AddText("好12345\n");
        // paragraphBuilder->AddText(inputChars);

        auto paragraph = paragraphBuilder->Build();

        paragraph->Layout(500);
        paragraph->Paint(&tr, 10, 20);

        // fr.renderPosText("你\n好12345", "30px 苹方-简", 10, 10, 100);
        // fr.renderPosText("你\n好12345", "50px sans-serif", 10, 10);
        // fr.renderPosText("abcdefghijklmnopqrstuvwxyz", "50px sans-serif", 20, 350);

        program.setVec3("textColor", sin(time) + 1.0, 1.0, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
        lastTime = time;
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}
