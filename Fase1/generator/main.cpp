#include <iostream>
#include <fstream>
#include <vector>
#include <string>

struct Point {
    float x, y, z;
};

void saveToFile(std::string filename, const std::vector<Point>& points) {
    std::ofstream file("../models/" + filename);
    if (file.is_open()) {
        file << points.size() << "\n";
        for (const auto& p : points) {
            file << p.x << " " << p.y << " " << p.z << "\n";
        }
        file.close();
    }
}

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

            // Triângulo 1
            points.push_back({x1, 0, z1});
            points.push_back({x1, 0, z2});
            points.push_back({x2, 0, z2});

            // Triângulo 2
            points.push_back({x1, 0, z1});
            points.push_back({x2, 0, z2});
            points.push_back({x2, 0, z1});
        }
    }
    saveToFile(filename, points);
}

int main(int argc, char** argv) {
    std::string primitive = argv[1];
    if (primitive == "plane") {
        createPlane(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    }
    // Outras primitivas virão aqui...
    return 0;
}