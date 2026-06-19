/* Material.h - Material de superfície + parser de arquivos .mtl (Wavefront)
 *
 * Lê os COEFICIENTES DE REFLEXÃO do arquivo .mtl:
 *   Ka -> reflexão ambiente
 *   Kd -> reflexão difusa
 *   Ks -> reflexão especular
 *   Ns -> expoente especular (shininess)
 *   map_Kd -> arquivo de textura difusa
 *
 * Esses valores são depois enviados como uniforms ao Fragment Shader, onde
 * entram no cálculo do modelo de iluminação de Phong.
 */
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Material
{
    glm::vec3 Ka = glm::vec3(0.2f);          // ambiente
    glm::vec3 Kd = glm::vec3(0.8f);          // difusa
    glm::vec3 Ks = glm::vec3(0.5f);          // especular
    float     Ns = 32.0f;                    // expoente especular (shininess)

    bool      hasTexture = false;            // tem textura difusa?
    GLuint    texID      = 0;                // id da textura no OpenGL
    std::string mapKd;                       // nome do arquivo de textura (map_Kd)
};

// Lê um arquivo .mtl e preenche os coeficientes do material.
// Campos ausentes mantêm os defaults (ex.: Suzanne.mtl não traz Kd -> 0.8;
// Cube.mtl é vazio -> material neutro). Retorna false se o arquivo não abrir.
inline bool loadMTL(const std::string& filePath, Material& mat)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "[MTL] Nao foi possivel abrir: " << filePath << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "Ka")      ss >> mat.Ka.r >> mat.Ka.g >> mat.Ka.b;
        else if (tag == "Kd") ss >> mat.Kd.r >> mat.Kd.g >> mat.Kd.b;
        else if (tag == "Ks") ss >> mat.Ks.r >> mat.Ks.g >> mat.Ks.b;
        else if (tag == "Ns") ss >> mat.Ns;
        else if (tag == "map_Kd")
        {
            ss >> mat.mapKd;
            mat.hasTexture = true;
        }
    }

    std::cout << "[MTL] " << filePath
              << "  Ka(" << mat.Ka.r << "," << mat.Ka.g << "," << mat.Ka.b << ")"
              << "  Kd(" << mat.Kd.r << "," << mat.Kd.g << "," << mat.Kd.b << ")"
              << "  Ks(" << mat.Ks.r << "," << mat.Ks.g << "," << mat.Ks.b << ")"
              << "  Ns=" << mat.Ns
              << (mat.hasTexture ? ("  map_Kd=" + mat.mapKd) : "")
              << std::endl;
    return true;
}
