#include <stdlib.h> // Deve vir antes da GLUT para evitar erros de redefinição no Windows

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

using namespace tinyxml2;

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

std::vector<Model> gModels;
Group gRootGroup;
int gWinW, gWinH;

struct Camera {
    Vec3 pos, lookAt, up;
    float fov, nearp, farp;
} gCam;

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

        if (name == "transform") {
            for (XMLElement* t = child->FirstChildElement(); t; t = t->NextSiblingElement()) {
                Transformation trans;
                trans.type = t->Name();
                trans.x = t->FloatAttribute("x", 0.0f);
                trans.y = t->FloatAttribute("y", 0.0f);
                trans.z = t->FloatAttribute("z", 0.0f);
                trans.angle = t->FloatAttribute("angle", 0.0f);
                g.transforms.push_back(trans);
            }
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

void renderGroup(const Group& g) {
    glPushMatrix();

    for (const auto& t : g.transforms) {
        if (t.type == "translate") glTranslatef(t.x, t.y, t.z);
        else if (t.type == "rotate") glRotatef(t.angle, t.x, t.y, t.z);
        else if (t.type == "scale") glScalef(t.x, t.y, t.z);
    }

    glBegin(GL_TRIANGLES);
    for (int idx : g.modelIndices) {
        for (size_t i = 0; i < gModels[idx].vertices.size(); i += 3) {
            glVertex3f(gModels[idx].vertices[i], gModels[idx].vertices[i+1], gModels[idx].vertices[i+2]);
        }
    }
    glEnd();

    for (const auto& childGroup : g.children) {
        renderGroup(childGroup);
    }

    glPopMatrix();
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(gCam.pos.x, gCam.pos.y, gCam.pos.z, 
              gCam.lookAt.x, gCam.lookAt.y, gCam.lookAt.z, 
              gCam.up.x, gCam.up.y, gCam.up.z);

    // Desenhar eixos para orientação (Positivo e Negativo)
    glBegin(GL_LINES);
    
    // Eixo X (Vermelho)
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-100.0f, 0.0f, 0.0f);
    glVertex3f( 100.0f, 0.0f, 0.0f);
    
    // Eixo Y (Verde)
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, -100.0f, 0.0f);
    glVertex3f(0.0f,  100.0f, 0.0f);
    
    // Eixo Z (Azul)
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, -100.0f);
    glVertex3f(0.0f, 0.0f,  100.0f);
    
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f); // Reset para branco para os modelos

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
    if (argc < 2) {
        std::cout << "Uso: engine <ficheiro.xml>" << std::endl;
        return 1;
    }

    XMLDocument doc;
    if (doc.LoadFile(argv[1]) != XML_SUCCESS) {
        std::cerr << "Erro ao carregar o ficheiro XML: " << argv[1] << std::endl;
        return 1;
    }

    XMLElement* root = doc.FirstChildElement("world");
    XMLElement* win = root->FirstChildElement("window");
    gWinW = win->IntAttribute("width", 800);
    gWinH = win->IntAttribute("height", 600);

    XMLElement* cam = root->FirstChildElement("camera");
    XMLElement* pos = cam->FirstChildElement("position");
    gCam.pos = { pos->FloatAttribute("x"), pos->FloatAttribute("y"), pos->FloatAttribute("z") };
    XMLElement* la = cam->FirstChildElement("lookAt");
    gCam.lookAt = { la->FloatAttribute("x"), la->FloatAttribute("y"), la->FloatAttribute("z") };
    XMLElement* up = cam->FirstChildElement("up");
    gCam.up = { up->FloatAttribute("x", 0), up->FloatAttribute("y", 1), up->FloatAttribute("z", 0) };
    XMLElement* proj = cam->FirstChildElement("projection");
    gCam.fov = proj->FloatAttribute("fov", 45);
    gCam.nearp = proj->FloatAttribute("near", 1);
    gCam.farp = proj->FloatAttribute("far", 1000);

    XMLElement* group = root->FirstChildElement("group");
    if (group) parseGroup(group, gRootGroup);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(gWinW, gWinH);
    glutCreateWindow("CG Engine - Fase 2");

    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glutDisplayFunc(renderScene);
    glutReshapeFunc(changeSize);
    glutMainLoop();

    return 0;
}