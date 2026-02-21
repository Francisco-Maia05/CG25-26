#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

using namespace std;

static void writeVertex(ofstream& out, float x, float y, float z) {
    out << fixed << setprecision(6) << x << " " << y << " " << z << "\n";
}

static void generatePlane(float length, int divisions, const string& filename) {
    if (divisions <= 0 || length <= 0.0f) {
        cerr << "Invalid plane params: length must be > 0 and divisions > 0\n";
        exit(1);
    }

    const float half = length / 2.0f;
    const float step = length / (float)divisions;

    // each cell -> 2 triangles -> 6 vertices
    const int vertexCount = divisions * divisions * 6;

    ofstream out(filename);
    if (!out.is_open()) {
        cerr << "Failed to open output file: " << filename << "\n";
        exit(1);
    }

    out << vertexCount << "\n";

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {

            float x = -half + i * step;
            float z = -half + j * step;

            // p0 (x,0,z), p1 (x+step,0,z), p2 (x+step,0,z+step), p3 (x,0,z+step)
            float x0 = x;
            float z0 = z;

            float x1 = x + step;
            float z1 = z;

            float x2 = x + step;
            float z2 = z + step;

            float x3 = x;
            float z3 = z + step;

            // triangle 1: p0 p1 p2
            writeVertex(out, x0, 0.0f, z0);
            writeVertex(out, x1, 0.0f, z1);
            writeVertex(out, x2, 0.0f, z2);

            // triangle 2: p0 p2 p3
            writeVertex(out, x0, 0.0f, z0);
            writeVertex(out, x2, 0.0f, z2);
            writeVertex(out, x3, 0.0f, z3);
        }
    }

    cout << "Generated plane: " << filename << " (" << vertexCount << " vertices)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage:\n"
             << "  generator plane <length> <divisions> <file>\n";
        return 1;
    }

    string primitive = argv[1];

    if (primitive == "plane") {
        if (argc != 5) {
            cerr << "Usage: generator plane <length> <divisions> <file>\n";
            return 1;
        }
        float length = stof(argv[2]);
        int divisions = stoi(argv[3]);
        string file = argv[4];

        generatePlane(length, divisions, file);
        return 0;
    }

    cerr << "Unknown primitive: " << primitive << "\n";
    return 1;
}