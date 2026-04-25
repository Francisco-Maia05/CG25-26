#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

struct Point {
    float x, y, z;
};

// Estrutura auxiliar para organizar os índices de cada patch de Bezier
struct Patch {
    std::vector<int> indices;
};

// Função para guardar os pontos no ficheiro .3d (formato: nº de vértices seguido das coordenadas)
void saveToFile(std::string filename, const std::vector<Point>& points) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << points.size() << "\n";
        for (const auto& p : points) {
            file << p.x << " " << p.y << " " << p.z << "\n";
        }
        file.close();
        std::cout << "Gerado: " << filename << " (" << points.size() << " vertices)" << std::endl;
    }
    else {
        std::cerr << "Erro ao abrir ficheiro para escrita: " << filename << std::endl;
    }
}

// --- BEZIER PATCH LOGIC ---

// Calcula um ponto na superfície de Bezier para parâmetros (u, v)
Point getBezierPoint(float u, float v, const std::vector<Point>& controlPoints, const Patch& patch) {
    Point p = { 0, 0, 0 };
    float bu[4], bv[4];

    // Polinómios de Bernstein (Grau 3)
    bu[0] = powf(1 - u, 3);
    bu[1] = 3 * u * powf(1 - u, 2);
    bu[2] = 3 * u * u * (1 - u);
    bu[3] = powf(u, 3);

    bv[0] = powf(1 - v, 3);
    bv[1] = 3 * v * powf(1 - v, 2);
    bv[2] = 3 * v * v * (1 - v);
    bv[3] = powf(v, 3);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            // O patch tem 16 índices (4x4)
            Point cp = controlPoints[patch.indices[i * 4 + j]];
            p.x += cp.x * bu[i] * bv[j];
            p.y += cp.y * bu[i] * bv[j];
            p.z += cp.z * bu[i] * bv[j];
        }
    }
    return p;
}

void createBezier(std::string patchFile, int tessellation, std::string outputFile) {
    std::ifstream file(patchFile);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ficheiro de patch: " << patchFile << std::endl;
        return;
    }

    // 1. Ler Índices dos Patches
    int numPatches;
    file >> numPatches;
    std::vector<Patch> patches(numPatches);
    for (int i = 0; i < numPatches; i++) {
        for (int j = 0; j < 16; j++) {
            int idx;
            file >> idx;
            patches[i].indices.push_back(idx);
            if (j < 15) { char comma; file >> comma; } // Consumir a vírgula
        }
    }

    // 2. Ler Pontos de Controlo
    int numControlPoints;
    file >> numControlPoints;
    std::vector<Point> controlPoints(numControlPoints);
    for (int i = 0; i < numControlPoints; i++) {
        file >> controlPoints[i].x;
        if (file.peek() == ',') file.ignore();
        file >> controlPoints[i].y;
        if (file.peek() == ',') file.ignore();
        file >> controlPoints[i].z;
    }

    // 3. Gerar Vértices com base no tessellation
    std::vector<Point> vertices;
    float step = 1.0f / tessellation;

    for (const auto& patch : patches) {
        for (int i = 0; i < tessellation; i++) {
            for (int j = 0; j < tessellation; j++) {
                float u = i * step;
                float v = j * step;
                float un = (i + 1) * step;
                float vn = (j + 1) * step;

                // Triângulo 1
                vertices.push_back(getBezierPoint(u, v, controlPoints, patch));
                vertices.push_back(getBezierPoint(un, v, controlPoints, patch));
                vertices.push_back(getBezierPoint(un, vn, controlPoints, patch));

                // Triângulo 2
                vertices.push_back(getBezierPoint(u, v, controlPoints, patch));
                vertices.push_back(getBezierPoint(un, vn, controlPoints, patch));
                vertices.push_back(getBezierPoint(u, vn, controlPoints, patch));
            }
        }
    }
    saveToFile(outputFile, vertices);
}

// --- PRIMITIVAS FASE 2 ---

void createPlane(float size, int divisions, std::string filename) {
    std::vector<Point> points;
    float start = -size / 2.0f;
    float step = size / divisions;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float x1 = start + i * step;
            float z1 = start + j * step;
            float x2 = x1 + step;
            float z2 = z1 + step;

            points.push_back({ x1, 0, z1 });
            points.push_back({ x1, 0, z2 });
            points.push_back({ x2, 0, z2 });

            points.push_back({ x1, 0, z1 });
            points.push_back({ x2, 0, z2 });
            points.push_back({ x2, 0, z1 });
        }
    }
    saveToFile(filename, points);
}

void createBox(float size, int divisions, std::string filename) {
    std::vector<Point> points;
    float step = size / divisions;
    float start = -size / 2.0f;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float a = start + i * step;
            float b = start + j * step;
            float na = a + step;
            float nb = b + step;
            float fixed = size / 2.0f;

            // Topo e Base
            points.push_back({ a, fixed, b }); points.push_back({ a, fixed, nb }); points.push_back({ na, fixed, nb });
            points.push_back({ a, fixed, b }); points.push_back({ na, fixed, nb }); points.push_back({ na, fixed, b });
            points.push_back({ a, -fixed, b }); points.push_back({ na, -fixed, nb }); points.push_back({ a, -fixed, nb });
            points.push_back({ a, -fixed, b }); points.push_back({ na, -fixed, b }); points.push_back({ na, -fixed, nb });

            // Frente e Tras
            points.push_back({ a, b, fixed }); points.push_back({ na, b, fixed }); points.push_back({ na, nb, fixed });
            points.push_back({ a, b, fixed }); points.push_back({ na, nb, fixed }); points.push_back({ a, nb, fixed });
            points.push_back({ a, b, -fixed }); points.push_back({ a, nb, -fixed }); points.push_back({ na, nb, -fixed });
            points.push_back({ a, b, -fixed }); points.push_back({ na, nb, -fixed }); points.push_back({ na, b, -fixed });

            // Lados
            points.push_back({ fixed, a, b }); points.push_back({ fixed, na, nb }); points.push_back({ fixed, a, nb });
            points.push_back({ fixed, a, b }); points.push_back({ fixed, na, b }); points.push_back({ fixed, na, nb });
            points.push_back({ -fixed, a, b }); points.push_back({ -fixed, a, nb }); points.push_back({ -fixed, na, nb });
            points.push_back({ -fixed, a, b }); points.push_back({ -fixed, na, nb }); points.push_back({ -fixed, na, b });
        }
    }
    saveToFile(filename, points);
}

void createSphere(float radius, int slices, int stacks, std::string filename) {
    std::vector<Point> points;
    float alphaStep = 2.0f * (float)M_PI / slices;
    float betaStep = (float)M_PI / stacks;

    for (int i = 0; i < slices; i++) {
        for (int j = 0; j < stacks; j++) {
            float a1 = i * alphaStep;
            float a2 = (i + 1) * alphaStep;
            float b1 = -(float)M_PI / 2.0f + j * betaStep;
            float b2 = -(float)M_PI / 2.0f + (j + 1) * betaStep;

            Point p1 = { radius * cosf(b1) * sinf(a1), radius * sinf(b1), radius * cosf(b1) * cosf(a1) };
            Point p2 = { radius * cosf(b1) * sinf(a2), radius * sinf(b1), radius * cosf(b1) * cosf(a2) };
            Point p3 = { radius * cosf(b2) * sinf(a1), radius * sinf(b2), radius * cosf(b2) * cosf(a1) };
            Point p4 = { radius * cosf(b2) * sinf(a2), radius * sinf(b2), radius * cosf(b2) * cosf(a2) };

            points.push_back(p1); points.push_back(p2); points.push_back(p4);
            points.push_back(p1); points.push_back(p4); points.push_back(p3);
        }
    }
    saveToFile(filename, points);
}

void createCone(float radius, float height, int slices, int stacks, std::string filename) {
    std::vector<Point> points;
    float alphaStep = 2.0f * (float)M_PI / slices;
    float hStep = height / stacks;
    float rStep = radius / stacks;

    for (int i = 0; i < slices; i++) {
        float a1 = i * alphaStep;
        float a2 = (i + 1) * alphaStep;

        // Base
        points.push_back({ 0, 0, 0 });
        points.push_back({ radius * sinf(a2), 0, radius * cosf(a2) });
        points.push_back({ radius * sinf(a1), 0, radius * cosf(a1) });

        for (int j = 0; j < stacks; j++) {
            float h1 = j * hStep;
            float h2 = (j + 1) * hStep;
            float r1 = radius - j * rStep;
            float r2 = radius - (j + 1) * rStep;

            Point p1 = { r1 * sinf(a1), h1, r1 * cosf(a1) };
            Point p2 = { r1 * sinf(a2), h1, r1 * cosf(a2) };
            Point p3 = { r2 * sinf(a1), h2, r2 * cosf(a1) };
            Point p4 = { r2 * sinf(a2), h2, r2 * cosf(a2) };

            points.push_back(p1); points.push_back(p2); points.push_back(p4);
            points.push_back(p1); points.push_back(p4); points.push_back(p3);
        }
    }
    saveToFile(filename, points);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso:" << std::endl;
        std::cerr << "  generator plane <size> <divisions> <file>" << std::endl;
        std::cerr << "  generator box <size> <divisions> <file>" << std::endl;
        std::cerr << "  generator sphere <radius> <slices> <stacks> <file>" << std::endl;
        std::cerr << "  generator cone <radius> <height> <slices> <stacks> <file>" << std::endl;
        std::cerr << "  generator patch <patchFile> <tessellation> <file>" << std::endl;
        return 1;
    }

    std::string p = argv[1];

    if (p == "plane" && argc == 5) {
        createPlane(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    }
    else if (p == "box" && argc == 5) {
        createBox(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    }
    else if (p == "sphere" && argc == 6) {
        createSphere(std::stof(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]), argv[5]);
    }
    else if (p == "cone" && argc == 7) {
        createCone(std::stof(argv[2]), std::stof(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), argv[6]);
    }
    else if (p == "patch" && argc == 5) {
        createBezier(argv[2], std::stoi(argv[3]), argv[4]);
    }
    else {
        std::cerr << "Comando invalido ou numero de argumentos incorreto." << std::endl;
        return 1;
    }

    return 0;
}