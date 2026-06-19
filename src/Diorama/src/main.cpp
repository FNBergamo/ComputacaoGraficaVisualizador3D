/* ============================================================================
 *  DIORAMA — Visualizador Final 3D
 *  Computação Gráfica / Unisinos
 *
 *  Integra TODO o pipeline da disciplina numa única cena:
 *    - Carregamento de malhas (.obj) e geometria procedural
 *    - Materiais lidos de .mtl (Ka/Kd/Ks/Ns) + mapeamento de textura
 *    - Iluminação de Phong com 3 fontes de luz (key / fill / back)
 *    - Câmera FPS navegável (teclado + mouse)
 *    - Seleção e transformação de objetos (translação, rotação, escala)
 *    - Animação de trajetória por curva de Bézier (play / pause)
 *
 *  ONDE ESTÁ CADA CÁLCULO (para a arguição):
 *    - Parser do arquivo de cena .............. loadScene()      (este arquivo)
 *    - Parser do material .mtl ................ loadMTL()        (Material.h)
 *    - Passagem de uniforms .................. desenhaObjeto() / loop principal
 *    - Matriz de Model ....................... montaModel()
 *    - Matriz de View ........................ Camera::getViewMatrix()  (Camera.h)
 *    - Cálculo de iluminação ................. Fragment Shader (abaixo)
 *    - Curva de Bézier ....................... evalClosedPath() (Bezier.h)
 * ==========================================================================*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Material.h"
// A implementação da stb_image deve existir em UMA única unidade de compilação.
// Mesh.h é quem inclui <stb_image.h>; definimos a macro só aqui, uma vez.
#define STB_IMAGE_IMPLEMENTATION
#include "Mesh.h"
#include "Bezier.h"

using namespace std;
using glm::vec3;
using glm::mat4;

// ─────────────────────────────────────────────────────────────
// Tipos da cena
// ─────────────────────────────────────────────────────────────

struct LightSrc
{
    string name = "luz";
    vec3   position  = vec3(0.0f);
    vec3   color     = vec3(1.0f);
    float  intensity = 1.0f;
    bool   enabled   = true;
};

struct SceneObject
{
    string   name;
    Mesh     mesh;
    Material material;

    vec3  basePosition = vec3(0.0f); // posição definida no arquivo de cena
    vec3  userTrans    = vec3(0.0f); // translação interativa (teclado)
    vec3  rotation     = vec3(0.0f); // rotação em graus (teclado)
    vec3  baseRotation = vec3(0.0f); // rotação fixa definida no arquivo de cena
    vec3  baseScale    = vec3(1.0f); // escala fixa definida no arquivo de cena
    float scaleUniform = 1.0f;       // escala uniforme
    bool  spin         = false;      // giro automático (Triforce)

    // Trajetória de Bézier
    vector<vec3> waypoints;
    bool  animated  = false;
    float t         = 0.0f;
    float speed     = 0.15f;          // fração da volta por segundo
    vec3  bezierPos = vec3(0.0f);
};

// ─────────────────────────────────────────────────────────────
// Globais
// ─────────────────────────────────────────────────────────────

int WIDTH = 1200, HEIGHT = 800;

Camera camera(vec3(0.0f, 3.0f, 12.0f));
float  lastX = WIDTH / 2.0f, lastY = HEIGHT / 2.0f;
bool   firstMouse = true;

float deltaTime = 0.0f, lastFrame = 0.0f;

vector<SceneObject> objects;
LightSrc            lights[3];
int  selectedObject = 0;
int  activeLight     = 0;     // luz cujo [ ] ajusta a intensidade
bool useTexture      = true;  // M alterna textura <-> cor do material
bool animating       = true;  // ESPAÇO dá play/pause na Bézier

Mesh   lightMarker;           // marcador visual das fontes de luz
GLuint shaderID = 0;

// ─────────────────────────────────────────────────────────────
// Shaders (raw string literals — fáceis de apontar na IDE)
// ─────────────────────────────────────────────────────────────

const char* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragPos;
out vec3 normal;
out vec2 texCoord;

void main()
{
    vec4 world = model * vec4(aPos, 1.0);
    fragPos    = world.xyz;
    // Normal no espaço de mundo (corrige escala não uniforme)
    normal     = mat3(transpose(inverse(model))) * aNormal;
    texCoord   = aUV;
    gl_Position = projection * view * world;
}
)";

const char* fragmentShaderSource = R"(
#version 400
in vec3 fragPos;
in vec3 normal;
in vec2 texCoord;

out vec4 fragColor;

struct Light {
    vec3  position;
    vec3  color;
    float intensity;
    int   enabled;
};
uniform Light lights[3];

// Coeficientes de reflexão lidos do .mtl
uniform vec3  Ka;
uniform vec3  Kd;
uniform vec3  Ks;
uniform float shininess;

uniform vec3  viewPos;
uniform int   useTexture;   // 1 = usa textura ; 0 = usa cor do material (Kd)
uniform sampler2D texBuff;
uniform float selected;     // realce do objeto selecionado
uniform int   unlit;        // 1 = cor cheia sem iluminação (marcador de luz)

void main()
{
    // Superfície base: textura OU branco (deixando Kd dar a cor do material)
    vec3 surface = (useTexture == 1) ? texture(texBuff, texCoord).rgb : vec3(1.0);

    if (unlit == 1) {
        fragColor = vec4(Kd, 1.0);
        return;
    }

    vec3 N = normalize(normal);
    vec3 V = normalize(viewPos - fragPos);

    // Termo ambiente global
    vec3 ambient  = Ka * vec3(0.25);
    vec3 diffuse  = vec3(0.0);
    vec3 specular = vec3(0.0);

    // MODELO DE PHONG: soma a contribuição das 3 luzes
    for (int i = 0; i < 3; ++i)
    {
        if (lights[i].enabled == 0) continue;

        vec3  L  = normalize(lights[i].position - fragPos);
        vec3  R  = reflect(-L, N);
        float df = max(dot(N, L), 0.0);
        float sp = pow(max(dot(V, R), 0.0), shininess);

        vec3 lc = lights[i].color * lights[i].intensity;
        diffuse  += Kd * df * lc;     // componente difusa
        specular += Ks * sp * lc;     // componente especular
    }

    vec3 color = (ambient + diffuse) * surface + specular;

    // Realce amarelado para o objeto selecionado
    color = mix(color, vec3(1.0, 1.0, 0.3), selected * 0.25);

    fragColor = vec4(color, 1.0);
}
)";

// ─────────────────────────────────────────────────────────────
// Protótipos
// ─────────────────────────────────────────────────────────────

GLuint setupShader();
void   key_callback(GLFWwindow*, int, int, int, int);
void   mouse_callback(GLFWwindow*, double, double);
void   framebuffer_callback(GLFWwindow*, int, int);
void   processInput(GLFWwindow*);
bool   loadScene(const string& path);
mat4   montaModel(const SceneObject& obj);
void   desenhaObjeto(SceneObject& obj, bool isSelected);
void   imprimeAjuda();

// ─────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────

int main()
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "Diorama - Visualizador 3D", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    cout << "Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL:   " << glGetString(GL_VERSION)  << endl;

    shaderID    = setupShader();
    lightMarker = makeLightMarker();

    // Carrega a cena do arquivo de configuração.
    const string sceneFile = "../src/Diorama/assets/cena.txt";
    if (!loadScene(sceneFile))
    {
        cout << "ERRO: nao foi possivel carregar a cena: " << sceneFile << endl;
        cout << "Execute o programa de dentro da pasta build/." << endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    imprimeAjuda();

    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        deltaTime = now - lastFrame;
        lastFrame = now;

        glfwPollEvents();
        processInput(window);

        // Atualiza as trajetórias de Bézier
        if (animating)
        {
            for (auto& obj : objects)
            {
                if (!obj.animated || obj.waypoints.size() < 2) continue;
                obj.t += obj.speed * deltaTime;
                obj.bezierPos = evalClosedPath(obj.waypoints, obj.t);
            }
        }

        glClearColor(0.07f, 0.10f, 0.16f, 1.0f); // céu à noite
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderID);

        // Matriz de projeção (perspectiva)
        mat4 projection = glm::perspective(glm::radians(50.0f),
                              (float)WIDTH / (float)HEIGHT, 0.1f, 200.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));

        // Matriz de visualização (câmera)
        mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"),
                           1, GL_FALSE, glm::value_ptr(view));

        // Posição da câmera (para o termo especular)
        glUniform3fv(glGetUniformLocation(shaderID, "viewPos"),
                     1, glm::value_ptr(camera.position));

        // Envia as 3 luzes como uniforms
        for (int i = 0; i < 3; ++i)
        {
            string b = "lights[" + to_string(i) + "].";
            glUniform3fv(glGetUniformLocation(shaderID, (b + "position").c_str()),
                         1, glm::value_ptr(lights[i].position));
            glUniform3fv(glGetUniformLocation(shaderID, (b + "color").c_str()),
                         1, glm::value_ptr(lights[i].color));
            glUniform1f(glGetUniformLocation(shaderID, (b + "intensity").c_str()),
                        lights[i].intensity);
            glUniform1i(glGetUniformLocation(shaderID, (b + "enabled").c_str()),
                        lights[i].enabled ? 1 : 0);
        }

        glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);

        // Desenha todos os objetos da cena
        for (int i = 0; i < (int)objects.size(); ++i)
            desenhaObjeto(objects[i], i == selectedObject);

        // Desenha os marcadores das fontes de luz (sem iluminação)
        glUniform1i(glGetUniformLocation(shaderID, "unlit"), 1);
        glUniform1i(glGetUniformLocation(shaderID, "useTexture"), 0);
        glUniform1f(glGetUniformLocation(shaderID, "selected"), 0.0f);
        for (int i = 0; i < 3; ++i)
        {
            if (!lights[i].enabled) continue;
            glUniform3fv(glGetUniformLocation(shaderID, "Kd"),
                         1, glm::value_ptr(lights[i].color));
            mat4 m = glm::translate(mat4(1.0f), lights[i].position);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"),
                               1, GL_FALSE, glm::value_ptr(m));
            glBindVertexArray(lightMarker.vao);
            glDrawArrays(GL_TRIANGLES, 0, lightMarker.nVertices);
        }
        glUniform1i(glGetUniformLocation(shaderID, "unlit"), 0);

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

// ─────────────────────────────────────────────────────────────
// Monta a MATRIZ DE MODEL do objeto (Translação * Rotação * Escala)
// ─────────────────────────────────────────────────────────────

mat4 montaModel(const SceneObject& obj)
{
    // Posição efetiva: trajetória de Bézier (se animado) + ajuste do usuário
    vec3 pos = (obj.animated ? obj.bezierPos : obj.basePosition) + obj.userTrans;

    vec3 rot = obj.baseRotation + obj.rotation;
    if (obj.spin) rot.y += (float)glfwGetTime() * 40.0f; // giro automático

    mat4 model = mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, glm::radians(rot.z), vec3(0, 0, 1));
    model = glm::rotate(model, glm::radians(rot.y), vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rot.x), vec3(1, 0, 0));
    model = glm::scale(model, obj.baseScale * vec3(obj.scaleUniform));
    return model;
}

// ─────────────────────────────────────────────────────────────
// Desenha um objeto: envia material + matriz de model e faz o draw
// ─────────────────────────────────────────────────────────────

void desenhaObjeto(SceneObject& obj, bool isSelected)
{
    if (obj.mesh.vao == 0) return;

    // Coeficientes do material (uniforms usados no Fragment Shader)
    glUniform3fv(glGetUniformLocation(shaderID, "Ka"), 1, glm::value_ptr(obj.material.Ka));
    glUniform3fv(glGetUniformLocation(shaderID, "Kd"), 1, glm::value_ptr(obj.material.Kd));
    glUniform3fv(glGetUniformLocation(shaderID, "Ks"), 1, glm::value_ptr(obj.material.Ks));
    glUniform1f (glGetUniformLocation(shaderID, "shininess"), obj.material.Ns);

    // Só usa textura se o material tiver E o modo global estiver ligado
    int texOn = (useTexture && obj.material.hasTexture) ? 1 : 0;
    glUniform1i(glGetUniformLocation(shaderID, "useTexture"), texOn);
    glUniform1f(glGetUniformLocation(shaderID, "selected"), isSelected ? 1.0f : 0.0f);
    glUniform1i(glGetUniformLocation(shaderID, "unlit"), 0);

    if (texOn)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, obj.material.texID);
    }

    mat4 model = montaModel(obj);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"),
                       1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(obj.mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, obj.mesh.nVertices);
}

// ─────────────────────────────────────────────────────────────
// Parser do arquivo de cena
// ─────────────────────────────────────────────────────────────
//  Formato:
//    light  <nome> on|off  px py pz   r g b   intensidade
//    object <nome> <geom>  <mtl|-|COL:r,g,b>  <tex|->  px py pz  escala  [spin] [scale sx sy sz] [rot rx ry rz]
//    waypoint x y z     (anexa ponto de trajetória ao último objeto)
// ─────────────────────────────────────────────────────────────

static void aplicaMtlField(const string& field, Material& mat)
{
    if (field == "-") return;
    if (field.rfind("COL:", 0) == 0)
    {
        // cor literal: COL:r,g,b  -> vira a cor difusa do material
        string nums = field.substr(4);
        for (char& c : nums) if (c == ',') c = ' ';
        istringstream cs(nums);
        vec3 col(0.8f);
        cs >> col.r >> col.g >> col.b;
        mat.Kd = col;
        mat.Ka = col * 0.6f;
        mat.Ks = vec3(0.4f);
        return;
    }
    loadMTL(field, mat); // caminho .mtl
}

bool loadScene(const string& path)
{
    ifstream file(path);
    if (!file.is_open()) return false;

    objects.clear();
    int lightCount = 0;

    string line;
    while (getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;
        istringstream ss(line);
        string tag;
        ss >> tag;

        if (tag == "light")
        {
            if (lightCount >= 3) continue;
            LightSrc& l = lights[lightCount++];
            string onoff;
            ss >> l.name >> onoff
               >> l.position.x >> l.position.y >> l.position.z
               >> l.color.r >> l.color.g >> l.color.b
               >> l.intensity;
            l.enabled = (onoff == "on" || onoff == "1");
        }
        else if (tag == "object")
        {
            SceneObject obj;
            string geom, mtl, tex;
            ss >> obj.name >> geom >> mtl >> tex
               >> obj.basePosition.x >> obj.basePosition.y >> obj.basePosition.z
               >> obj.scaleUniform;

            string extra;
            int rotIndex = 0;
            while (ss >> extra)
            {
                if (extra == "spin")
                {
                    obj.spin = true;
                    continue;
                }
                if (extra == "scale")
                {
                    ss >> obj.baseScale.x >> obj.baseScale.y >> obj.baseScale.z;
                    continue;
                }
                if (extra == "rot")
                {
                    ss >> obj.baseRotation.x >> obj.baseRotation.y >> obj.baseRotation.z;
                    continue;
                }

                float value = stof(extra);
                if (rotIndex == 0) obj.baseRotation.x = value;
                else if (rotIndex == 1) obj.baseRotation.y = value;
                else if (rotIndex == 2) obj.baseRotation.z = value;
                ++rotIndex;
            }

            obj.mesh = loadGeometry(geom);
            aplicaMtlField(mtl, obj.material);

            if (tex != "-")
            {
                obj.material.texID      = loadTexture(tex);
                obj.material.hasTexture = true;
            }
            objects.push_back(obj);
        }
        else if (tag == "waypoint" && !objects.empty())
        {
            vec3 p;
            ss >> p.x >> p.y >> p.z;
            objects.back().waypoints.push_back(p);
        }
    }

    // Marca como animado quem tiver pelo menos 2 waypoints
    for (auto& obj : objects)
        if (obj.waypoints.size() >= 2)
        {
            obj.animated  = true;
            obj.bezierPos = obj.waypoints[0];
        }

    cout << "[CENA] " << objects.size() << " objetos, "
         << lightCount << " luzes." << endl;
    return !objects.empty();
}

// ─────────────────────────────────────────────────────────────
// Entrada contínua (teclas seguradas)
// ─────────────────────────────────────────────────────────────

void processInput(GLFWwindow* window)
{
    // Câmera FPS
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.move(0, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.move(1, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.move(2, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.move(3, deltaTime);

    if (objects.empty()) return;
    SceneObject& obj = objects[selectedObject];

    float tv = 3.0f * deltaTime;   // velocidade de translação
    float rv = 90.0f * deltaTime;  // velocidade de rotação (graus/s)
    float sv = 1.0f * deltaTime;   // velocidade de escala

    // Translação do objeto selecionado (J/L = X, I/K = Z, U/O = Y)
bool moved = false;
if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) { obj.userTrans.x -= tv; moved = true; }
if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) { obj.userTrans.x += tv; moved = true; }
if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) { obj.userTrans.z -= tv; moved = true; }
if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) { obj.userTrans.z += tv; moved = true; }
if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) obj.userTrans.y += tv;
if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) obj.userTrans.y -= tv;

// Imprime a posição efetiva do objeto sempre que ele é movido com I/J/K/L
// (throttle de 0.1s para não inundar o console enquanto a tecla fica segurada)
if (moved)
{
    static float lastPrint = 0.0f;
    float now = (float)glfwGetTime();
    if (now - lastPrint > 0.1f)
    {
        vec3 pos = obj.basePosition + obj.userTrans; // posição final no mundo
        cout << "[" << obj.name << "] pos = ("
             << pos.x << ", " << pos.y << ", " << pos.z << ")" << endl;
        lastPrint = now;
    }
}
    // Rotação em Y (R / T)
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) obj.rotation.y -= rv;
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) obj.rotation.y += rv;

    // Escala uniforme (+ / -)
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) obj.scaleUniform += sv;
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        obj.scaleUniform = glm::max(0.05f, obj.scaleUniform - sv);

    // Intensidade da luz ativa ( [ diminui  ] aumenta )
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        lights[activeLight].intensity = glm::max(0.0f, lights[activeLight].intensity - sv);
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        lights[activeLight].intensity += sv;
}

// ─────────────────────────────────────────────────────────────
// Entrada discreta (teclas pressionadas uma vez)
// ─────────────────────────────────────────────────────────────

void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GL_TRUE);

    // Seleção de objeto
    if (key == GLFW_KEY_TAB && !objects.empty())
    {
        selectedObject = (selectedObject + 1) % (int)objects.size();
        cout << "Selecionado: [" << selectedObject << "] "
             << objects[selectedObject].name << endl;
    }
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
    {
        int idx = key - GLFW_KEY_1;
        if (idx < (int)objects.size())
        {
            selectedObject = idx;
            cout << "Selecionado: [" << idx << "] " << objects[idx].name << endl;
        }
    }

    // Reset das transformações do objeto selecionado
    if (key == GLFW_KEY_0 && !objects.empty())
    {
        SceneObject& o = objects[selectedObject];
        o.userTrans = vec3(0.0f);
        o.rotation  = vec3(0.0f);
        o.baseScale    = vec3(1.0f);
        o.scaleUniform = 1.0f;
        cout << "Transformacoes resetadas." << endl;
    }

    // Liga/desliga cada luz
    if (key == GLFW_KEY_F1) { lights[0].enabled = !lights[0].enabled; cout << "Luz key  " << (lights[0].enabled?"ON":"OFF") << endl; }
    if (key == GLFW_KEY_F2) { lights[1].enabled = !lights[1].enabled; cout << "Luz fill " << (lights[1].enabled?"ON":"OFF") << endl; }
    if (key == GLFW_KEY_F3) { lights[2].enabled = !lights[2].enabled; cout << "Luz back " << (lights[2].enabled?"ON":"OFF") << endl; }

    // Luz ativa para ajuste de intensidade
    if (key == GLFW_KEY_G)
    {
        activeLight = (activeLight + 1) % 3;
        cout << "Luz ativa p/ intensidade: [" << activeLight << "] "
             << lights[activeLight].name << endl;
    }

    // Alterna material(.mtl) <-> textura
    if (key == GLFW_KEY_M)
    {
        useTexture = !useTexture;
        cout << "Exibicao: " << (useTexture ? "TEXTURA" : "COR DO MATERIAL (.mtl)") << endl;
    }

    // Play / pause da animação de Bézier
    if (key == GLFW_KEY_SPACE)
    {
        animating = !animating;
        cout << "Animacao Bezier: " << (animating ? "PLAY" : "PAUSE") << endl;
    }
}

// ─────────────────────────────────────────────────────────────
// Callbacks de mouse / janela
// ─────────────────────────────────────────────────────────────

void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = (float)xpos; lastY = (float)ypos; firstMouse = false;
    }
    float xoff = (float)xpos - lastX;
    float yoff = lastY - (float)ypos;
    lastX = (float)xpos; lastY = (float)ypos;
    camera.rotate(xoff, yoff);
}

void framebuffer_callback(GLFWwindow*, int w, int h)
{
    WIDTH = w; HEIGHT = h;
    glViewport(0, 0, w, h);
}

// ─────────────────────────────────────────────────────────────
// Compilação dos shaders
// ─────────────────────────────────────────────────────────────

GLuint setupShader()
{
    GLint ok; GLchar log[512];

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, 512, NULL, log); cout << "VERTEX ERRO:\n" << log << endl; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, 512, NULL, log); cout << "FRAGMENT ERRO:\n" << log << endl; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, 512, NULL, log); cout << "LINK ERRO:\n" << log << endl; }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ─────────────────────────────────────────────────────────────
// Legenda de controles
// ─────────────────────────────────────────────────────────────

void imprimeAjuda()
{
    cout << "\n========== DIORAMA - CONTROLES ==========\n"
         << " Camera:      W A S D + mouse\n"
         << " Selecionar:  TAB (proximo) | 1-9 (indice)\n"
         << " Transladar:  J/L = X   I/K = Z   U/O = Y\n"
         << " Rotacionar:  R / T  (eixo Y)\n"
         << " Escala:      + / -  (uniforme)   | 0 = reset\n"
         << " Luzes:       F1/F2/F3 liga-desliga key/fill/back\n"
         << "              G = troca luz ativa | [ ] = intensidade\n"
         << " Material:    M = textura <-> cor do material (.mtl)\n"
         << " Animacao:    ESPACO = play/pause (Bezier)\n"
         << " Sair:        ESC\n"
         << "===============================================\n" << endl;
}
