#ifndef HUD_MENU_H
#define HUD_MENU_H

#include <vector>
#include <string>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stb_easy_font.h"

extern int WIDTH, HEIGHT;

inline const char* hudVertexShaderSrc = R"(
#version 400
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
uniform mat4 projection;
out vec4 vColor;
void main()
{
    gl_Position = projection * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

inline const char* hudFragmentShaderSrc = R"(
#version 400
in vec4 vColor;
out vec4 fragColor;
void main() { fragColor = vColor; }
)";

inline GLuint hudShaderID = 0, hudVAO = 0, hudVBO = 0, hudEBO = 0;
inline const int HUD_MAX_QUADS = 512;
inline bool showMenu = true; // tecla H alterna

inline GLuint setupHudShader()
{
    GLint ok; GLchar log[512];
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &hudVertexShaderSrc, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, 512, NULL, log); std::cout << "HUD VERTEX ERRO:\n" << log << std::endl; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &hudFragmentShaderSrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, 512, NULL, log); std::cout << "HUD FRAGMENT ERRO:\n" << log << std::endl; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, 512, NULL, log); std::cout << "HUD LINK ERRO:\n" << log << std::endl; }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

inline void setupHudBuffers()
{
    glGenVertexArrays(1, &hudVAO);
    glGenBuffers(1, &hudVBO);
    glGenBuffers(1, &hudEBO);

    glBindVertexArray(hudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);

    std::vector<unsigned int> indices(HUD_MAX_QUADS * 6);
    for (int i = 0; i < HUD_MAX_QUADS; i++)
    {
        unsigned int b = i * 4;
        indices[i*6+0]=b+0; indices[i*6+1]=b+1; indices[i*6+2]=b+2;
        indices[i*6+3]=b+2; indices[i*6+4]=b+3; indices[i*6+5]=b+0;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hudEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

inline void drawHudText(float x, float y, const std::string& text, float scale,
                         unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    static char buffer[99999];
    unsigned char color[4] = { r, g, b, a };

    int numQuads = stb_easy_font_print(0.0f, 0.0f, (char*)text.c_str(), color, buffer, sizeof(buffer));
    if (numQuads <= 0) return;
    if (numQuads > HUD_MAX_QUADS) numQuads = HUD_MAX_QUADS;

    int numVerts = numQuads * 4;
    for (int i = 0; i < numVerts; i++)
    {
        float* v = (float*)(buffer + i * 16);
        v[0] = v[0] * scale + x;
        v[1] = v[1] * scale + y;
    }

    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    glBufferData(GL_ARRAY_BUFFER, numVerts * 16, buffer, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, numQuads * 6, GL_UNSIGNED_INT, 0);
}

inline void drawHudPanel(float x, float y, float w, float h,
                          unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    struct V { float x,y,z; unsigned char c[4]; };
    V verts[4] = {
        { x,     y,     0, {r,g,b,a} },
        { x + w, y,     0, {r,g,b,a} },
        { x + w, y + h, 0, {r,g,b,a} },
        { x,     y + h, 0, {r,g,b,a} },
    };
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

inline const std::vector<std::string> menuLines = {
    "CAMERA       W A S D + mouse",
    "SELECIONAR   TAB | 1-9",
    "MOVER:",      
    "   J/L       esquerda/direita",
    "   I/K       tras/frente",
    "   U/O       baixo/cima",
    "ROTACIONAR:",  
    "   Z/X       eixo X",
    "   C/V       eixo Y",
    "   B/N       eixo Z",
    "ESCALA:",
    "   +/-      aumenta/diminui a escala",
    "   0        reverte para a escala original",
    "LUZES:",
    "   F1/F2/F3 liga/desliga luzes",
    "   F4       troca luz ativa",
    "   F5/F6    diminui/aumenta a intensidade",
    "T           desativa/ativa textura",
    "ESPACO      pause/inicia animacao Bezier",
    "H           esconde/mostra menu",
    "ESC         fecha o diorama"
};

inline void drawMenuOverlay()
{
    if (!showMenu) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hudShaderID);
    glm::mat4 proj = glm::ortho(0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(hudShaderID, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glBindVertexArray(hudVAO);

    const float scale = 1.4f;
    const float lineH  = 13.0f * scale;
    const float padX = 12, padY = 10;
    const float panelW = 360;
    const float panelH = menuLines.size() * lineH + padY * 2;

    drawHudPanel(0, 0, panelW, panelH, 15, 18, 28, 190);

    float cy = 0 + padY;
    for (auto& line : menuLines)
    {
        drawHudText(10 + padX, cy, line, scale, 255, 255, 255, 255);
        cy += lineH;
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}

#endif