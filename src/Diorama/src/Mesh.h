#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <stb_image.h>

struct Mesh
{
    GLuint vao       = 0;
    int    nVertices = 0;
};

inline void pushVertex(std::vector<float>& buf,
                       const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv)
{
    buf.push_back(p.x); buf.push_back(p.y); buf.push_back(p.z);
    buf.push_back(n.x); buf.push_back(n.y); buf.push_back(n.z);
    buf.push_back(uv.x); buf.push_back(uv.y);
}

// Cria o VAO/VBO a partir de um buffer intercalado de 8 floats por vértice.
inline Mesh createMesh(const std::vector<float>& buf)
{
    Mesh mesh;
    mesh.nVertices = (int)(buf.size() / 8);

    GLuint VBO;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &VBO);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);

    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return mesh;
}

inline Mesh loadSimpleOBJ(const std::string& filePath)
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<float>     vBuffer;

    std::ifstream arq(filePath.c_str());
    if (!arq.is_open())
    {
        std::cerr << "[OBJ] Erro ao abrir: " << filePath << std::endl;
        return Mesh{};
    }

    std::string line;
    while (std::getline(arq, line))
    {
        std::istringstream ss(line);
        std::string word;
        ss >> word;

        if (word == "v")
        {
            glm::vec3 v; ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (word == "vt")
        {
            glm::vec2 vt; ss >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 vn; ss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (word == "f")
        {
            std::vector<int> vi, ti, ni;
            std::string token;
            while (ss >> token)
            {
                int iv = 0, it = 0, in = 0;
                std::istringstream fs(token);
                std::string idx;
                if (std::getline(fs, idx, '/')) iv = !idx.empty() ? std::stoi(idx) - 1 : -1;
                if (std::getline(fs, idx, '/')) it = !idx.empty() ? std::stoi(idx) - 1 : -1;
                if (std::getline(fs, idx))      in = !idx.empty() ? std::stoi(idx) - 1 : -1;
                vi.push_back(iv); ti.push_back(it); ni.push_back(in);
            }

            for (size_t k = 1; k + 1 < vi.size(); ++k)
            {
                int tri[3] = { (int)0, (int)k, (int)(k + 1) };

                glm::vec3 faceN(0.0f);
                if (ni[tri[0]] < 0 || normals.empty())
                {
                    glm::vec3 a = positions[vi[tri[0]]];
                    glm::vec3 b = positions[vi[tri[1]]];
                    glm::vec3 c = positions[vi[tri[2]]];
                    faceN = glm::normalize(glm::cross(b - a, c - a));
                }

                for (int j = 0; j < 3; ++j)
                {
                    int t = tri[j];
                    glm::vec3 p = positions[vi[t]];
                    glm::vec3 n = (ni[t] >= 0 && !normals.empty()) ? normals[ni[t]] : faceN;
                    glm::vec2 uv = (ti[t] >= 0 && !texCoords.empty()) ? texCoords[ti[t]] : glm::vec2(0.0f);
                    pushVertex(vBuffer, p, n, uv);
                }
            }
        }
    }
    arq.close();

    Mesh mesh = createMesh(vBuffer);
    std::cout << "[OBJ] " << filePath << " (" << mesh.nVertices << " vertices)" << std::endl;
    return mesh;
}

inline GLuint loadTexture(const std::string& filePath)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &ch, 0);
    if (data)
    {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "[TEX] " << filePath << " (" << w << "x" << h << ")" << std::endl;
    }
    else
    {
        std::cout << "[TEX] Falha ao carregar: " << filePath << std::endl;
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

// Esfera UV (base: inicializaEsfera de CuboIluminacao.cpp), agora com uv.
inline Mesh makeSphere(float radius = 0.5f, int stacks = 24, int sectors = 24)
{
    std::vector<float> buf;
    const float PI = 3.14159265359f;
    auto vtx = [&](float phi, float theta)
    {
        float x = radius * sin(phi) * cos(theta);
        float y = radius * cos(phi);
        float z = radius * sin(phi) * sin(theta);
        glm::vec3 p(x, y, z);
        glm::vec3 n = glm::normalize(p);
        glm::vec2 uv(theta / (2 * PI), 1.0f - phi / PI);
        pushVertex(buf, p, n, uv);
    };
    for (int i = 0; i < stacks; ++i)
    {
        float phi1 = PI * float(i) / stacks;
        float phi2 = PI * float(i + 1) / stacks;
        for (int j = 0; j < sectors; ++j)
        {
            float t1 = 2.0f * PI * float(j) / sectors;
            float t2 = 2.0f * PI * float(j + 1) / sectors;
            vtx(phi1, t1); vtx(phi2, t1); vtx(phi1, t2);
            vtx(phi1, t2); vtx(phi2, t1); vtx(phi2, t2);
        }
    }
    return createMesh(buf);
}

// Pequena esfera/cubo para marcar visualmente uma fonte de luz.
inline Mesh makeLightMarker() { return makeSphere(0.15f, 10, 10); };