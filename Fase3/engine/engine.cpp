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

struct Vec3 {
    float x, y, z;
};

struct Transformation {
    std::string type;
    float x = 0, y = 0, z = 0;
    float angle = 0;
    float time = 0;
    bool align = false;
    std::vector<Vec3> points;
};

struct Model {
    GLuint vbo_id = 0;
    int vertexCount = 0;
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

std::vector<Model> gModels;
Group gRootGroup;
Camera gCam;

float camAlpha = 0.0f;
float camBeta = 0.0f;
float speed = 5.0f;

const float PI = 3.14159265359f;

// ------------------------------------------------------
// Vetores auxiliares
// ------------------------------------------------------

Vec3 normalize(Vec3 v) {
    float l = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (l == 0) return {0, 0, 0};
    return {v.x / l, v.y / l, v.z / l};
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// ------------------------------------------------------
// Catmull-Rom
// ------------------------------------------------------

void getCatmullRomPoint(float t, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3* pos, Vec3* deriv) {
    float m[4][4] = {
        {-0.5f,  1.5f, -1.5f,  0.5f},
        { 1.0f, -2.5f,  2.0f, -0.5f},
        {-0.5f,  0.0f,  0.5f,  0.0f},
        { 0.0f,  1.0f,  0.0f,  0.0f}
    };

    float px[4] = {p0.x, p1.x, p2.x, p3.x};
    float py[4] = {p0.y, p1.y, p2.y, p3.y};
    float pz[4] = {p0.z, p1.z, p2.z, p3.z};

    float T[4]  = {t * t * t, t * t, t, 1};
    float dT[4] = {3 * t * t, 2 * t, 1, 0};

    auto compute = [&](float* p, float* tv) {
        float res = 0;
        for (int i = 0; i < 4; i++) {
            float a = 0;
            for (int j = 0; j < 4; j++) {
                a += tv[j] * m[j][i];
            }
            res += a * p[i];
        }
        return res;
    };

    if (pos) {
        pos->x = compute(px, T);
        pos->y = compute(py, T);
        pos->z = compute(pz, T);
    }

    if (deriv) {
        deriv->x = compute(px, dT);
        deriv->y = compute(py, dT);
        deriv->z = compute(pz, dT);
    }
}

void getGlobalCatmullRomPoint(float gt, Vec3* pos, Vec3* deriv, const std::vector<Vec3>& points) {
    int size = points.size();

    float t = gt * size;
    int index = floor(t);
    t = t - index;

    int indices[4];
    indices[0] = (index + size - 1) % size;
    indices[1] = index % size;
    indices[2] = (index + 1) % size;
    indices[3] = (index + 2) % size;

    getCatmullRomPoint(
        t,
        points[indices[0]],
        points[indices[1]],
        points[indices[2]],
        points[indices[3]],
        pos,
        deriv
    );
}

void renderCatmullRomCurve(const std::vector<Vec3>& points) {
    glColor3f(1.0f, 1.0f, 0.0f);

    glBegin(GL_LINE_LOOP);
    for (float t = 0; t < 1.0f; t += 0.01f) {
        Vec3 pos;
        getGlobalCatmullRomPoint(t, &pos, nullptr, points);
        glVertex3f(pos.x, pos.y, pos.z);
    }
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
}

// ------------------------------------------------------
// VBO
// ------------------------------------------------------

int loadModelVBO(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "[engine] Erro: nao foi possivel abrir " << filename << std::endl;
        return -1;
    }

    int n;
    file >> n;

    std::vector<float> vertices;
    vertices.reserve(n * 3);

    float x, y, z;
    while (file >> x >> y >> z) {
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);
    }

    Model m;
    m.vertexCount = vertices.size() / 3;

    glGenBuffers(1, &m.vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo_id);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    gModels.push_back(m);
    return gModels.size() - 1;
}

// ------------------------------------------------------
// XML
// ------------------------------------------------------

void parseGroup(XMLElement* element, Group& g) {
    if (!element) return;

    for (XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
        std::string name = child->Name();

        if (name == "transform") {
            parseGroup(child, g);
        }
        else if (name == "translate" || name == "rotate" || name == "scale") {
            Transformation t;
            t.type = name;

            t.x = child->FloatAttribute("x", 0);
            t.y = child->FloatAttribute("y", 0);
            t.z = child->FloatAttribute("z", 0);

            t.angle = child->FloatAttribute("angle", 0);
            t.time = child->FloatAttribute("time", 0);
            t.align = child->BoolAttribute("align", false);

            if (name == "translate") {
                for (XMLElement* p = child->FirstChildElement("point"); p; p = p->NextSiblingElement("point")) {
                    t.points.push_back({
                        p->FloatAttribute("x", 0),
                        p->FloatAttribute("y", 0),
                        p->FloatAttribute("z", 0)
                    });
                }
            }

            g.transforms.push_back(t);
        }
        else if (name == "models") {
            for (XMLElement* m = child->FirstChildElement("model"); m; m = m->NextSiblingElement("model")) {
                const char* file = m->Attribute("file");

                if (!file) {
                    std::cerr << "[engine] Erro: modelo sem atributo file" << std::endl;
                    continue;
                }

                int idx = loadModelVBO(file);
                if (idx != -1) {
                    g.modelIndices.push_back(idx);
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

// ------------------------------------------------------
// Câmara
// ------------------------------------------------------

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
        case 'w':
            gCam.pos.x += dirX * speed;
            gCam.pos.z += dirZ * speed;
            break;

        case 's':
            gCam.pos.x -= dirX * speed;
            gCam.pos.z -= dirZ * speed;
            break;

        case 'a':
            gCam.pos.x += dirZ * speed;
            gCam.pos.z -= dirX * speed;
            break;

        case 'd':
            gCam.pos.x -= dirZ * speed;
            gCam.pos.z += dirX * speed;
            break;

        case 'r':
            gCam.pos.y += speed;
            break;

        case 'f':
            gCam.pos.y -= speed;
            break;
    }

    updateCameraPosition();
    glutPostRedisplay();
}

void processSpecialKeys(int key, int xx, int yy) {
    switch (key) {
        case GLUT_KEY_LEFT:
            camAlpha += 2.0f;
            break;

        case GLUT_KEY_RIGHT:
            camAlpha -= 2.0f;
            break;

        case GLUT_KEY_UP:
            camBeta += 2.0f;
            if (camBeta > 89.0f) camBeta = 89.0f;
            break;

        case GLUT_KEY_DOWN:
            camBeta -= 2.0f;
            if (camBeta < -89.0f) camBeta = -89.0f;
            break;
    }

    updateCameraPosition();
    glutPostRedisplay();
}

// ------------------------------------------------------
// Alinhamento com curva
// ------------------------------------------------------

void buildRotationMatrix(Vec3 deriv, Vec3 up, float* m) {
    Vec3 z = normalize(deriv);
    Vec3 x = normalize(cross(up, z));
    Vec3 y = normalize(cross(z, x));

    m[0] = x.x; m[1] = x.y; m[2] = x.z; m[3] = 0;
    m[4] = y.x; m[5] = y.y; m[6] = y.z; m[7] = 0;
    m[8] = z.x; m[9] = z.y; m[10] = z.z; m[11] = 0;
    m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
}


void renderOrbit(float radius) {
    glColor3f(0.6f, 0.6f, 0.6f);

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 100; i++) {
        float angle = 2 * PI * i / 100;
        glVertex3f(radius * sin(angle), 0, radius * cos(angle));
    }
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
}


// ------------------------------------------------------
// Render
// ------------------------------------------------------

void renderGroup(const Group& g) {
    glPushMatrix();

    float elapsedTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

    for (size_t i = 0; i < g.transforms.size(); i++) {
    const auto& t = g.transforms[i];

    // DESENHAR ÓRBITA quando há rotate time seguido de translate estático
    if (t.type == "rotate" && t.time > 0 && i + 1 < g.transforms.size()) {
        const auto& next = g.transforms[i + 1];

        if (next.type == "translate" && next.time == 0) {
            float radius = sqrt(next.x * next.x + next.z * next.z);
            renderOrbit(radius);
        }
    }
        if (t.type == "translate") {
            if (t.time > 0 && t.points.size() >= 4) {
                // Desenha a curva fixa no mundo
                renderCatmullRomCurve(t.points);

                float gt = fmod(elapsedTime, t.time) / t.time;

                Vec3 pos, deriv;
                getGlobalCatmullRomPoint(gt, &pos, &deriv, t.points);

                glTranslatef(pos.x, pos.y, pos.z);

                if (t.align) {
                    float m[16];
                    buildRotationMatrix(deriv, {0, 1, 0}, m);
                    glMultMatrixf(m);
                }
            }
            else {
                glTranslatef(t.x, t.y, t.z);
            }
        }
        else if (t.type == "rotate") {
            float angle;

            if (t.time > 0) {
                angle = (fmod(elapsedTime, t.time) / t.time) * 360.0f;
            }
            else {
                angle = t.angle;
            }

            glRotatef(angle, t.x, t.y, t.z);
        }
        else if (t.type == "scale") {
            glScalef(t.x, t.y, t.z);
        }
    }

    for (int idx : g.modelIndices) {
        glBindBuffer(GL_ARRAY_BUFFER, gModels[idx].vbo_id);

        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, nullptr);

        glDrawArrays(GL_TRIANGLES, 0, gModels[idx].vertexCount);

        glDisableClientState(GL_VERTEX_ARRAY);
    }

    for (const auto& child : g.children) {
        renderGroup(child);
    }

    glPopMatrix();
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(
        gCam.pos.x, gCam.pos.y, gCam.pos.z,
        gCam.lookAt.x, gCam.lookAt.y, gCam.lookAt.z,
        gCam.up.x, gCam.up.y, gCam.up.z
    );

    // Eixos
    glBegin(GL_LINES);

    glColor3f(1, 0, 0);
    glVertex3f(-200, 0, 0);
    glVertex3f(200, 0, 0);

    glColor3f(0, 1, 0);
    glVertex3f(0, -200, 0);
    glVertex3f(0, 200, 0);

    glColor3f(0, 0, 1);
    glVertex3f(0, 0, -200);
    glVertex3f(0, 0, 200);

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

// ------------------------------------------------------
// Main
// ------------------------------------------------------

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: ./engine ficheiro.xml" << std::endl;
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(900, 700);
    glutCreateWindow("CG Engine - Fase 3");

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Erro GLEW: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    XMLDocument doc;

    if (doc.LoadFile(argv[1]) != XML_SUCCESS) {
        std::cerr << "[engine] Erro ao abrir XML: " << argv[1] << std::endl;
        return 1;
    }

    XMLElement* world = doc.FirstChildElement("world");

    if (!world) {
        std::cerr << "[engine] XML invalido: falta <world>" << std::endl;
        return 1;
    }

    XMLElement* window = world->FirstChildElement("window");
    if (window) {
        int w = window->IntAttribute("width", 900);
        int h = window->IntAttribute("height", 700);
        glutReshapeWindow(w, h);
    }

    XMLElement* cam = world->FirstChildElement("camera");

    if (!cam) {
        std::cerr << "[engine] XML invalido: falta <camera>" << std::endl;
        return 1;
    }

    XMLElement* pos = cam->FirstChildElement("position");
    XMLElement* la = cam->FirstChildElement("lookAt");
    XMLElement* up = cam->FirstChildElement("up");
    XMLElement* proj = cam->FirstChildElement("projection");

    if (!pos || !la || !up || !proj) {
        std::cerr << "[engine] XML invalido: camera incompleta" << std::endl;
        return 1;
    }

    gCam.pos = {
        pos->FloatAttribute("x", 0),
        pos->FloatAttribute("y", 0),
        pos->FloatAttribute("z", 0)
    };

    gCam.lookAt = {
        la->FloatAttribute("x", 0),
        la->FloatAttribute("y", 0),
        la->FloatAttribute("z", 0)
    };

    gCam.up = {
        up->FloatAttribute("x", 0),
        up->FloatAttribute("y", 1),
        up->FloatAttribute("z", 0)
    };

    gCam.fov = proj->FloatAttribute("fov", 60);
    gCam.nearp = proj->FloatAttribute("near", 1);
    gCam.farp = proj->FloatAttribute("far", 1000);

    initCameraAngles();
    updateCameraPosition();

    parseGroup(world->FirstChildElement("group"), gRootGroup);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glutDisplayFunc(renderScene);
    glutIdleFunc(renderScene);
    glutReshapeFunc(changeSize);
    glutKeyboardFunc(processKeys);
    glutSpecialFunc(processSpecialKeys);

    glutMainLoop();

    return 0;
}