#pragma once

#include <vector>
#include <glm/glm.hpp>

// Fórmula da Bézier cúbica (forma de Bernstein).
inline glm::vec3 bezierCubic(const glm::vec3& P0, const glm::vec3& P1,
                             const glm::vec3& P2, const glm::vec3& P3, float t)
{
    float u  = 1.0f - t;
    float b0 = u * u * u;
    float b1 = 3.0f * u * u * t;
    float b2 = 3.0f * u * t * t;
    float b3 = t * t * t;
    return b0 * P0 + b1 * P1 + b2 * P2 + b3 * P3;
}

// Avalia uma trajetória FECHADA que passa por todos os waypoints.
// t global em [0,1) cobre a volta inteira. Cada segmento entre waypoint[i] e
// waypoint[i+1] é uma Bézier cúbica; P1 e P2 saem das tangentes de Catmull-Rom.
inline glm::vec3 evalClosedPath(const std::vector<glm::vec3>& wp, float t)
{
    int n = (int)wp.size();
    if (n == 0) return glm::vec3(0.0f);
    if (n == 1) return wp[0];

    t = t - std::floor(t);              // mantém em [0,1)
    float scaled = t * n;               // n segmentos
    int   i      = (int)scaled;         // segmento atual
    float lt     = scaled - i;          // parâmetro local [0,1)
    if (i >= n) i = n - 1;

    // Índices vizinhos com wrap-around (curva fechada).
    const glm::vec3& Pm1 = wp[(i - 1 + n) % n];
    const glm::vec3& P0  = wp[i % n];
    const glm::vec3& P1w = wp[(i + 1) % n];
    const glm::vec3& P2w = wp[(i + 2) % n];

    // Pontos de controle internos da Bézier a partir das tangentes Catmull-Rom.
    glm::vec3 c1 = P0  + (P1w - Pm1) / 6.0f;
    glm::vec3 c2 = P1w - (P2w - P0) / 6.0f;

    return bezierCubic(P0, c1, c2, P1w, lt);
}
