#define GLUT_DISABLE_ATEXIT_HACK
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <map>
#include <cctype>

#define FREEGLUT_STATIC
#include <GL/glew.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

#include <IL/il.h>
#include "tinyxml2.h"

using namespace tinyxml2;

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Transformation {
    std::string type = "";
    float x = 0, y = 0, z = 0, angle = 0, time = 0;
    bool align = false;
    std::vector<Vec3> points;
};

struct Material {
    float diffuse[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    float ambient[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    float specular[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float emissive[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float shininess = 0.0f;
};

struct ModelInstance {
    int modelIdx = -1;
    GLuint textureID = 0;
    Material mat;
};

struct Model {
    GLuint vbo_id = 0;
    int vertexCount = 0;
    float radius = 1.0f;
};

struct Light {
    std::string type = "";
    float pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float dir[3] = {0.0f, -1.0f, 0.0f};
    float cutoff = 0.0f;
};

struct Group {
    std::vector<Transformation> transforms;
    std::vector<ModelInstance> models;
    std::vector<Group> children;
};

struct Camera {
    Vec3 pos, lookAt, up;
    float fov = 60.0f, nearp = 1.0f, farp = 1000.0f;
};

std::vector<Model> gModels;
std::vector<Light> gLights;
std::map<std::string, GLuint> gTextures;
Group gRootGroup;
Camera gCam;

float camAlpha = 0.0f;
float camBeta = 0.0f;
float speed = 0.5f;

const float PI = 3.14159265359f;

std::string gXMLDir = ".";

std::string getDirectory(const std::string& path) {
    size_t p = path.find_last_of("/\\");

    if (p == std::string::npos) {
        return ".";
    }

    return path.substr(0, p);
}

bool isAbsolutePath(const std::string& path) {
    if (path.empty()) return false;

    if (path[0] == '/' || path[0] == '\\') return true;

    return path.size() > 1 && path[1] == ':';
}

std::string resolveRelativeToXML(const std::string& path) {
    if (path.empty() || isAbsolutePath(path)) {
        return path;
    }

    return gXMLDir + "/" + path;
}

Vec3 normalize(Vec3 v) {
    float l = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    if (l == 0) {
        return Vec3{0, 0, 0};
    }

    return Vec3{v.x / l, v.y / l, v.z / l};
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

void buildRotationMatrix(Vec3 deriv, Vec3 up, float* m) {
    Vec3 z = normalize(deriv);
    Vec3 x = normalize(cross(up, z));
    Vec3 y = normalize(cross(z, x));

    m[0] = x.x;
    m[1] = x.y;
    m[2] = x.z;
    m[3] = 0;

    m[4] = y.x;
    m[5] = y.y;
    m[6] = y.z;
    m[7] = 0;

    m[8] = z.x;
    m[9] = z.y;
    m[10] = z.z;
    m[11] = 0;

    m[12] = 0;
    m[13] = 0;
    m[14] = 0;
    m[15] = 1;
}

void drawAxes() {
    glDisable(GL_LIGHTING);

    glBegin(GL_LINES);

    glColor3f(1, 0, 0);
    glVertex3f(-100, 0, 0);
    glVertex3f(100, 0, 0);

    glColor3f(0, 1, 0);
    glVertex3f(0, -100, 0);
    glVertex3f(0, 100, 0);

    glColor3f(0, 0, 1);
    glVertex3f(0, 0, -100);
    glVertex3f(0, 0, 100);

    glEnd();

    glEnable(GL_LIGHTING);
}

GLuint loadTexture(const std::string& path) {
    if (gTextures.count(path)) {
        return gTextures[path];
    }

    std::vector<std::string> tries;

    tries.push_back(path);

    std::string xmlRelative = resolveRelativeToXML(path);

    if (xmlRelative != path) {
        tries.push_back(xmlRelative);
    }

    unsigned int img;
    ilGenImages(1, &img);
    ilBindImage(img);

    std::string loadedPath;
    bool ok = false;

    for (const auto& candidate : tries) {
        if (ilLoadImage((ILstring)candidate.c_str())) {
            loadedPath = candidate;
            ok = true;
            break;
        }
    }

    if (!ok) {
        std::cout << "ERRO: Nao foi possivel carregar a textura: " << path << std::endl;
        std::cout << "      Caminhos tentados:" << std::endl;

        for (const auto& candidate : tries) {
            std::cout << "      - " << candidate << std::endl;
        }

        ilDeleteImages(1, &img);
        return 0;
    }

    ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

    int w = ilGetInteger(IL_IMAGE_WIDTH);
    int h = ilGetInteger(IL_IMAGE_HEIGHT);

    GLuint texID;

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        w,
        h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        ilGetData()
    );

    glGenerateMipmap(GL_TEXTURE_2D);

    ilDeleteImages(1, &img);

    gTextures[path] = texID;

    std::cout << "SUCESSO: Carregada textura "
              << loadedPath << " (" << w << "x" << h << ")" << std::endl;

    return texID;
}

int loadModelVBO(const std::string& filename) {
    std::vector<std::string> tries;

    tries.push_back(filename);

    std::string xmlRelative = resolveRelativeToXML(filename);

    if (xmlRelative != filename) {
        tries.push_back(xmlRelative);
    }

    std::ifstream file;
    std::string openedPath;

    for (const auto& candidate : tries) {
        file.open(candidate);

        if (file.is_open()) {
            openedPath = candidate;
            break;
        }

        file.clear();
    }

    if (!file.is_open()) {
        std::cout << "ERRO: Nao abriu ficheiro 3D: " << filename << std::endl;
        std::cout << "      Caminhos tentados:" << std::endl;

        for (const auto& candidate : tries) {
            std::cout << "      - " << candidate << std::endl;
        }

        return -1;
    }

    int n;
    file >> n;

    std::vector<float> data;
    data.reserve(n * 8);

    float val;

    while (file >> val) {
        data.push_back(val);
    }

    if ((int)data.size() < n * 8) {
        std::cout << "ERRO: ficheiro 3D antigo/incompleto: " << openedPath << std::endl;
        std::cout << "      A Fase 4 exige 8 floats por vertice: x y z nx ny nz u v." << std::endl;

        return -1;
    }

    float maxDist = 0.0f;

    for (int i = 0; i < n; i++) {
        float x = data[i * 8 + 0];
        float y = data[i * 8 + 1];
        float z = data[i * 8 + 2];

        float dist = sqrt(x * x + y * y + z * z);

        if (dist > maxDist) {
            maxDist = dist;
        }
    }

    Model m;

    m.vertexCount = n;
    m.radius = maxDist;

    glGenBuffers(1, &m.vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo_id);

    glBufferData(
        GL_ARRAY_BUFFER,
        data.size() * sizeof(float),
        data.data(),
        GL_STATIC_DRAW
    );

    gModels.push_back(m);

    std::cout << "SUCESSO: Carregado modelo "
              << openedPath << " (" << n << " vertices, raio " << maxDist << ")" << std::endl;

    return (int)gModels.size() - 1;
}

void getCatmullRomPoint(float t, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3* pos, Vec3* deriv) {
    float m[4][4] = {
        {-0.5f, 1.5f, -1.5f, 0.5f},
        {1.0f, -2.5f, 2.0f, -0.5f},
        {-0.5f, 0.0f, 0.5f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f}
    };

    float px[4] = {p0.x, p1.x, p2.x, p3.x};
    float py[4] = {p0.y, p1.y, p2.y, p3.y};
    float pz[4] = {p0.z, p1.z, p2.z, p3.z};

    float T[4] = {t * t * t, t * t, t, 1};
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
    int s = (int)points.size();

    float t = gt * s;

    int i = (int)floor(t);

    t -= i;

    getCatmullRomPoint(
        t,
        points[(i + s - 1) % s],
        points[i % s],
        points[(i + 1) % s],
        points[(i + 2) % s],
        pos,
        deriv
    );
}

void renderCatmullRomCurve(const std::vector<Vec3>& points) {
    glDisable(GL_LIGHTING);

    glColor3f(0.5f, 0.5f, 0.5f);

    glBegin(GL_LINE_LOOP);

    for (float t = 0; t < 1.0f; t += 0.01f) {
        Vec3 p;

        getGlobalCatmullRomPoint(t, &p, nullptr, points);

        glVertex3f(p.x, p.y, p.z);
    }

    glEnd();

    glEnable(GL_LIGHTING);
}

void updateCameraPosition() {
    float a = camAlpha * PI / 180.0f;
    float b = camBeta * PI / 180.0f;

    gCam.lookAt.x = gCam.pos.x + cos(b) * sin(a);
    gCam.lookAt.y = gCam.pos.y + sin(b);
    gCam.lookAt.z = gCam.pos.z + cos(b) * cos(a);
}

void initCameraAngles() {
    Vec3 d = {
        gCam.lookAt.x - gCam.pos.x,
        gCam.lookAt.y - gCam.pos.y,
        gCam.lookAt.z - gCam.pos.z
    };

    float r = sqrt(d.x * d.x + d.y * d.y + d.z * d.z);

    camAlpha = atan2(d.x, d.z) * 180.0f / PI;
    camBeta = asin(d.y / r) * 180.0f / PI;
}

void processKeys(unsigned char key, int xx, int yy) {
    float a = camAlpha * PI / 180.0f;

    switch (tolower(key)) {
        case 'w':
            gCam.pos.x += sin(a) * speed;
            gCam.pos.z += cos(a) * speed;
            break;

        case 's':
            gCam.pos.x -= sin(a) * speed;
            gCam.pos.z -= cos(a) * speed;
            break;

        case 'a':
            gCam.pos.x += cos(a) * speed;
            gCam.pos.z -= sin(a) * speed;
            break;

        case 'd':
            gCam.pos.x -= cos(a) * speed;
            gCam.pos.z += sin(a) * speed;
            break;

        case 'r':
            gCam.pos.y += speed;
            break;

        case 'f':
            gCam.pos.y -= speed;
            break;
    }

    updateCameraPosition();
}

void processSpecialKeys(int key, int xx, int yy) {
    if (key == GLUT_KEY_LEFT) {
        camAlpha += 2.0f;
    }

    if (key == GLUT_KEY_RIGHT) {
        camAlpha -= 2.0f;
    }

    if (key == GLUT_KEY_UP) {
        camBeta += 2.0f;

        if (camBeta > 89.0f) {
            camBeta = 89.0f;
        }
    }

    if (key == GLUT_KEY_DOWN) {
        camBeta -= 2.0f;

        if (camBeta < -89.0f) {
            camBeta = -89.0f;
        }
    }

    updateCameraPosition();
}

bool sphereInFrustum(float radius) {
    float modelview[16];
    float projection[16];
    float clip[16];
    float frustum[6][4];

    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);

    clip[0]  = modelview[0] * projection[0] + modelview[1] * projection[4] + modelview[2] * projection[8] + modelview[3] * projection[12];
    clip[1]  = modelview[0] * projection[1] + modelview[1] * projection[5] + modelview[2] * projection[9] + modelview[3] * projection[13];
    clip[2]  = modelview[0] * projection[2] + modelview[1] * projection[6] + modelview[2] * projection[10] + modelview[3] * projection[14];
    clip[3]  = modelview[0] * projection[3] + modelview[1] * projection[7] + modelview[2] * projection[11] + modelview[3] * projection[15];

    clip[4]  = modelview[4] * projection[0] + modelview[5] * projection[4] + modelview[6] * projection[8] + modelview[7] * projection[12];
    clip[5]  = modelview[4] * projection[1] + modelview[5] * projection[5] + modelview[6] * projection[9] + modelview[7] * projection[13];
    clip[6]  = modelview[4] * projection[2] + modelview[5] * projection[6] + modelview[6] * projection[10] + modelview[7] * projection[14];
    clip[7]  = modelview[4] * projection[3] + modelview[5] * projection[7] + modelview[6] * projection[11] + modelview[7] * projection[15];

    clip[8]  = modelview[8] * projection[0] + modelview[9] * projection[4] + modelview[10] * projection[8] + modelview[11] * projection[12];
    clip[9]  = modelview[8] * projection[1] + modelview[9] * projection[5] + modelview[10] * projection[9] + modelview[11] * projection[13];
    clip[10] = modelview[8] * projection[2] + modelview[9] * projection[6] + modelview[10] * projection[10] + modelview[11] * projection[14];
    clip[11] = modelview[8] * projection[3] + modelview[9] * projection[7] + modelview[10] * projection[11] + modelview[11] * projection[15];

    clip[12] = modelview[12] * projection[0] + modelview[13] * projection[4] + modelview[14] * projection[8] + modelview[15] * projection[12];
    clip[13] = modelview[12] * projection[1] + modelview[13] * projection[5] + modelview[14] * projection[9] + modelview[15] * projection[13];
    clip[14] = modelview[12] * projection[2] + modelview[13] * projection[6] + modelview[14] * projection[10] + modelview[15] * projection[14];
    clip[15] = modelview[12] * projection[3] + modelview[13] * projection[7] + modelview[14] * projection[11] + modelview[15] * projection[15];

    // Plano direito
    frustum[0][0] = clip[3] - clip[0];
    frustum[0][1] = clip[7] - clip[4];
    frustum[0][2] = clip[11] - clip[8];
    frustum[0][3] = clip[15] - clip[12];

    // Plano esquerdo
    frustum[1][0] = clip[3] + clip[0];
    frustum[1][1] = clip[7] + clip[4];
    frustum[1][2] = clip[11] + clip[8];
    frustum[1][3] = clip[15] + clip[12];

    // Plano inferior
    frustum[2][0] = clip[3] + clip[1];
    frustum[2][1] = clip[7] + clip[5];
    frustum[2][2] = clip[11] + clip[9];
    frustum[2][3] = clip[15] + clip[13];

    // Plano superior
    frustum[3][0] = clip[3] - clip[1];
    frustum[3][1] = clip[7] - clip[5];
    frustum[3][2] = clip[11] - clip[9];
    frustum[3][3] = clip[15] - clip[13];

    // Plano longínquo
    frustum[4][0] = clip[3] - clip[2];
    frustum[4][1] = clip[7] - clip[6];
    frustum[4][2] = clip[11] - clip[10];
    frustum[4][3] = clip[15] - clip[14];

    // Plano próximo
    frustum[5][0] = clip[3] + clip[2];
    frustum[5][1] = clip[7] + clip[6];
    frustum[5][2] = clip[11] + clip[10];
    frustum[5][3] = clip[15] + clip[14];

    for (int i = 0; i < 6; i++) {
        float length = sqrt(
            frustum[i][0] * frustum[i][0] +
            frustum[i][1] * frustum[i][1] +
            frustum[i][2] * frustum[i][2]
        );

        if (length == 0) continue;

        frustum[i][0] /= length;
        frustum[i][1] /= length;
        frustum[i][2] /= length;
        frustum[i][3] /= length;

        if (frustum[i][3] <= -radius) {
            return false;
        }
    }

    return true;
}

void renderGroup(const Group& g) {
    glPushMatrix();

    float t_now = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

    for (const auto& t : g.transforms) {
        if (t.type == "translate") {
            if (t.time > 0 && t.points.size() >= 4) {
                renderCatmullRomCurve(t.points);

                Vec3 p, d;

                getGlobalCatmullRomPoint(
                    fmod(t_now, t.time) / t.time,
                    &p,
                    &d,
                    t.points
                );

                glTranslatef(p.x, p.y, p.z);

                if (t.align) {
                    float m[16];

                    buildRotationMatrix(d, {0, 1, 0}, m);

                    glMultMatrixf(m);
                }
            }
            else {
                glTranslatef(t.x, t.y, t.z);
            }
        }
        else if (t.type == "rotate") {
            float angle = t.angle;

            if (t.time > 0) {
                angle = (fmod(t_now, t.time) / t.time) * 360.0f;
            }

            glRotatef(angle, t.x, t.y, t.z);
        }
        else if (t.type == "scale") {
            glScalef(t.x, t.y, t.z);
        }
    }

    for (const auto& inst : g.models) {
        if (inst.modelIdx < 0) continue;

        const auto& m = gModels[inst.modelIdx];

        if (!sphereInFrustum(m.radius)) {
            continue;
        }

        glMaterialfv(GL_FRONT, GL_DIFFUSE, inst.mat.diffuse);
        glMaterialfv(GL_FRONT, GL_AMBIENT, inst.mat.ambient);
        glMaterialfv(GL_FRONT, GL_SPECULAR, inst.mat.specular);
        glMaterialfv(GL_FRONT, GL_EMISSION, inst.mat.emissive);
        glMaterialf(GL_FRONT, GL_SHININESS, inst.mat.shininess);

        if (inst.textureID != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, inst.textureID);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        else {
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glBindBuffer(GL_ARRAY_BUFFER, m.vbo_id);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glVertexPointer(
            3,
            GL_FLOAT,
            8 * sizeof(float),
            (void*)0
        );

        glNormalPointer(
            GL_FLOAT,
            8 * sizeof(float),
            (void*)(3 * sizeof(float))
        );

        glTexCoordPointer(
            2,
            GL_FLOAT,
            8 * sizeof(float),
            (void*)(6 * sizeof(float))
        );

        glDrawArrays(GL_TRIANGLES, 0, m.vertexCount);

        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        glBindTexture(GL_TEXTURE_2D, 0);
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

    drawAxes();

    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    for (int i = 0; i < (int)gLights.size() && i < 8; i++) {
        glEnable(GL_LIGHT0 + i);

        glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, white);
        glLightfv(GL_LIGHT0 + i, GL_SPECULAR, white);
        glLightfv(GL_LIGHT0 + i, GL_POSITION, gLights[i].pos);

        if (gLights[i].type == "spot") {
            glLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION, gLights[i].dir);
            glLightf(GL_LIGHT0 + i, GL_SPOT_CUTOFF, gLights[i].cutoff);
        }
    }

    renderGroup(gRootGroup);

    glutSwapBuffers();
}

void parseGroup(XMLElement* element, Group& g) {
    if (!element) return;

    for (XMLElement* child = element->FirstChildElement();
         child;
         child = child->NextSiblingElement()) {

        std::string name = child->Name();

        if (name == "transform") {
            parseGroup(child, g);
        }
        else if (name == "translate" || name == "rotate" || name == "scale") {
            Transformation t;

            t.type = name;

            t.x = child->FloatAttribute("x");
            t.y = child->FloatAttribute("y");
            t.z = child->FloatAttribute("z");

            t.angle = child->FloatAttribute("angle");
            t.time = child->FloatAttribute("time");
            t.align = child->BoolAttribute("align");

            if (name == "translate") {
                for (XMLElement* p = child->FirstChildElement("point");
                     p;
                     p = p->NextSiblingElement("point")) {

                    t.points.push_back({
                        p->FloatAttribute("x"),
                        p->FloatAttribute("y"),
                        p->FloatAttribute("z")
                    });
                }
            }

            g.transforms.push_back(t);
        }
        else if (name == "models") {
            for (XMLElement* m = child->FirstChildElement("model");
                 m;
                 m = m->NextSiblingElement("model")) {

                ModelInstance inst;

                const char* modelFile = m->Attribute("file");

                if (!modelFile) continue;

                int idx = loadModelVBO(modelFile);

                if (idx != -1) {
                    inst.modelIdx = idx;

                    XMLElement* texture = m->FirstChildElement("texture");

                    if (texture && texture->Attribute("file")) {
                        inst.textureID = loadTexture(texture->Attribute("file"));
                    }
                    else if (m->Attribute("texture")) {
                        inst.textureID = loadTexture(m->Attribute("texture"));
                    }

                    XMLElement* color = m->FirstChildElement("color");

                    if (color) {
                        auto parseColor = [](XMLElement* e, float* a) {
                            if (e) {
                                a[0] = e->FloatAttribute("R") / 255.0f;
                                a[1] = e->FloatAttribute("G") / 255.0f;
                                a[2] = e->FloatAttribute("B") / 255.0f;
                                a[3] = 1.0f;
                            }
                        };

                        parseColor(color->FirstChildElement("diffuse"), inst.mat.diffuse);
                        parseColor(color->FirstChildElement("ambient"), inst.mat.ambient);
                        parseColor(color->FirstChildElement("specular"), inst.mat.specular);
                        parseColor(color->FirstChildElement("emissive"), inst.mat.emissive);

                        if (color->FirstChildElement("shininess")) {
                            inst.mat.shininess =
                                color->FirstChildElement("shininess")->FloatAttribute("value");
                        }
                    }

                    g.models.push_back(inst);
                }
            }
        }
        else if (name == "group") {
            Group cg;

            parseGroup(child, cg);

            g.children.push_back(cg);
        }
    }
}

void changeSize(int w, int h) {
    if (h == 0) h = 1;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(gCam.fov, (float)w / h, gCam.nearp, gCam.farp);

    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Uso: engine <ficheiro.xml>" << std::endl;
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(900, 700);
    glutCreateWindow("CG Engine - Fase 4");

    glewInit();

    ilInit();
    ilEnable(IL_ORIGIN_SET);
    ilOriginFunc(IL_ORIGIN_LOWER_LEFT);

    gXMLDir = getDirectory(argv[1]);

    XMLDocument doc;

    if (doc.LoadFile(argv[1]) != XML_SUCCESS) {
        std::cout << "ERRO: XML nao encontrado." << std::endl;
        return 1;
    }

    XMLElement* world = doc.FirstChildElement("world");

    if (!world) {
        std::cout << "ERRO: XML sem elemento <world>." << std::endl;
        return 1;
    }

    XMLElement* cam = world->FirstChildElement("camera");

    if (!cam) {
        std::cout << "ERRO: XML sem <camera>." << std::endl;
        return 1;
    }

    XMLElement* pos = cam->FirstChildElement("position");
    XMLElement* look = cam->FirstChildElement("lookAt");
    XMLElement* up = cam->FirstChildElement("up");
    XMLElement* proj = cam->FirstChildElement("projection");

    gCam.pos = {
        pos->FloatAttribute("x"),
        pos->FloatAttribute("y"),
        pos->FloatAttribute("z")
    };

    gCam.lookAt = {
        look->FloatAttribute("x"),
        look->FloatAttribute("y"),
        look->FloatAttribute("z")
    };

    gCam.up = {
        up->FloatAttribute("x"),
        up->FloatAttribute("y"),
        up->FloatAttribute("z")
    };

    gCam.fov = proj->FloatAttribute("fov");
    gCam.nearp = proj->FloatAttribute("near");
    gCam.farp = proj->FloatAttribute("far");

    initCameraAngles();
    updateCameraPosition();

    XMLElement* ls = world->FirstChildElement("lights");

    if (ls) {
        for (XMLElement* l = ls->FirstChildElement("light");
             l;
             l = l->NextSiblingElement("light")) {

            Light nl;

            if (l->Attribute("type")) {
                nl.type = l->Attribute("type");
            }

            if (nl.type == "directional") {
                nl.pos[0] = l->FloatAttribute("dirx");
                nl.pos[1] = l->FloatAttribute("diry");
                nl.pos[2] = l->FloatAttribute("dirz");
                nl.pos[3] = 0.0f;
            }
            else {
                nl.pos[0] = l->FloatAttribute("posx");
                nl.pos[1] = l->FloatAttribute("posy");
                nl.pos[2] = l->FloatAttribute("posz");
                nl.pos[3] = 1.0f;
            }

            if (nl.type == "spot") {
                nl.dir[0] = l->FloatAttribute("dirx");
                nl.dir[1] = l->FloatAttribute("diry");
                nl.dir[2] = l->FloatAttribute("dirz");
                nl.cutoff = l->FloatAttribute("cutoff");
            }

            gLights.push_back(nl);
        }
    }

    parseGroup(world->FirstChildElement("group"), gRootGroup);

    glEnable(GL_DEPTH_TEST);

    // Back-Face Culling
    glEnable(GL_CULL_FACE);

    glEnable(GL_LIGHTING);
    glEnable(GL_RESCALE_NORMAL);

    float global_amb[4] = {0.3f, 0.3f, 0.3f, 1.0f};

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_amb);

    glutDisplayFunc(renderScene);
    glutIdleFunc(renderScene);
    glutReshapeFunc(changeSize);
    glutKeyboardFunc(processKeys);
    glutSpecialFunc(processSpecialKeys);

    glutMainLoop();

    return 0;
}