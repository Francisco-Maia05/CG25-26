#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>

struct Point {
    float x, y, z;
};

// Função para guardar os pontos no ficheiro .3d
void saveToFile(std::string filename, const std::vector<Point>& points) {
    // Tenta guardar na pasta atual. Se quiseres numa pasta específica, ajusta o caminho.
    std::ofstream file(filename);
    if (file.is_open()) {
        file << points.size() << "\n";
        for (const auto& p : points) {
            file << p.x << " " << p.y << " " << p.z << "\n";
        }
        file.close();
        std::cout << "Ficheiro " << filename << " gerado com sucesso (" << points.size() << " vertices)." << std::endl;
    } else {
        std::cerr << "Erro ao abrir o ficheiro!" << std::endl;
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

// --- BOX ---
void createBox(float units_x, float units_y, float units_z, int divisions, std::string filename) {
    std::vector<Point> points;
    float dx = units_x / divisions;
    float dy = units_y / divisions;
    float dz = units_z / divisions;

    float x_start = -units_x / 2.0f;
    float y_start = -units_y / 2.0f;
    float z_start = -units_z / 2.0f;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float x = x_start + i * dx;
            float y = y_start + i * dy;
            float z = z_start + j * dz;
            
            float nx = x + dx;
            float ny = y + dy;
            float nz = z + dz;

            // Face Topo (Y positivo)
            points.push_back({x,  units_y/2, z});  points.push_back({x,  units_y/2, nz}); points.push_back({nx, units_y/2, nz});
            points.push_back({x,  units_y/2, z});  points.push_back({nx, units_y/2, nz}); points.push_back({nx, units_y/2, z});

            // Face Base (Y negativo)
            points.push_back({x, -units_y/2, z});  points.push_back({nx, -units_y/2, nz}); points.push_back({x, -units_y/2, nz});
            points.push_back({x, -units_y/2, z});  points.push_back({nx, -units_y/2, z});  points.push_back({nx, -units_y/2, nz});

            // Face Frente (Z positivo)
            float fx = x_start + i * dx;
            float fy = y_start + j * dy;
            float nfx = fx + dx;
            float nfy = fy + dy;
            points.push_back({fx,  fy,  units_z/2}); points.push_back({nfx, fy,  units_z/2}); points.push_back({nfx, nfy, units_z/2});
            points.push_back({fx,  fy,  units_z/2}); points.push_back({nfx, nfy, units_z/2}); points.push_back({fx,  nfy, units_z/2});

            // Face Trás (Z negativo)
            points.push_back({fx,  fy, -units_z/2}); points.push_back({fx,  nfy, -units_z/2}); points.push_back({nfx, nfy, -units_z/2});
            points.push_back({fx,  fy, -units_z/2}); points.push_back({nfx, nfy, -units_z/2}); points.push_back({nfx, fy,  -units_z/2});

            // Face Direita (X positivo)
            float rz = z_start + i * dz;
            float ry = y_start + j * dy;
            float nrz = rz + dz;
            float nry = ry + dy;
            points.push_back({units_x/2, ry,  rz});  points.push_back({units_x/2, nry, nrz}); points.push_back({units_x/2, ry,  nrz});
            points.push_back({units_x/2, ry,  rz});  points.push_back({units_x/2, nry, rz});  points.push_back({units_x/2, nry, nrz});

            // Face Esquerda (X negativo)
            points.push_back({-units_x/2, ry,  rz});  points.push_back({-units_x/2, ry,  nrz}); points.push_back({-units_x/2, nry, nrz});
            points.push_back({-units_x/2, ry,  rz});  points.push_back({-units_x/2, nry, nrz}); points.push_back({-units_x/2, nry, rz});
        }
    }
    saveToFile(filename, points);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Uso: ./generator <primitiva> <params> <ficheiro.3d>" << std::endl;
        return 1;
    }

    std::string primitive = argv[1];

    if (primitive == "plane" && argc == 5) {
        createPlane(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    } 
    else if (primitive == "box" && argc == 7) {
        // box <x> <y> <z> <divs> <file>
        createBox(std::stof(argv[2]), std::stof(argv[3]), std::stof(argv[4]), std::stoi(argv[5]), argv[6]);
    }
    else {
        std::cout << "Primitiva invalida ou numero de argumentos incorreto." << std::endl;
    }

    return 0;
}