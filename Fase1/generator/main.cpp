#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>

struct Point {
    float x, y, z;
};

// Função para guardar os pontos no ficheiro .3d
void saveToFile(std::string filename, const std::vector<Point>& points) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << points.size() << "\n";
        for (const auto& p : points) {
            file << p.x << " " << p.y << " " << p.z << "\n";
        }
        file.close();
        std::cout << "Gerado: " << filename << " (" << points.size() << " vertices)" << std::endl;
    } else {
        std::cerr << "Erro ao abrir ficheiro!" << std::endl;
    }
}

// --- PLANE ---
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

            points.push_back({x1, 0, z1}); points.push_back({x1, 0, z2}); points.push_back({x2, 0, z2});
            points.push_back({x1, 0, z1}); points.push_back({x2, 0, z2}); points.push_back({x2, 0, z1});
        }
    }
    saveToFile(filename, points);
}

// --- BOX ---
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

            // Topo e Base (Y fixo)
            points.push_back({a,  fixed, b});  points.push_back({a,  fixed, nb}); points.push_back({na, fixed, nb});
            points.push_back({a,  fixed, b});  points.push_back({na, fixed, nb}); points.push_back({na, fixed, b});
            points.push_back({a, -fixed, b});  points.push_back({na, -fixed, nb}); points.push_back({a, -fixed, nb});
            points.push_back({a, -fixed, b});  points.push_back({na, -fixed, b});  points.push_back({na, -fixed, nb});

            // Frente e Tras (Z fixo)
            points.push_back({a, b,  fixed});  points.push_back({na, b,  fixed});  points.push_back({na, nb, fixed});
            points.push_back({a, b,  fixed});  points.push_back({na, nb, fixed});  points.push_back({a,  nb, fixed});
            points.push_back({a, b, -fixed});  points.push_back({a,  nb, -fixed}); points.push_back({na, nb, -fixed});
            points.push_back({a, b, -fixed});  points.push_back({na, nb, -fixed}); points.push_back({na, b, -fixed});

            // Laterais (X fixo)
            points.push_back({ fixed, a, b});  points.push_back({ fixed, na, nb}); points.push_back({ fixed, a, nb});
            points.push_back({ fixed, a, b});  points.push_back({ fixed, na, b});  points.push_back({ fixed, na, nb});
            points.push_back({-fixed, a, b});  points.push_back({-fixed, a, nb});  points.push_back({-fixed, na, nb});
            points.push_back({-fixed, a, b});  points.push_back({-fixed, na, nb}); points.push_back({-fixed, na, b});
        }
    }
    saveToFile(filename, points);
}

// --- SPHERE ---
void createSphere(float radius, int slices, int stacks, std::string filename) {
    std::vector<Point> points;
    float alphaStep = 2.0f * M_PI / slices;
    float betaStep = M_PI / stacks;

    for (int i = 0; i < slices; i++) {
        for (int j = 0; j < stacks; j++) {
            float a1 = i * alphaStep; float a2 = (i + 1) * alphaStep;
            float b1 = -M_PI/2.0f + j * betaStep; float b2 = -M_PI/2.0f + (j + 1) * betaStep;

            Point p1 = {radius*cos(b1)*sin(a1), radius*sin(b1), radius*cos(b1)*cos(a1)};
            Point p2 = {radius*cos(b1)*sin(a2), radius*sin(b1), radius*cos(b1)*cos(a2)};
            Point p3 = {radius*cos(b2)*sin(a1), radius*sin(b2), radius*cos(b2)*cos(a1)};
            Point p4 = {radius*cos(b2)*sin(a2), radius*sin(b2), radius*cos(b2)*cos(a2)};

            points.push_back(p1); points.push_back(p2); points.push_back(p4);
            points.push_back(p1); points.push_back(p4); points.push_back(p3);
        }
    }
    saveToFile(filename, points);
}

// --- CONE ---
void createCone(float radius, float height, int slices, int stacks, std::string filename) {
    std::vector<Point> points;
    float alphaStep = 2.0f * M_PI / slices;
    float hStep = height / stacks;
    float rStep = radius / stacks;

    for (int i = 0; i < slices; i++) {
        float a1 = i * alphaStep;
        float a2 = (i + 1) * alphaStep;

        // Base (No plano XZ, y=0)
        points.push_back({0, 0, 0});
        points.push_back({radius * sin(a2), 0, radius * cos(a2)});
        points.push_back({radius * sin(a1), 0, radius * cos(a1)});

        // Corpo Lateral (por camadas/stacks)
        for (int j = 0; j < stacks; j++) {
            float h1 = j * hStep;
            float h2 = (j + 1) * hStep;
            float r1 = radius - j * rStep;
            float r2 = radius - (j + 1) * rStep;

            Point p1 = {r1 * sin(a1), h1, r1 * cos(a1)};
            Point p2 = {r1 * sin(a2), h1, r1 * cos(a2)};
            Point p3 = {r2 * sin(a1), h2, r2 * cos(a1)};
            Point p4 = {r2 * sin(a2), h2, r2 * cos(a2)};

            points.push_back(p1); points.push_back(p2); points.push_back(p4);
            points.push_back(p1); points.push_back(p4); points.push_back(p3);
        }
    }
    saveToFile(filename, points);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::string p = argv[1];
    if (p == "plane") createPlane(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    else if (p == "box") createBox(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    else if (p == "sphere") createSphere(std::stof(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]), argv[5]);
    else if (p == "cone") createCone(std::stof(argv[2]), std::stof(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), argv[6]);
    return 0;
}