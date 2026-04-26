#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>

struct Point {
    float x, y, z;
};

void saveToFile(std::string filename, const std::vector<Point>& points) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << points.size() << "\n";
        for (const auto& p : points) {
            file << p.x << " " << p.y << " " << p.z << "\n";
        }
        file.close();
        std::cout << "Gerado: " << filename << std::endl;
    }
}

// ---------- PLANE ----------
void createPlane(float size, int divisions, std::string filename) {
    std::vector<Point> points;
    float step = size / divisions;
    float start = -size / 2;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float x = start + i * step;
            float z = start + j * step;

            points.push_back({x, 0, z});
            points.push_back({x, 0, z + step});
            points.push_back({x + step, 0, z + step});

            points.push_back({x, 0, z});
            points.push_back({x + step, 0, z + step});
            points.push_back({x + step, 0, z});
        }
    }
    saveToFile(filename, points);
}

// ---------- BOX ----------
void createBox(float size, int divisions, std::string filename) {
    std::vector<Point> points;
    float step = size / divisions;
    float start = -size / 2;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float x = start + i * step;
            float y = start + j * step;

            float nx = x + step;
            float ny = y + step;
            float s = size / 2;

            // TOP
            points.push_back({x, s, y});
            points.push_back({nx, s, y});
            points.push_back({nx, s, ny});

            points.push_back({x, s, y});
            points.push_back({nx, s, ny});
            points.push_back({x, s, ny});
        }
    }
    saveToFile(filename, points);
}

// ---------- SPHERE ----------
void createSphere(float radius, int slices, int stacks, std::string filename) {
    std::vector<Point> points;

    float alphaStep = 2 * M_PI / slices;
    float betaStep = M_PI / stacks;

    for (int i = 0; i < slices; i++) {
        for (int j = 0; j < stacks; j++) {

            float a1 = i * alphaStep;
            float a2 = (i + 1) * alphaStep;

            float b1 = j * betaStep - M_PI/2;
            float b2 = (j + 1) * betaStep - M_PI/2;

            Point p1 = {radius * cos(b1) * sin(a1), radius * sin(b1), radius * cos(b1) * cos(a1)};
            Point p2 = {radius * cos(b1) * sin(a2), radius * sin(b1), radius * cos(b1) * cos(a2)};
            Point p3 = {radius * cos(b2) * sin(a1), radius * sin(b2), radius * cos(b2) * cos(a1)};
            Point p4 = {radius * cos(b2) * sin(a2), radius * sin(b2), radius * cos(b2) * cos(a2)};

            points.push_back(p1);
            points.push_back(p2);
            points.push_back(p4);

            points.push_back(p1);
            points.push_back(p4);
            points.push_back(p3);
        }
    }

    saveToFile(filename, points);
}

// ---------- CONE ----------
void createCone(float radius, float height, int slices, int stacks, std::string filename) {
    std::vector<Point> points;

    float step = 2 * M_PI / slices;

    for (int i = 0; i < slices; i++) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        // base
        points.push_back({0, 0, 0});
        points.push_back({radius * sin(a2), 0, radius * cos(a2)});
        points.push_back({radius * sin(a1), 0, radius * cos(a1)});
    }

    saveToFile(filename, points);
}

// ---------- RING (NOVO) ----------
void createRing(float innerRadius, float outerRadius, int slices, std::string filename) {
    std::vector<Point> points;

    float step = 2.0f * M_PI / slices;

    for (int i = 0; i < slices; i++) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        Point p1 = { innerRadius * sinf(a1), 0, innerRadius * cosf(a1) };
        Point p2 = { outerRadius * sinf(a1), 0, outerRadius * cosf(a1) };
        Point p3 = { outerRadius * sinf(a2), 0, outerRadius * cosf(a2) };
        Point p4 = { innerRadius * sinf(a2), 0, innerRadius * cosf(a2) };

        points.push_back(p1);
        points.push_back(p2);
        points.push_back(p3);

        points.push_back(p1);
        points.push_back(p3);
        points.push_back(p4);
    }

    saveToFile(filename, points);
}

// ---------- MAIN ----------
int main(int argc, char** argv) {

    if (argc < 2) {
        std::cout << "Uso:\n";
        std::cout << "plane size divisions file\n";
        std::cout << "box size divisions file\n";
        std::cout << "sphere radius slices stacks file\n";
        std::cout << "cone radius height slices stacks file\n";
        std::cout << "ring inner outer slices file\n";
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "plane")
        createPlane(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);

    else if (cmd == "box")
        createBox(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);

    else if (cmd == "sphere")
        createSphere(std::stof(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]), argv[5]);

    else if (cmd == "cone")
        createCone(std::stof(argv[2]), std::stof(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), argv[6]);

    else if (cmd == "ring")
        createRing(std::stof(argv[2]), std::stof(argv[3]), std::stoi(argv[4]), argv[5]);

    else
        std::cout << "Comando invalido\n";

    return 0;
}