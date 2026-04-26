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
 
// ---------------------------------------------------------------------------
// STRUCTURES
// ---------------------------------------------------------------------------
 
struct Vec3 { float x, y, z; };
 
struct Transformation {
    std::string type;
    // static translate / scale
    float x = 0, y = 0, z = 0;
    // static rotate
    float angle = 0;
    // time-based
    float time = 0;
    // catmull-rom alignment
    bool align = false;
    // catmull-rom control points
    std::vector<Vec3> points;
};
 
struct Model {
    GLuint vbo_id    = 0;
    int    vertexCount = 0;
};
 
struct Group {
    std::vector<Transformation> transforms;
    std::vector<int>            modelIndices;
    std::vector<Group>          children;
};
 
struct Camera {
    Vec3  pos, lookAt, up;
    float fov = 60.0f, nearp = 1.0f, farp = 1000.0f;
};
 
// ---------------------------------------------------------------------------
// GLOBALS
// ---------------------------------------------------------------------------
 
std::vector<Model> gModels;
Group              gRootGroup;
Camera             gCam;
float              camAlpha = 0.0f, camBeta = 0.0f, camSpeed = 5.0f;
const float        PI = 3.14159265359f;
 
// ---------------------------------------------------------------------------
// CATMULL-ROM MATH
// ---------------------------------------------------------------------------
 
/*  Catmull-Rom matrix (centripetal α=0.5 in the traditional formulation,
    but the standard CG course uses the Catmull-Rom matrix below).        */
static const float CR[4][4] = {
    {-0.5f,  1.5f, -1.5f,  0.5f},
    { 1.0f, -2.5f,  2.0f, -0.5f},
    {-0.5f,  0.0f,  0.5f,  0.0f},
    { 0.0f,  1.0f,  0.0f,  0.0f}
};
 
// Multiply a 4-vector by the CR matrix and dot with a control-point vector.
static float crEval(const float T[4], const float p[4]) {
    float res = 0.0f;
    for (int i = 0; i < 4; i++) {
        float a = 0.0f;
        for (int j = 0; j < 4; j++) a += T[j] * CR[j][i];
        res += a * p[i];
    }
    return res;
}
 
void getCatmullRomPoint(float t,
                        Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3,
                        Vec3* pos, Vec3* deriv)
{
    float px[4] = { p0.x, p1.x, p2.x, p3.x };
    float py[4] = { p0.y, p1.y, p2.y, p3.y };
    float pz[4] = { p0.z, p1.z, p2.z, p3.z };
 
    float T[4]  = { t*t*t, t*t, t, 1.0f };
    float dT[4] = { 3.0f*t*t, 2.0f*t, 1.0f, 0.0f };
 
    if (pos) {
        pos->x = crEval(T, px);
        pos->y = crEval(T, py);
        pos->z = crEval(T, pz);
    }
    if (deriv) {
        deriv->x = crEval(dT, px);
        deriv->y = crEval(dT, py);
        deriv->z = crEval(dT, pz);
    }
}
 
void getGlobalCatmullRomPoint(float gt,
                               const std::vector<Vec3>& points,
                               Vec3* pos, Vec3* deriv)
{
    int   size  = (int)points.size();
    float tScaled = gt * size;
    int   index   = (int)floorf(tScaled);
    float t       = tScaled - index;
 
    Vec3 p0 = points[(index + size - 1) % size];
    Vec3 p1 = points[ index             % size];
    Vec3 p2 = points[(index + 1)        % size];
    Vec3 p3 = points[(index + 2)        % size];
 
    getCatmullRomPoint(t, p0, p1, p2, p3, pos, deriv);
}
 
// ---------------------------------------------------------------------------
// DRAW CATMULL-ROM TRAJECTORY
// ---------------------------------------------------------------------------
 
void renderCatmullRomCurve(const std::vector<Vec3>& points) {
    glBegin(GL_LINE_LOOP);
    const int steps = 200;
    for (int i = 0; i <= steps; i++) {
        float gt = (float)i / steps;
        Vec3 pos;
        getGlobalCatmullRomPoint(gt, points, &pos, nullptr);
        glVertex3f(pos.x, pos.y, pos.z);
    }
    glEnd();
}
 
// ---------------------------------------------------------------------------
// VBO LOADING
// ---------------------------------------------------------------------------
 
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
    m.vertexCount = n;
    glGenBuffers(1, &m.vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo_id);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(vertices.size() * sizeof(float)),
                 vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
 
    gModels.push_back(m);
    std::cout << "[engine] Carregado VBO: " << filename
              << " (" << n << " vertices)" << std::endl;
    return (int)gModels.size() - 1;
}
 
// ---------------------------------------------------------------------------
// XML PARSING
// ---------------------------------------------------------------------------
 
void parseGroup(XMLElement* element, Group& g) {
    for (XMLElement* child = element->FirstChildElement();
         child;
         child = child->NextSiblingElement())
    {
        std::string name = child->Name();
 
        if (name == "transform") {
            // Recurse into <transform> wrapper
            parseGroup(child, g);
        }
        else if (name == "translate" || name == "rotate" || name == "scale") {
            Transformation t;
            t.type  = name;
            t.x     = child->FloatAttribute("x",     0.0f);
            t.y     = child->FloatAttribute("y",     0.0f);
            t.z     = child->FloatAttribute("z",     0.0f);
            t.angle = child->FloatAttribute("angle", 0.0f);
            t.time  = child->FloatAttribute("time",  0.0f);
            t.align = child->BoolAttribute ("align", false);
 
            if (name == "translate") {
                for (XMLElement* p = child->FirstChildElement("point");
                     p;
                     p = p->NextSiblingElement("point"))
                {
                    t.points.push_back({
                        p->FloatAttribute("x", 0.0f),
                        p->FloatAttribute("y", 0.0f),
                        p->FloatAttribute("z", 0.0f)
                    });
                }
            }
            g.transforms.push_back(t);
        }
        else if (name == "models") {
            for (XMLElement* m = child->FirstChildElement("model");
                 m;
                 m = m->NextSiblingElement("model"))
            {
                const char* filePath = m->Attribute("file");
                if (filePath) {
                    int idx = loadModelVBO(filePath);
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
 
// ---------------------------------------------------------------------------
// CAMERA
// ---------------------------------------------------------------------------
 
void updateCameraPosition() {
    float aRad = camAlpha * PI / 180.0f;
    float bRad = camBeta  * PI / 180.0f;
    gCam.lookAt.x = gCam.pos.x + cosf(bRad) * sinf(aRad);
    gCam.lookAt.y = gCam.pos.y + sinf(bRad);
    gCam.lookAt.z = gCam.pos.z + cosf(bRad) * cosf(aRad);
}
 
void initCameraAngles() {
    float dx = gCam.lookAt.x - gCam.pos.x;
    float dy = gCam.lookAt.y - gCam.pos.y;
    float dz = gCam.lookAt.z - gCam.pos.z;
    float r  = sqrtf(dx*dx + dy*dy + dz*dz);
    camAlpha = atan2f(dx, dz) * 180.0f / PI;
    camBeta  = asinf(dy / r)  * 180.0f / PI;
}
 
void processKeys(unsigned char key, int /*x*/, int /*y*/) {
    float aRad = camAlpha * PI / 180.0f;
    float dirX =  sinf(aRad);
    float dirZ =  cosf(aRad);
    switch (tolower(key)) {
        case 'w': gCam.pos.x += dirX * camSpeed; gCam.pos.z += dirZ * camSpeed; break;
        case 's': gCam.pos.x -= dirX * camSpeed; gCam.pos.z -= dirZ * camSpeed; break;
        case 'a': gCam.pos.x += dirZ * camSpeed; gCam.pos.z -= dirX * camSpeed; break;
        case 'd': gCam.pos.x -= dirZ * camSpeed; gCam.pos.z += dirX * camSpeed; break;
        case 'r': gCam.pos.y += camSpeed; break;
        case 'f': gCam.pos.y -= camSpeed; break;
        case 27 : exit(0); // ESC
    }
    updateCameraPosition();
    glutPostRedisplay();
}
 
void processSpecialKeys(int key, int /*x*/, int /*y*/) {
    switch (key) {
        case GLUT_KEY_LEFT:  camAlpha += 2.0f; break;
        case GLUT_KEY_RIGHT: camAlpha -= 2.0f; break;
        case GLUT_KEY_UP:    camBeta  += 2.0f; if (camBeta  >  89.0f) camBeta  =  89.0f; break;
        case GLUT_KEY_DOWN:  camBeta  -= 2.0f; if (camBeta  < -89.0f) camBeta  = -89.0f; break;
    }
    updateCameraPosition();
    glutPostRedisplay();
}
 
// ---------------------------------------------------------------------------
// ALIGNMENT MATRIX (align object tangent to Catmull-Rom curve)
//
//  Z-axis  = normalised tangent (deriv)
//  X-axis  = Z × world-up
//  Y-axis  = Z × X
//  The result is stored in column-major order for glMultMatrixf.
// ---------------------------------------------------------------------------
 
static Vec3 normalise(Vec3 v) {
    float n = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (n < 1e-6f) return {0,0,1};
    return { v.x/n, v.y/n, v.z/n };
}
static Vec3 cross(Vec3 a, Vec3 b) {
    return { a.y*b.z - a.z*b.y,
             a.z*b.x - a.x*b.z,
             a.x*b.y - a.y*b.x };
}
 
void buildAlignMatrix(Vec3 tangent, float m[16]) {
    // Zero out
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
 
    Vec3 Z  = normalise(tangent);
    Vec3 up = {0.0f, 1.0f, 0.0f};
    // Handle degenerate case where tangent is parallel to up
    if (fabsf(Z.y) > 0.99f) up = {0.0f, 0.0f, 1.0f};
 
    Vec3 X  = normalise(cross(Z, up));
    Vec3 Y  = cross(X, Z);   // already unit length
 
    // Column-major layout for OpenGL
    m[0]  = X.x; m[1]  = X.y; m[2]  = X.z;
    m[4]  = Y.x; m[5]  = Y.y; m[6]  = Y.z;
    m[8]  = Z.x; m[9]  = Z.y; m[10] = Z.z;
    m[15] = 1.0f;
}
 
// ---------------------------------------------------------------------------
// RENDER
// ---------------------------------------------------------------------------
 
void renderGroup(const Group& g) {
    glPushMatrix();
 
    float elapsedSec = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
 
    for (const auto& t : g.transforms) {
 
        if (t.type == "translate") {
 
            if (t.time > 0.0f && t.points.size() >= 4) {
                // ---- Catmull-Rom animation ----
                float gt = fmodf(elapsedSec, t.time) / t.time;
 
                Vec3 pos, deriv;
                getGlobalCatmullRomPoint(gt, t.points, &pos, &deriv);
 
                glTranslatef(pos.x, pos.y, pos.z);
 
                if (t.align) {
                    float m[16] = {};
                    buildAlignMatrix(deriv, m);
                    glMultMatrixf(m);
                }
 
                // Draw the trajectory in yellow (save/restore colour state)
                glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
                glDisable(GL_LIGHTING);
                glColor3f(1.0f, 1.0f, 0.0f);
                renderCatmullRomCurve(t.points);
                glPopAttrib();
 
            } else {
                // ---- Static translation ----
                glTranslatef(t.x, t.y, t.z);
            }
 
        } else if (t.type == "rotate") {
 
            float angle;
            if (t.time > 0.0f) {
                // Full 360° in t.time seconds
                angle = fmodf(elapsedSec, t.time) / t.time * 360.0f;
            } else {
                angle = t.angle;
            }
            glRotatef(angle, t.x, t.y, t.z);
 
        } else if (t.type == "scale") {
            glScalef(t.x, t.y, t.z);
        }
    }
 
    // Draw models using VBOs
    glEnableClientState(GL_VERTEX_ARRAY);
    for (int idx : g.modelIndices) {
        glBindBuffer(GL_ARRAY_BUFFER, gModels[idx].vbo_id);
        glVertexPointer(3, GL_FLOAT, 0, nullptr);
        glDrawArrays(GL_TRIANGLES, 0, gModels[idx].vertexCount);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
 
    for (const auto& child : g.children) renderGroup(child);
 
    glPopMatrix();
}
 
void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(gCam.pos.x,    gCam.pos.y,    gCam.pos.z,
              gCam.lookAt.x, gCam.lookAt.y, gCam.lookAt.z,
              gCam.up.x,     gCam.up.y,     gCam.up.z);
 
    // Coordinate axes (X=red, Y=green, Z=blue)
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
        glColor3f(1,0,0); glVertex3f(-200,0,0); glVertex3f(200,0,0);
        glColor3f(0,1,0); glVertex3f(0,-200,0); glVertex3f(0,200,0);
        glColor3f(0,0,1); glVertex3f(0,0,-200); glVertex3f(0,0,200);
    glEnd();
    glColor3f(1,1,1);
    glPopAttrib();
 
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
 
// ---------------------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------------------
 
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: engine <scene.xml>" << std::endl;
        return 1;
    }
 
    // Default camera in case XML parsing fails
    gCam.pos    = {0, 10, 30};
    gCam.lookAt = {0, 0, 0};
    gCam.up     = {0, 1, 0};
 
    // Read XML first (before GLUT so we can read window size)
    XMLDocument doc;
    int winW = 800, winH = 600;
 
    if (doc.LoadFile(argv[1]) != XML_SUCCESS) {
        std::cerr << "Erro ao carregar XML: " << argv[1] << std::endl;
        return 1;
    }
 
    XMLElement* world = doc.FirstChildElement("world");
    if (!world) { std::cerr << "Elemento <world> nao encontrado." << std::endl; return 1; }
 
    // Window size
    XMLElement* win = world->FirstChildElement("window");
    if (win) {
        winW = win->IntAttribute("width",  800);
        winH = win->IntAttribute("height", 600);
    }
 
    // Camera
    XMLElement* cam = world->FirstChildElement("camera");
    if (cam) {
        if (auto* e = cam->FirstChildElement("position"))
            gCam.pos = { e->FloatAttribute("x"), e->FloatAttribute("y"), e->FloatAttribute("z") };
        if (auto* e = cam->FirstChildElement("lookAt"))
            gCam.lookAt = { e->FloatAttribute("x"), e->FloatAttribute("y"), e->FloatAttribute("z") };
        if (auto* e = cam->FirstChildElement("up"))
            gCam.up = { e->FloatAttribute("x",0), e->FloatAttribute("y",1), e->FloatAttribute("z",0) };
        if (auto* e = cam->FirstChildElement("projection")) {
            gCam.fov   = e->FloatAttribute("fov",  60.0f);
            gCam.nearp = e->FloatAttribute("near",  1.0f);
            gCam.farp  = e->FloatAttribute("far", 1000.0f);
        }
    }
    initCameraAngles();
    updateCameraPosition();
 
    // GLUT init
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("CG Engine - Fase 3");
    glewInit();
 
    // Load scene groups (VBOs need an active GL context)
    XMLElement* rootGroup = world->FirstChildElement("group");
    if (rootGroup) parseGroup(rootGroup, gRootGroup);
 
    // OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
 
    // Callbacks
    glutDisplayFunc   (renderScene);
    glutIdleFunc      (renderScene);
    glutReshapeFunc   (changeSize);
    glutKeyboardFunc  (processKeys);
    glutSpecialFunc   (processSpecialKeys);
 
    glutMainLoop();
    return 0;
}
