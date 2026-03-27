#include <stdlib.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

#include "tinyxml2.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>

using namespace tinyxml2;

// Estruturas de suporte
struct Vec3 { float x, y, z; };

struct Transformation {
    std::string type;
    float x, y, z, angle;
};

struct Model {
    std::vector<float> vertices;
};

struct Group {
    std::vector<Transformation> transforms;
    std::vector<int> modelIndices;
    std::vector<Group> children;
};

struct Camera {
    Vec3 pos, lookAt, up;
    float fov, nearp, farp;
};

// Variáveis Globais
std::vector<Model> gModels;
Group gRootGroup;
Camera gCam;
int gWinW, gWinH;

// Controlo de Câmara (Ângulos e Velocidade)
float camAlpha = 0.0f; 
float camBeta = 0.0f;  
float speed = 5.0f;
const float PI = 3.14159265359f;

// --- FUNÇÕES DE CARREGAMENTO E PARSING ---

int loadModel(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro: Nao foi possivel abrir o modelo " << filename << std::endl;
        return -1;
    }
    int n; file >> n;
    Model m;
    float x, y, z;
    while (file >> x >> y >> z) {
        m.vertices.push_back(x);
        m.vertices.push_back(y);
        m.vertices.push_back(z);
    }
    gModels.push_back(m);
    return (int)gModels.size() - 1;
}

void parseGroup(XMLElement* element, Group& g) {
    for (XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
        std::string name = child->Name();

        if (name == "translate" || name == "rotate" || name == "scale") {
            Transformation trans;
            trans.type = name;
            trans.x = child->FloatAttribute("x", 0.0f);
            trans.y = child->FloatAttribute("y", 0.0f);
            trans.z = child->FloatAttribute("z", 0.0f);
            trans.angle = child->FloatAttribute("angle", 0.0f);
            g.transforms.push_back(trans);
        }
        else if (name == "transform") {
            parseGroup(child, g); // Recursão para ler dentro de <transform>
        }
        else if (name == "models") {
            for (XMLElement* m = child->FirstChildElement("model"); m; m = m->NextSiblingElement("model")) {
                const char* filename = m->Attribute("file");
                if (filename) {
                    int idx = loadModel(filename);
                    if (idx != -1) g.modelIndices.push_back(idx);
                }
            }
        }
        else if (name == "group") {
            Group childGroup;
            parseGroup(child, childGroup);
            g.children.push_back(childGroup);
        }
    }
}

// --- LÓGICA DA CÂMARA LIVRE ---

void updateCameraPosition() {
    float alphaRad = camAlpha * PI / 180.0f;
    float betaRad = camBeta * PI / 180.0f;

    float dirX = cos(betaRad) * sin(alphaRad);
    float dirY = sin(betaRad);
    float dirZ = cos(betaRad) * cos(alphaRad);

    gCam.lookAt.x = gCam.pos.x + dirX;
    gCam.lookAt.y = gCam.pos.y + dirY;
    gCam.lookAt.z = gCam.pos.z + dirZ;
}

// Inicializa os ângulos Alpha e Beta com base no lookAt do XML
void initCameraAngles() {
    float dx = gCam.lookAt.x - gCam.pos.x;
    float dy = gCam.lookAt.y - gCam.pos.y;
    float dz = gCam.lookAt.z - gCam.pos.z;
    float r = sqrt(dx*dx + dy*dy + dz*dz);

    camAlpha = atan2(dx, dz) * 180.0f / PI;
    camBeta = asin(dy / r) * 180.0f / PI;
}

void processKeys(unsigned char key, int xx, int yy) {
    float alphaRad = camAlpha * PI / 180.0f;
    float dirX = sin(alphaRad);
    float dirZ = cos(alphaRad);

    switch (tolower(key)) {
        case 'w': gCam.pos.x += dirX * speed; gCam.pos.z += dirZ * speed; break;
        case 's': gCam.pos.x -= dirX * speed; gCam.pos.z -= dirZ * speed; break;
        case 'a': gCam.pos.x += dirZ * speed; gCam.pos.z -= dirX * speed; break;
        case 'd': gCam.pos.x -= dirZ * speed; gCam.pos.z += dirX * speed; break;
        case 'r': gCam.pos.y += speed; break;
        case 'f': gCam.pos.y -= speed; break;
    }
    updateCameraPosition();
    glutPostRedisplay();
}

void processSpecialKeys(int key, int xx, int yy) {
    switch (key) {
        case GLUT_KEY_LEFT:  camAlpha += 2.0f; break;
        case GLUT_KEY_RIGHT: camAlpha -= 2.0f; break;
        case GLUT_KEY_UP:    camBeta += 2.0f; if (camBeta > 89.0f) camBeta = 89.0f; break;
        case GLUT_KEY_DOWN:  camBeta -= 2.0f; if (camBeta < -89.0f) camBeta = -89.0f; break;
    }
    updateCameraPosition();
    glutPostRedisplay();
}

// --- RENDERIZAÇÃO ---

void renderGroup(const Group& g) {
    glPushMatrix();
    for (const auto& t : g.transforms) {
        if (t.type == "translate") glTranslatef(t.x, t.y, t.z);
        else if (t.type == "rotate") glRotatef(t.angle, t.x, t.y, t.z);
        else if (t.type == "scale") glScalef(t.x, t.y, t.z);
    }
    for (int idx : g.modelIndices) {
        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < gModels[idx].vertices.size(); i += 3) {
            glVertex3f(gModels[idx].vertices[i], gModels[idx].vertices[i+1], gModels[idx].vertices[i+2]);
        }
        glEnd();
    }
    for (const auto& childGroup : g.children) renderGroup(childGroup);
    glPopMatrix();
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(gCam.pos.x, gCam.pos.y, gCam.pos.z, 
              gCam.lookAt.x, gCam.lookAt.y, gCam.lookAt.z, 
              gCam.up.x, gCam.up.y, gCam.up.z);

    // Eixos para orientação
    glBegin(GL_LINES);
    glColor3f(1,0,0); glVertex3f(-1000,0,0); glVertex3f(1000,0,0);
    glColor3f(0,1,0); glVertex3f(0,-1000,0); glVertex3f(0,1000,0);
    glColor3f(0,0,1); glVertex3f(0,0,-1000); glVertex3f(0,0,1000);
    glEnd();

    glColor3f(1,1,1);
    renderGroup(gRootGroup);
    glutSwapBuffers();
}

void changeSize(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(gCam.fov, (float)w/h, gCam.nearp, gCam.farp);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Uso: engine <ficheiro.xml>" << std::endl; return 1; }

    XMLDocument doc;
    if (doc.LoadFile(argv[1]) != XML_SUCCESS) return 1;

    XMLElement* root = doc.FirstChildElement("world");
    if (!root) return 1;

    // Parsing da Janela
    XMLElement* win = root->FirstChildElement("window");
    gWinW = (win) ? win->IntAttribute("width", 800) : 800;
    gWinH = (win) ? win->IntAttribute("height", 600) : 600;

    // Parsing da Câmara
    XMLElement* cam = root->FirstChildElement("camera");
    if (cam) {
        XMLElement* pos = cam->FirstChildElement("position");
        if (pos) gCam.pos = { pos->FloatAttribute("x"), pos->FloatAttribute("y"), pos->FloatAttribute("z") };
        XMLElement* la = cam->FirstChildElement("lookAt");
        if (la) gCam.lookAt = { la->FloatAttribute("x"), la->FloatAttribute("y"), la->FloatAttribute("z") };
        XMLElement* up = cam->FirstChildElement("up");
        gCam.up = (up) ? Vec3{ up->FloatAttribute("x", 0), up->FloatAttribute("y", 1), up->FloatAttribute("z", 0) } : Vec3{0, 1, 0};
        XMLElement* proj = cam->FirstChildElement("projection");
        gCam.fov = (proj) ? proj->FloatAttribute("fov", 45) : 45;
        gCam.nearp = (proj) ? proj->FloatAttribute("near", 1) : 1;
        gCam.farp = (proj) ? proj->FloatAttribute("far", 1000) : 1000;
    }

    // Parsing dos Grupos
    for (XMLElement* group = root->FirstChildElement("group"); group; group = group->NextSiblingElement("group")) {
        parseGroup(group, gRootGroup);
    }

    // --- Sincronizar câmara livre com os valores do XML ---
    initCameraAngles();
    updateCameraPosition();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(gWinW, gWinH);
    glutCreateWindow("CG Engine - Fase 2");

    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glutDisplayFunc(renderScene);
    glutReshapeFunc(changeSize);
    glutKeyboardFunc(processKeys);
    glutSpecialFunc(processSpecialKeys);

    glutMainLoop();
    return 0;
}
