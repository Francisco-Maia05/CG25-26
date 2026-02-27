#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

struct Vec3 { float x=0, y=0, z=0; };

struct Camera {
    Vec3 position{0,0,5};
    Vec3 lookAt{0,0,0};
    Vec3 up{0,1,0};
    float fov=60.0f, nearp=1.0f, farp=1000.0f;
};

struct Model {
    std::string file;
    std::vector<float> vertices; // x,y,z ...
};

static int gWinW = 512, gWinH = 512;
static Camera gCam;
static std::vector<Model> gModels;
// le o ficheiro XML para uma string
static std::string readFileToString(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Failed to open file: " << path << "\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
/*recebe um bocado de texto XML e extrai o valor de um atributo específico key, 
convertendo-o para float e colocando-o em out*/
static bool extractAttrFloat(const std::string& tag, const std::string& key, float& out) {
    auto pos = tag.find(key + "=\"");
    if (pos == std::string::npos) return false;
    pos += key.size() + 2;
    auto end = tag.find("\"", pos);
    if (end == std::string::npos) return false;
    out = std::stof(tag.substr(pos, end - pos));
    return true;
}
// mesma coisa mas para int
static bool extractAttrInt(const std::string& tag, const std::string& key, int& out) {
    float tmp;
    if (!extractAttrFloat(tag, key, tmp)) return false;
    out = (int)tmp;
    return true;
}
// mesma coisa mas para string
static bool extractAttrStr(const std::string& tag, const std::string& key, std::string& out) {
    auto pos = tag.find(key + "=\"");
    if (pos == std::string::npos) return false;
    pos += key.size() + 2;
    auto end = tag.find("\"", pos);
    if (end == std::string::npos) return false;
    out = tag.substr(pos, end - pos);
    return true;
}
/*parser XML simples para extrair as informações necessárias para configurar a janela,
 a câmera e os modelos a serem renderizados.se tagName="window", <window width="512" height="512"*/ 
static std::string getTagChunk(const std::string& xml, const std::string& tagName) {
    auto pos = xml.find("<" + tagName);
    if (pos == std::string::npos) return "";
    auto end = xml.find("/>", pos);
    if (end == std::string::npos) return "";
    return xml.substr(pos, end - pos);
}
// interpreta o XML para configurar a janela, câmera e modelos
static void parseWindow(const std::string& xml) {
    std::string tag = getTagChunk(xml, "window");
    if (tag.empty()) return;
    extractAttrInt(tag, "width", gWinW);
    extractAttrInt(tag, "height", gWinH);
}

void drawAxes() {
    glBegin(GL_LINES);
    // Eixo X (Vermelho)
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-100.0f, 0.0f, 0.0f);
    glVertex3f(100.0f, 0.0f, 0.0f);

    // Eixo Y (Verde)
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, -100.0f, 0.0f);
    glVertex3f(0.0f, 100.0f, 0.0f);

    // Eixo Z (Azul)
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, -100.0f);
    glVertex3f(0.0f, 0.0f, 100.0f);
    glEnd();
}

// interpreta o XML para configurar a câmera
static void parseCamera(const std::string& xml) {
    {
        std::string tag = getTagChunk(xml, "position");
        if (!tag.empty()) {
            extractAttrFloat(tag, "x", gCam.position.x);
            extractAttrFloat(tag, "y", gCam.position.y);
            extractAttrFloat(tag, "z", gCam.position.z);
        }
    }
    {
        std::string tag = getTagChunk(xml, "lookAt");
        if (!tag.empty()) {
            extractAttrFloat(tag, "x", gCam.lookAt.x);
            extractAttrFloat(tag, "y", gCam.lookAt.y);
            extractAttrFloat(tag, "z", gCam.lookAt.z);
        }
    }
    {
        std::string tag = getTagChunk(xml, "up");
        if (!tag.empty()) {
            extractAttrFloat(tag, "x", gCam.up.x);
            extractAttrFloat(tag, "y", gCam.up.y);
            extractAttrFloat(tag, "z", gCam.up.z);
        }
    }
    {
        std::string tag = getTagChunk(xml, "projection");
        if (!tag.empty()) {
            extractAttrFloat(tag, "fov", gCam.fov);
            extractAttrFloat(tag, "near", gCam.nearp);
            extractAttrFloat(tag, "far", gCam.farp);
        }
    }
}
/*indica ao motor gráfico quais os ficheiros de modelos 3D a carregar, 
procurando por tags <model file="..."/> no XML*/
static std::vector<std::string> parseModelFiles(const std::string& xml) {
    std::vector<std::string> files;
    size_t pos = 0;
    while (true) {
        auto mpos = xml.find("<model", pos);
        if (mpos == std::string::npos) break;
        auto end = xml.find("/>", mpos);
        if (end == std::string::npos) break;
        std::string tag = xml.substr(mpos, end - mpos);

        std::string file;
        if (extractAttrStr(tag, "file", file)) files.push_back(file);

        pos = end + 2;
    }
    return files;
}
//abre o ficheiro do modelo 3D e carrega os vertices para a estrutura de dados do motor gráfico.
static void loadModel3D(Model& m) {
    std::ifstream in(m.file);
    if (!in.is_open()) {
        std::cerr << "Failed to open model file: " << m.file << "\n";
        std::exit(1);
    }

    int n = 0;
    in >> n;
    if (n <= 0) {
        std::cerr << "Invalid vertex count in: " << m.file << "\n";
        std::exit(1);
    }

    m.vertices.clear();
    m.vertices.reserve((size_t)n * 3);

    for (int i = 0; i < n; i++) {
        float x, y, z;
        in >> x >> y >> z;
        m.vertices.push_back(x);
        m.vertices.push_back(y);
        m.vertices.push_back(z);
    }

    std::cout << "Loaded " << n << " vertices from " << m.file << "\n";
}
//callback do glut para quando a janela é redimensionada,ajusta a projecao para evitar distorcoes.
static void changeSize(int w, int h) {
    if (h == 0) h = 1;
    float ratio = (float)w / (float)h;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, w, h);

    gluPerspective(gCam.fov, ratio, gCam.nearp, gCam.farp);

    glMatrixMode(GL_MODELVIEW);
}
/*limpa o ecrã, configura a câmera e desenha os modelos 3D usando 
glBegin(GL_TRIANGLES) e glVertex3f para cada vértice.*/
static void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(
        gCam.position.x, gCam.position.y, gCam.position.z,
        gCam.lookAt.x,   gCam.lookAt.y,   gCam.lookAt.z,
        gCam.up.x,       gCam.up.y,       gCam.up.z
    );

    drawAxes();

    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_TRIANGLES);
    for (const auto& m : gModels) {
        for (size_t i = 0; i + 2 < m.vertices.size(); i += 3) {
            glVertex3f(m.vertices[i], m.vertices[i + 1], m.vertices[i + 2]);
        }
    }
    glEnd();

    glutSwapBuffers();
}
//inicializa o open gl
static void initGL() {
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe para veres bem os triângulos
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: engine <scene.xml>\n";
        return 1;
    }

    std::string xml = readFileToString(argv[1]);

    parseWindow(xml);
    parseCamera(xml);

    auto files = parseModelFiles(xml);
    if (files.empty()) {
        std::cerr << "No <model file=\"...\"/> found in XML.\n";
        return 1;
    }

    gModels.clear();
    gModels.reserve(files.size());
    for (const auto& f : files) {
        Model m;
        m.file = f;
        loadModel3D(m);
        gModels.push_back(std::move(m));
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(gWinW, gWinH);
    glutCreateWindow("CG Engine - Phase 1");

    initGL();

    glutReshapeFunc(changeSize);
    glutDisplayFunc(renderScene);
    glutIdleFunc(renderScene);

    glutMainLoop();
    return 0;
}
