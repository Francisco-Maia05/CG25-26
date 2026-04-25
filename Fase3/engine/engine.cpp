#include <cstdlib>
#define FREEGLUT_STATIC
#include <GL/glew.h>

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

// --- ESTRUTURAS ---
struct Vec3 { float x, y, z; };

struct Transformation {
    std::string type;
    float x, y, z, angle;
    float time;
    bool align;
    std::vector<Vec3> points;
};

struct Model {
    GLuint vbo_id;
    int vertexCount;
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

// --- VARIÁVEIS GLOBAIS ---
std::vector<Model> gModels;
Group gRootGroup;
Camera gCam;
float camAlpha = 0.0f, camBeta = 0.0f, speed = 5.0f;
const float PI = 3.14159265359f;

// --- MATEMÁTICA CATMULL-ROM ---
void getCatmullRomPoint(float t, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3* pos, Vec3* deriv) {
    float m[4][4] = { {-0.5f,  1.5f, -1.5f,  0.5f},
                      { 1.0f, -2.5f,  2.0f, -0.5f},
                      {-0.5f,  0.0f,  0.5f,  0.0f},
                      { 0.0f,  1.0f,  0.0f,  0.0f} };
    float px[4] = { p0.x, p1.x, p2.x, p3.x }, py[4] = { p0.y, p1.y, p2.y, p3.y }, pz[4] = { p0.z, p1.z, p2.z, p3.z };
    float T[4] = { t * t * t, t * t, t, 1 }, dT[4] = { 3 * t * t, 2 * t, 1, 0 };
    auto compute = [&](float* p, float* tvec) {
        float res = 0;
        for (int i = 0; i < 4; i++) { float a = 0; for (int j = 0; j < 4; j++) a += tvec[j] * m[j][i]; res += a * p[i]; }
        return res;
        };
    if (pos) { pos->x = compute(px, T); pos->y = compute(py, T); pos->z = compute(pz, T); }
    if (deriv) { deriv->x = compute(px, dT); deriv->y = compute(py, dT); deriv->z = compute(pz, dT); }
}

void getGlobalCatmullRomPoint(float gt, Vec3* pos, Vec3* deriv, const std::vector<Vec3>& points) {
    int size = points.size();
    float t = gt * size;
    int index = floor(t);
    t = t - index;
    getCatmullRomPoint(t, points[(index + size - 1) % size], points[index % size], points[(index + 1) % size], points[(index + 2) % size], pos, deriv);
}

// --- DESENHAR A TRAJETÓRIA ---
void renderCatmullRomCurve(const std::vector<Vec3>& points) {
    glBegin(GL_LINE_STRIP);
    for (float t = 0; t < 1.0f; t += 0.01f) {
        Vec3 pos;
        getGlobalCatmullRomPoint(t, &pos, NULL, points);
        glVertex3f(pos.x, pos.y, pos.z);
    }
    // Desenhar o último ponto para fechar a curva
    Vec3 pos;
    getGlobalCatmullRomPoint(1.0f, &pos, NULL, points);
    glVertex3f(pos.x, pos.y, pos.z);
    glEnd();
}

// --- CARREGAMENTO VBO ---
int loadModelVBO(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return -1;
    int n; file >> n;
    std::vector<float> vertices;
    float x, y, z;
    while (file >> x >> y >> z) { vertices.push_back(x); vertices.push_back(y); vertices.push_back(z); }
    Model m; m.vertexCount = n;
    glGenBuffers(1, &m.vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo_id);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    gModels.push_back(m);
    return gModels.size() - 1;
}

// --- PARSING XML ---
void parseGroup(XMLElement* element, Group& g) {
    for (XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
        std::string name = child->Name();
        if (name == "transform") parseGroup(child, g);
        else if (name == "translate" || name == "rotate" || name == "scale") {
            Transformation t;
            t.type = name;
            t.x = child->FloatAttribute("x", 0); t.y = child->FloatAttribute("y", 0); t.z = child->FloatAttribute("z", 0);
            t.angle = child->FloatAttribute("angle", 0); t.time = child->FloatAttribute("time", 0); t.align = child->BoolAttribute("align", false);
            if (name == "translate") {
                for (XMLElement* p = child->FirstChildElement("point"); p; p = p->NextSiblingElement("point"))
                    t.points.push_back({ p->FloatAttribute("x"), p->FloatAttribute("y"), p->FloatAttribute("z") });
            }
            g.transforms.push_back(t);
        }
        else if (name == "models") {
            for (XMLElement* m = child->FirstChildElement("model"); m; m = m->NextSiblingElement("model")) {
                int idx = loadModelVBO(m->Attribute("file"));
                if (idx != -1) g.modelIndices.push_back(idx);
            }
        }
        else if (name == "group") {
            Group childGroup;
            parseGroup(child, childGroup);
            g.children.push_back(childGroup);
        }
    }
}

// --- CÂMARA (Fase 2) ---
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

void initCameraAngles() {
    float dx = gCam.lookAt.x - gCam.pos.x;
    float dy = gCam.lookAt.y - gCam.pos.y;
    float dz = gCam.lookAt.z - gCam.pos.z;
    float r = sqrt(dx * dx + dy * dy + dz * dz);
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

// --- RENDER ---
void buildRotationMatrix(Vec3 z, Vec3 up, float* m) {
    Vec3 x; float nZ = sqrt(z.x * z.x + z.y * z.y + z.z * z.z); z.x /= nZ; z.y /= nZ; z.z /= nZ;
    x.x = up.y * z.z - up.z * z.y; x.y = up.z * z.x - up.x * z.z; x.z = up.x * z.y - up.y * z.x;
    float nX = sqrt(x.x * x.x + x.y * x.y + x.z * x.z); x.x /= nX; x.y /= nX; x.z /= nX;
    Vec3 y = { z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x };
    m[0] = x.x; m[1] = x.y; m[2] = x.z; m[4] = y.x; m[5] = y.y; m[6] = y.z; m[8] = z.x; m[9] = z.y; m[10] = z.z; m[15] = 1;
}

void renderGroup(const Group& g) {
    glPushMatrix();
    float time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    for (const auto& t : g.transforms) {
        if (t.type == "translate") {
            if (t.time > 0 && t.points.size() >= 4) {
                // Desenhar a curva em amarelo
                glDisable(GL_LIGHTING); // Garantir que a linha é visível
                glColor3f(1.0f, 1.0f, 0.0f);
                renderCatmullRomCurve(t.points);
                glColor3f(1.0f, 1.0f, 1.0f); // Reset para cor branca

                Vec3 pos, deriv;
                getGlobalCatmullRomPoint(fmod(time, t.time) / t.time, &pos, &deriv, t.points);
                glTranslatef(pos.x, pos.y, pos.z);
                if (t.align) { float m[16] = { 0 }; buildRotationMatrix(deriv, { 0, 1, 0 }, m); glMultMatrixf(m); }
            }
            else glTranslatef(t.x, t.y, t.z);
        }
        else if (t.type == "rotate") {
            float angle = (t.time > 0) ? (fmod(time, t.time) / t.time) * 360.0f : t.angle;
            glRotatef(angle, t.x, t.y, t.z);
        }
        else if (t.type == "scale") glScalef(t.x, t.y, t.z);
    }
    for (int idx : g.modelIndices) {
        glBindBuffer(GL_ARRAY_BUFFER, gModels[idx].vbo_id);
        glVertexPointer(3, GL_FLOAT, 0, 0);
        glEnableClientState(GL_VERTEX_ARRAY);
        glDrawArrays(GL_TRIANGLES, 0, gModels[idx].vertexCount);
    }
    for (const auto& child : g.children) renderGroup(child);
    glPopMatrix();
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(gCam.pos.x, gCam.pos.y, gCam.pos.z, gCam.lookAt.x, gCam.lookAt.y, gCam.lookAt.z, gCam.up.x, gCam.up.y, gCam.up.z);

    // EIXOS
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(-100, 0, 0); glVertex3f(100, 0, 0);
    glColor3f(0, 1, 0); glVertex3f(0, -100, 0); glVertex3f(0, 100, 0);
    glColor3f(0, 0, 1); glVertex3f(0, 0, -100); glVertex3f(0, 0, 100);
    glEnd();
    glColor3f(1, 1, 1);

    renderGroup(gRootGroup);
    glutSwapBuffers();
}

void changeSize(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(gCam.fov, (float)w / h, gCam.nearp, gCam.farp);
    glMatrixMode(GL_MODELVIEW);
}

// --- MAIN ---
int main(int argc, char** argv) {
    if (argc < 2) return 1;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("CG Engine - Fase 3 (Com Trajetória)");
    glewInit();

    XMLDocument doc;
    if (doc.LoadFile(argv[1]) == XML_SUCCESS) {
        XMLElement* world = doc.FirstChildElement("world");
        XMLElement* cam = world->FirstChildElement("camera");
        if (cam) {
            XMLElement* pos = cam->FirstChildElement("position");
            gCam.pos = { pos->FloatAttribute("x"), pos->FloatAttribute("y"), pos->FloatAttribute("z") };
            XMLElement* la = cam->FirstChildElement("lookAt");
            gCam.lookAt = { la->FloatAttribute("x"), la->FloatAttribute("y"), la->FloatAttribute("z") };
            XMLElement* up = cam->FirstChildElement("up");
            gCam.up = { up->FloatAttribute("x", 0), up->FloatAttribute("y", 1), up->FloatAttribute("z", 0) };
            XMLElement* proj = cam->FirstChildElement("projection");
            gCam.fov = proj->FloatAttribute("fov", 45); gCam.nearp = proj->FloatAttribute("near", 1); gCam.farp = proj->FloatAttribute("far", 1000);
        }
        initCameraAngles(); updateCameraPosition();
        parseGroup(world->FirstChildElement("group"), gRootGroup);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT, GL_LINE);

    glutDisplayFunc(renderScene);
    glutIdleFunc(renderScene);
    glutReshapeFunc(changeSize);
    glutKeyboardFunc(processKeys);
    glutSpecialFunc(processSpecialKeys);

    glutMainLoop();
    return 0;
}