#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

struct Point {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct Patch {
    std::vector<int> indices;
};

void saveToFile(const std::string& filename, const std::vector<Point>& points) {
    std::ofstream file(filename);
    if (!file.is_open()) return;

    file << points.size() << "\n";

    for (const auto& p : points) {
        file << p.x << " " << p.y << " " << p.z << " "
             << p.nx << " " << p.ny << " " << p.nz << " "
             << p.u << " " << p.v << "\n";
    }

    file.close();
}

static void bernstein3(float t, float b[4]) {
    float tm = 1.0f - t;

    b[0] = tm * tm * tm;
    b[1] = 3.0f * t * tm * tm;
    b[2] = 3.0f * t * t * tm;
    b[3] = t * t * t;
}

static void bernstein3Deriv(float t, float d[4]) {
    float tm = 1.0f - t;

    d[0] = -3.0f * tm * tm;
    d[1] = 3.0f * tm * tm - 6.0f * t * tm;
    d[2] = 6.0f * t * tm - 3.0f * t * t;
    d[3] = 3.0f * t * t;
}

Point getBezierPoint(float u, float v, const std::vector<Point>& cp, const Patch& patch) {
    float bu[4], bv[4], dbu[4], dbv[4];

    bernstein3(u, bu);
    bernstein3(v, bv);
    bernstein3Deriv(u, dbu);
    bernstein3Deriv(v, dbv);

    Point p = {0, 0, 0, 0, 0, 0, u, v};

    float tx = 0, ty = 0, tz = 0;
    float bx = 0, by = 0, bz = 0;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const Point& c = cp[patch.indices[i * 4 + j]];

            p.x += c.x * bu[i] * bv[j];
            p.y += c.y * bu[i] * bv[j];
            p.z += c.z * bu[i] * bv[j];

            tx += c.x * dbu[i] * bv[j];
            ty += c.y * dbu[i] * bv[j];
            tz += c.z * dbu[i] * bv[j];

            bx += c.x * bu[i] * dbv[j];
            by += c.y * bu[i] * dbv[j];
            bz += c.z * bu[i] * dbv[j];
        }
    }

    float nx = ty * bz - tz * by;
    float ny = tz * bx - tx * bz;
    float nz = tx * by - ty * bx;

    float len = sqrt(nx * nx + ny * ny + nz * nz);

    if (len > 0) {
        p.nx = nx / len;
        p.ny = ny / len;
        p.nz = nz / len;
    }

    return p;
}

void createPlane(float size, int divisions, const std::string& filename) {
    std::vector<Point> points;

    float start = -size / 2.0f;
    float step = size / divisions;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float x1 = start + i * step;
            float z1 = start + j * step;
            float x2 = x1 + step;
            float z2 = z1 + step;

            float u1 = (float)i / divisions;
            float v1 = (float)j / divisions;
            float u2 = (float)(i + 1) / divisions;
            float v2 = (float)(j + 1) / divisions;

            points.push_back({x1, 0, z1, 0, 1, 0, u1, v1});
            points.push_back({x1, 0, z2, 0, 1, 0, u1, v2});
            points.push_back({x2, 0, z2, 0, 1, 0, u2, v2});

            points.push_back({x1, 0, z1, 0, 1, 0, u1, v1});
            points.push_back({x2, 0, z2, 0, 1, 0, u2, v2});
            points.push_back({x2, 0, z1, 0, 1, 0, u2, v1});
        }
    }

    saveToFile(filename, points);
}

void createBox(float size, int divisions, const std::string& filename) {
    std::vector<Point> pts;

    float step = size / divisions;
    float start = -size / 2.0f;
    float h = size / 2.0f;

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            float a = start + i * step;
            float b = start + j * step;
            float na = a + step;
            float nb = b + step;

            float u1 = (float)i / divisions;
            float v1 = (float)j / divisions;
            float u2 = (float)(i + 1) / divisions;
            float v2 = (float)(j + 1) / divisions;

            pts.push_back({a, h, b, 0, 1, 0, u1, v1});
            pts.push_back({a, h, nb, 0, 1, 0, u1, v2});
            pts.push_back({na, h, nb, 0, 1, 0, u2, v2});

            pts.push_back({a, h, b, 0, 1, 0, u1, v1});
            pts.push_back({na, h, nb, 0, 1, 0, u2, v2});
            pts.push_back({na, h, b, 0, 1, 0, u2, v1});

            pts.push_back({a, -h, b, 0, -1, 0, u1, v1});
            pts.push_back({na, -h, nb, 0, -1, 0, u2, v2});
            pts.push_back({a, -h, nb, 0, -1, 0, u1, v2});

            pts.push_back({a, -h, b, 0, -1, 0, u1, v1});
            pts.push_back({na, -h, b, 0, -1, 0, u2, v1});
            pts.push_back({na, -h, nb, 0, -1, 0, u2, v2});

            pts.push_back({a, b, h, 0, 0, 1, u1, v1});
            pts.push_back({na, b, h, 0, 0, 1, u2, v1});
            pts.push_back({na, nb, h, 0, 0, 1, u2, v2});

            pts.push_back({a, b, h, 0, 0, 1, u1, v1});
            pts.push_back({na, nb, h, 0, 0, 1, u2, v2});
            pts.push_back({a, nb, h, 0, 0, 1, u1, v2});

            pts.push_back({a, b, -h, 0, 0, -1, u1, v1});
            pts.push_back({a, nb, -h, 0, 0, -1, u1, v2});
            pts.push_back({na, nb, -h, 0, 0, -1, u2, v2});

            pts.push_back({a, b, -h, 0, 0, -1, u1, v1});
            pts.push_back({na, nb, -h, 0, 0, -1, u2, v2});
            pts.push_back({na, b, -h, 0, 0, -1, u2, v1});

            pts.push_back({h, a, b, 1, 0, 0, u1, v1});
            pts.push_back({h, na, b, 1, 0, 0, u2, v1});
            pts.push_back({h, na, nb, 1, 0, 0, u2, v2});

            pts.push_back({h, a, b, 1, 0, 0, u1, v1});
            pts.push_back({h, na, nb, 1, 0, 0, u2, v2});
            pts.push_back({h, a, nb, 1, 0, 0, u1, v2});

            pts.push_back({-h, a, b, -1, 0, 0, u1, v1});
            pts.push_back({-h, a, nb, -1, 0, 0, u1, v2});
            pts.push_back({-h, na, nb, -1, 0, 0, u2, v2});

            pts.push_back({-h, a, b, -1, 0, 0, u1, v1});
            pts.push_back({-h, na, nb, -1, 0, 0, u2, v2});
            pts.push_back({-h, na, b, -1, 0, 0, u2, v1});
        }
    }

    saveToFile(filename, pts);
}

void createSphere(float r, int sl, int st, const std::string& f) {
    std::vector<Point> pts;

    float aS = 2.0f * (float)M_PI / sl;
    float bS = (float)M_PI / st;

    for (int i = 0; i < sl; i++) {
        for (int j = 0; j < st; j++) {
            float a1 = i * aS;
            float a2 = (i + 1) * aS;

            float b1 = -(float)M_PI / 2 + j * bS;
            float b2 = b1 + bS;

            auto gP = [&](float a, float b) {
                float px = r * cos(b) * sin(a);
                float py = r * sin(b);
                float pz = r * cos(b) * cos(a);

                return Point {
                    px, py, pz,
                    px / r, py / r, pz / r,
                    a / (2.0f * (float)M_PI),
                    (b + (float)M_PI / 2) / (float)M_PI
                };
            };

            pts.push_back(gP(a1, b1));
            pts.push_back(gP(a2, b1));
            pts.push_back(gP(a2, b2));

            pts.push_back(gP(a1, b1));
            pts.push_back(gP(a2, b2));
            pts.push_back(gP(a1, b2));
        }
    }

    saveToFile(f, pts);
}

void createCone(float r, float h, int sl, int st, const std::string& f) {
    std::vector<Point> pts;

    float aS = 2.0f * (float)M_PI / sl;
    float hS = h / st;
    float rS = r / st;

    for (int i = 0; i < sl; i++) {
        float a1 = i * aS;
        float a2 = (i + 1) * aS;

        pts.push_back({0, 0, 0, 0, -1, 0, 0.5f, 0.5f});
        pts.push_back({r * sin(a2), 0, r * cos(a2), 0, -1, 0, (sin(a2) + 1) / 2, (cos(a2) + 1) / 2});
        pts.push_back({r * sin(a1), 0, r * cos(a1), 0, -1, 0, (sin(a1) + 1) / 2, (cos(a1) + 1) / 2});

        for (int j = 0; j < st; j++) {
            float h1 = j * hS;
            float h2 = (j + 1) * hS;

            float r1 = r - j * rS;
            float r2 = r - (j + 1) * rS;

            auto gV = [&](float rr, float hh, float aa) {
                float n_mag = sqrt(h * h + r * r);

                return Point {
                    rr * sin(aa), hh, rr * cos(aa),
                    (h / n_mag) * sin(aa),
                    r / n_mag,
                    (h / n_mag) * cos(aa),
                    aa / (2.0f * (float)M_PI),
                    hh / h
                };
            };

            pts.push_back(gV(r1, h1, a1));
            pts.push_back(gV(r1, h1, a2));
            pts.push_back(gV(r2, h2, a2));

            pts.push_back(gV(r1, h1, a1));
            pts.push_back(gV(r2, h2, a2));
            pts.push_back(gV(r2, h2, a1));
        }
    }

    saveToFile(f, pts);
}

void createRing(float innerR, float outerR, int slices, const std::string& filename) {
    std::vector<Point> pts;

    float aS = 2.0f * (float)M_PI / slices;

    for (int i = 0; i < slices; i++) {
        float a1 = i * aS;
        float a2 = (i + 1) * aS;

        Point in1Top  = {innerR * sin(a1), 0, innerR * cos(a1), 0, 1, 0, 0.0f, 0.0f};
        Point out1Top = {outerR * sin(a1), 0, outerR * cos(a1), 0, 1, 0, 1.0f, 0.0f};
        Point in2Top  = {innerR * sin(a2), 0, innerR * cos(a2), 0, 1, 0, 0.0f, 1.0f};
        Point out2Top = {outerR * sin(a2), 0, outerR * cos(a2), 0, 1, 0, 1.0f, 1.0f};

        pts.push_back(in1Top);
        pts.push_back(out1Top);
        pts.push_back(out2Top);

        pts.push_back(in1Top);
        pts.push_back(out2Top);
        pts.push_back(in2Top);

        Point in1Bot = in1Top;
        Point out1Bot = out1Top;
        Point in2Bot = in2Top;
        Point out2Bot = out2Top;

        in1Bot.ny = -1;
        out1Bot.ny = -1;
        in2Bot.ny = -1;
        out2Bot.ny = -1;

        pts.push_back(in1Bot);
        pts.push_back(out2Bot);
        pts.push_back(out1Bot);

        pts.push_back(in1Bot);
        pts.push_back(in2Bot);
        pts.push_back(out2Bot);
    }

    saveToFile(filename, pts);
}

void createBezier(const std::string& pf, int tess, const std::string& of) {
    std::ifstream f(pf);

    if (!f.is_open()) return;

    int nP;
    f >> nP;
    f.ignore(1000, '\n');

    std::vector<Patch> patches(nP);

    for (int i = 0; i < nP; i++) {
        std::string l;
        std::getline(f, l);
        std::replace(l.begin(), l.end(), ',', ' ');

        std::istringstream ss(l);
        int idx;

        while (ss >> idx) {
            patches[i].indices.push_back(idx);
        }
    }

    int nC;
    f >> nC;
    f.ignore(1000, '\n');

    std::vector<Point> cp(nC);

    for (int i = 0; i < nC; i++) {
        std::string l;
        std::getline(f, l);
        std::replace(l.begin(), l.end(), ',', ' ');

        std::istringstream ss(l);
        ss >> cp[i].x >> cp[i].y >> cp[i].z;
    }

    std::vector<Point> res;

    float step = 1.0f / tess;

    for (const auto& p : patches) {
        for (int i = 0; i < tess; i++) {
            for (int j = 0; j < tess; j++) {
                float u = i * step;
                float v = j * step;
                float un = (i + 1) * step;
                float vn = (j + 1) * step;

                Point p00 = getBezierPoint(u, v, cp, p);
                Point p10 = getBezierPoint(un, v, cp, p);
                Point p01 = getBezierPoint(u, vn, cp, p);
                Point p11 = getBezierPoint(un, vn, cp, p);

                res.push_back(p00);
                res.push_back(p10);
                res.push_back(p11);

                res.push_back(p00);
                res.push_back(p11);
                res.push_back(p01);
            }
        }
    }

    saveToFile(of, res);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;

    std::string cmd = argv[1];

    if (cmd == "plane" && argc == 5) {
        createPlane(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    }
    else if (cmd == "box" && argc == 5) {
        createBox(std::stof(argv[2]), std::stoi(argv[3]), argv[4]);
    }
    else if (cmd == "sphere" && argc == 6) {
        createSphere(std::stof(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]), argv[5]);
    }
    else if (cmd == "cone" && argc == 7) {
        createCone(std::stof(argv[2]), std::stof(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), argv[6]);
    }
    else if (cmd == "patch" && argc == 5) {
        createBezier(argv[2], std::stoi(argv[3]), argv[4]);
    }
    else if (cmd == "ring" && argc == 6) {
        createRing(std::stof(argv[2]), std::stof(argv[3]), std::stoi(argv[4]), argv[5]);
    }
    else {
        std::cout << "Uso:\n"
                  << "  generator plane <size> <divisions> <file>\n"
                  << "  generator box <size> <divisions> <file>\n"
                  << "  generator sphere <radius> <slices> <stacks> <file>\n"
                  << "  generator cone <radius> <height> <slices> <stacks> <file>\n"
                  << "  generator patch <patchFile> <tessellation> <file>\n"
                  << "  generator ring <innerRadius> <outerRadius> <slices> <file>\n";

        return 1;
    }

    return 0;
}