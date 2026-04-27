#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

// ---------------------------------------------------------------------------
// DATA TYPES
// ---------------------------------------------------------------------------

struct Point {
    float x, y, z;
};

// Stores the 16 control-point indices of one Bezier patch
struct Patch {
    std::vector<int> indices;  // always 16 entries
};

// ---------------------------------------------------------------------------
// FILE I/O  – writes <N>\n then N lines of "x y z"
// ---------------------------------------------------------------------------

void saveToFile(const std::string& filename, const std::vector<Point>& points) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ficheiro para escrita: " << filename << std::endl;
        return;
    }
    file << points.size() << "\n";
    for (const auto& p : points)
        file << p.x << " " << p.y << " " << p.z << "\n";
    file.close();
    std::cout << "Gerado: " << filename << " (" << points.size() << " vertices)" << std::endl;
}

// ---------------------------------------------------------------------------
// BEZIER PATCH
// ---------------------------------------------------------------------------

// Bernstein basis polynomials of degree 3
static void bernstein3(float t, float b[4]) {
    float tm = 1.0f - t;
    b[0] = tm * tm * tm;
    b[1] = 3.0f * t * tm * tm;
    b[2] = 3.0f * t * t * tm;
    b[3] = t * t * t;
}

Point getBezierPoint(float u, float v,
                     const std::vector<Point>& cp,
                     const Patch& patch)
{
    float bu[4], bv[4];
    bernstein3(u, bu);
    bernstein3(v, bv);

    Point p = {0, 0, 0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const Point& c = cp[patch.indices[i * 4 + j]];
            float w = bu[i] * bv[j];
            p.x += c.x * w;
            p.y += c.y * w;
            p.z += c.z * w;
        }
    }
    return p;
}

// Helper: strip all commas from a token so we can parse "1,2,3" safely
static float parseFloat(const std::string& s) {
    std::string clean;
    for (char c : s) if (c != ',') clean += c;
    return std::stof(clean);
}

void createBezier(const std::string& patchFile,
                  int tessellation,
                  const std::string& outputFile)
{
    std::ifstream file(patchFile);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ficheiro de patch: " << patchFile << std::endl;
        return;
    }

    // ---- 1. Read patch count ----
    int numPatches = 0;
    file >> numPatches;
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::vector<Patch> patches(numPatches);
    for (int i = 0; i < numPatches; i++) {
        std::string line;
        std::getline(file, line);
        // Replace commas with spaces for easy tokenisation
        for (char& c : line) if (c == ',') c = ' ';
        std::istringstream ss(line);
        int idx;
        while (ss >> idx) patches[i].indices.push_back(idx);
        // Validate
        if ((int)patches[i].indices.size() != 16) {
            std::cerr << "Aviso: patch " << i << " tem "
                      << patches[i].indices.size() << " indices (esperado 16)." << std::endl;
        }
    }

    // ---- 2. Read control points ----
    int numCP = 0;
    file >> numCP;
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::vector<Point> controlPoints(numCP);
    for (int i = 0; i < numCP; i++) {
        std::string line;
        std::getline(file, line);
        for (char& c : line) if (c == ',') c = ' ';
        std::istringstream ss(line);
        ss >> controlPoints[i].x >> controlPoints[i].y >> controlPoints[i].z;
    }

    // ---- 3. Tessellate ----
    std::vector<Point> vertices;
    float step = 1.0f / tessellation;

    for (const auto& patch : patches) {
        for (int i = 0; i < tessellation; i++) {
            for (int j = 0; j < tessellation; j++) {
                float u  = i * step,       v  = j * step;
                float un = (i + 1) * step, vn = (j + 1) * step;

                Point p00 = getBezierPoint(u,  v,  controlPoints, patch);
                Point p10 = getBezierPoint(un, v,  controlPoints, patch);
                Point p01 = getBezierPoint(u,  vn, controlPoints, patch);
                Point p11 = getBezierPoint(un, vn, controlPoints, patch);

                // Triangle 1
                vertices.push_back(p00);
                vertices.push_back(p10);
                vertices.push_back(p11);

                // Triangle 2
                vertices.push_back(p00);
                vertices.push_back(p11);
                vertices.push_back(p01);
            }
        }
    }

    saveToFile(outputFile, vertices);
}

// ---------------------------------------------------------------------------
// PRIMITIVES
// ---------------------------------------------------------------------------

void createPlane(float size, int divisions, const std::string& filename) {
    std::vector<Point> points;
    float start = -size / 2.0f;
    float step  =  size / divisions;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float x1 = start + i * step, z1 = start + j * step;
            float x2 = x1 + step,        z2 = z1 + step;

            points.push_back({x1, 0, z1});
            points.push_back({x1, 0, z2});
            points.push_back({x2, 0, z2});

            points.push_back({x1, 0, z1});
            points.push_back({x2, 0, z2});
            points.push_back({x2, 0, z1});
        }
    }
    saveToFile(filename, points);
}

void createBox(float size, int divisions, const std::string& filename) {
    std::vector<Point> points;
    float step  = size / divisions;
    float start = -size / 2.0f;
    float half  =  size / 2.0f;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float a = start + i * step, b = start + j * step;
            float na = a + step,        nb = b + step;

            // Top (+Y)
            points.push_back({a,  half, b });  points.push_back({a,  half, nb}); points.push_back({na, half, nb});
            points.push_back({a,  half, b });  points.push_back({na, half, nb}); points.push_back({na, half, b });
            // Bottom (-Y)
            points.push_back({a, -half, b });  points.push_back({na,-half, nb}); points.push_back({a, -half, nb});
            points.push_back({a, -half, b });  points.push_back({na,-half, b }); points.push_back({na,-half, nb});
            // Front (+Z)
            points.push_back({a,  b,  half}); points.push_back({na, b,  half}); points.push_back({na, nb, half});
            points.push_back({a,  b,  half}); points.push_back({na, nb, half}); points.push_back({a,  nb, half});
            // Back (-Z)
            points.push_back({a,  b, -half}); points.push_back({a,  nb,-half}); points.push_back({na, nb,-half});
            points.push_back({a,  b, -half}); points.push_back({na, nb,-half}); points.push_back({na, b, -half});
            // Right (+X)
            points.push_back({half, a,  b }); points.push_back({half, na, nb}); points.push_back({half, a,  nb});
            points.push_back({half, a,  b }); points.push_back({half, na, b }); points.push_back({half, na, nb});
            // Left (-X)
            points.push_back({-half, a,  b }); points.push_back({-half, a,  nb}); points.push_back({-half, na, nb});
            points.push_back({-half, a,  b }); points.push_back({-half, na, nb}); points.push_back({-half, na, b });
        }
    }
    saveToFile(filename, points);
}

void createSphere(float radius, int slices, int stacks, const std::string& filename) {
    std::vector<Point> points;
    float aStep = 2.0f * (float)M_PI / slices;
    float bStep = (float)M_PI / stacks;

    for (int i = 0; i < slices; i++) {
        for (int j = 0; j < stacks; j++) {
            float a1 = i * aStep,       a2 = (i + 1) * aStep;
            float b1 = -(float)M_PI / 2.0f + j * bStep;
            float b2 = b1 + bStep;

            Point p1 = { radius*cosf(b1)*sinf(a1), radius*sinf(b1), radius*cosf(b1)*cosf(a1) };
            Point p2 = { radius*cosf(b1)*sinf(a2), radius*sinf(b1), radius*cosf(b1)*cosf(a2) };
            Point p3 = { radius*cosf(b2)*sinf(a1), radius*sinf(b2), radius*cosf(b2)*cosf(a1) };
            Point p4 = { radius*cosf(b2)*sinf(a2), radius*sinf(b2), radius*cosf(b2)*cosf(a2) };

            points.push_back(p1); points.push_back(p2); points.push_back(p4);
            points.push_back(p1); points.push_back(p4); points.push_back(p3);
        }
    }
    saveToFile(filename, points);
}

void createCone(float radius, float height, int slices, int stacks,
                const std::string& filename)
{
    std::vector<Point> points;
    float aStep = 2.0f * (float)M_PI / slices;
    float hStep = height / stacks;
    float rStep = radius  / stacks;

    for (int i = 0; i < slices; i++) {
        float a1 = i * aStep, a2 = (i + 1) * aStep;

        // Base disc
        points.push_back({0, 0, 0});
        points.push_back({radius*sinf(a2), 0, radius*cosf(a2)});
        points.push_back({radius*sinf(a1), 0, radius*cosf(a1)});

        for (int j = 0; j < stacks; j++) {
            float h1 = j * hStep, h2 = (j + 1) * hStep;
            float r1 = radius - j * rStep, r2 = radius - (j + 1) * rStep;

            Point p1 = {r1*sinf(a1), h1, r1*cosf(a1)};
            Point p2 = {r1*sinf(a2), h1, r1*cosf(a2)};
            Point p3 = {r2*sinf(a1), h2, r2*cosf(a1)};
            Point p4 = {r2*sinf(a2), h2, r2*cosf(a2)};

            points.push_back(p1); points.push_back(p2); points.push_back(p4);
            points.push_back(p1); points.push_back(p4); points.push_back(p3);
        }
    }
    saveToFile(filename, points);
}

// ---------------------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso:\n"
                  << "  generator plane  <size> <divisions> <file>\n"
                  << "  generator box    <size> <divisions> <file>\n"
                  << "  generator sphere <radius> <slices> <stacks> <file>\n"
                  << "  generator cone   <radius> <height> <slices> <stacks> <file>\n"
                  << "  generator patch  <patchFile> <tessellation> <file>\n";
        return 1;
    }

    std::string cmd = argv[1];

    if      (cmd == "plane"  && argc == 5)
        createPlane(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    else if (cmd == "box"    && argc == 5)
        createBox(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    else if (cmd == "sphere" && argc == 6)
        createSphere(std::stof(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]), argv[5]);
    else if (cmd == "cone"   && argc == 7)
        createCone(std::stof(argv[2]), std::stof(argv[3]),
                   std::stoi(argv[4]), std::stoi(argv[5]), argv[6]);
    else if (cmd == "patch"  && argc == 5)
        createBezier(argv[2], std::stoi(argv[3]), argv[4]);
    else {
        std::cerr << "Comando invalido ou numero de argumentos incorreto.\n";
        return 1;
    }

    return 0;
}