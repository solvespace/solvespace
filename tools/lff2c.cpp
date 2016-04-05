#define _USE_MATH_DEFINES
#include <zlib.h>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#define TOLERANCE 1e-6

double correctAngle(double a) {
    return M_PI + remainder(a - M_PI, 2 * M_PI);
}

struct Point {
    double x;
    double y;

    Point operator+(const Point &o) const { return { x + o.x, y + o.y }; }
    Point operator-(const Point &o) const { return { x - o.x, y - o.y }; }
    Point operator*(const Point &o) const { return { x * o.x, y * o.y }; }
    Point operator/(const Point &o) const { return { x / o.x, y / o.y }; }

    Point operator*(double k) const { return { x * k, y * k }; }
    Point operator/(double k) const { return { x / k, y / k }; }

    double length() const{
        return sqrt(x * x + y * y);
    }

    double angle() const {
        return correctAngle(atan2(y, x));
    }

    double distanceTo(const Point &v) const {
        return (*this - v).length();
    }

    double angleTo(const Point &v) const {
        return (v - *this).angle();
    }

    static Point polar(double radius, double angle) {
        return { radius * cos(angle), radius * sin(angle) };
    }

    bool operator==(const Point &o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point &o) const { return x != o.x || y != o.y; }

};

struct Curve {
    std::vector<Point> points;
};

struct Glyph {
    char32_t character;
    char32_t baseCharacter;
    std::vector<Curve> curves;

    void getBoundWidth(double *rminw, double *rmaxw) const {
        if(curves.empty()) {
            *rminw = 0.0;
            *rmaxw = 0.0;
            return;
        }
        double minw = curves[0].points[0].x;
        double maxw = minw;
        for(const Curve &c : curves) {
            for(const Point &p : c.points) {
                maxw = std::max(maxw, p.x);
                minw = std::min(minw, p.x);
            }
        }
        *rminw = minw;
        *rmaxw = maxw;
    }

    void getBoundHeight(double *rminh, double *rmaxh) const {
        if(curves.empty()) {
            *rminh = 0.0;
            *rmaxh = 0.0;
            return;
        }
        double minh = curves[0].points[0].y;
        double maxh = minh;
        for(const Curve &c : curves) {
            for(const Point &p : c.points) {
                maxh = std::max(maxh, p.y);
                minh = std::min(minh, p.y);
            }
        }
        *rminh = minh;
        *rmaxh = maxh;
    }

    double getWidth() const {
        double maxw;
        double minw;
        getBoundWidth(&minw, &maxw);
        return maxw - minw;
    }

    double getHeight() const {
        double maxh;
        double minh;
        getBoundHeight(&minh, &maxh);
        return maxh - minh;
    }

    bool operator<(const Glyph &o) const { return character < o.character; }
};

struct Font {
    double letterSpacing;
    double wordSpacing;
    std::vector<Glyph> glyphs;

    void getGlyphBound(double *rminw, double *rminh, double *rmaxw, double *rmaxh) {
        if(glyphs.empty()) {
            *rminw = 0.0;
            *rmaxw = 0.0;
            *rminh = 0.0;
            *rmaxh = 0.0;
            return;
        }

        glyphs[0].getBoundWidth(rminw, rmaxw);
        glyphs[0].getBoundHeight(rminh, rmaxh);
        for(const Glyph &g : glyphs) {
            double minw, minh, maxw, maxh;
            g.getBoundWidth(&minw, &maxw);
            g.getBoundHeight(&minh, &maxh);
            *rminw = std::min(*rminw, minw);
            *rminh = std::min(*rminh, minh);
            *rmaxw = std::max(*rmaxw, maxw);
            *rmaxh = std::max(*rmaxh, maxh);
        }
    }

    void createArc(Curve &curve, const Point &cp, double radius,
                   double a1, double a2, bool reversed) {
        if (radius < 1e-6) return;

        double aSign = 1.0;
        if(reversed) {
            if(a1 <= a2 + TOLERANCE) a1 += 2.0 * M_PI;
            aSign = -1.0;
        } else {
            if(a2 <= a1 + TOLERANCE) a2 += 2.0 * M_PI;
        }

        // Angle Step (rad)
        double da = fabs(a2 - a1);
        int numPoints = 8;
        double aStep = aSign * da / double(numPoints);

        for(int i = 0; i <= numPoints; i++) {
            curve.points.push_back(cp + Point::polar(radius, a1 + aStep * i));
        }
    }

    void createBulge(const Point &v, double bulge, Curve &curve) {
        bool reversed = bulge < 0.0;
        double alpha = atan(bulge) * 4.0;
        Point &point = curve.points.back();

        Point middle = (point + v) / 2.0;
        double dist = point.distanceTo(v) / 2.0;
        double angle = point.angleTo(v);

        // alpha can't be 0.0 at this point
        double radius = fabs(dist / sin(alpha / 2.0));
        double wu = fabs(radius*radius - dist*dist);
        double h = sqrt(wu);

        if(bulge > 0.0) {
            angle += M_PI_2;
        } else {
            angle -= M_PI_2;
        }

        if (fabs(alpha) > M_PI) {
            h *= -1.0;
        }

        Point center = Point::polar(h, angle);
        center = center + middle;

        double a1 = center.angleTo(point);
        double a2 = center.angleTo(v);
        createArc(curve, center, radius, a1, a2, reversed);
    }

    void readLff(const std::string &path) {
        gzFile lfffont = gzopen(path.c_str(), "rb");
        if(!lfffont) {
            std::cerr << path << ": gzopen failed" << std::endl;
            std::exit(1);
        }

        // Read line by line until we find a new letter:
        Glyph *currentGlyph = NULL;
        while(!gzeof(lfffont)) {
            std::string line;
            do {
                char buf[128] = {0};
                if(!gzgets(lfffont, buf, sizeof(buf)))
                    break;
                line += buf;
            } while(line.back() != '\n');

            if(line.empty() || line[0] == '\n') {
                continue;
            } else if(line[0] == '#') {
                // This is comment or metadata.
                std::istringstream ss(line.substr(1));

                std::vector<std::string> tokens;
                while(!ss.eof()) {
                    std::string token;
                    std::getline(ss, token, ':');
                    std::istringstream(token) >> token; // trim
                    if(!token.empty())
                        tokens.push_back(token);
                }

                // If not in form of "a:b" then it's not metadata, just a comment.
                if (tokens.size() != 2)
                    continue;

                std::string &identifier = tokens[0];
                std::string &value = tokens[1];

                std::transform(identifier.begin(), identifier.end(), identifier.begin(),
                               ::tolower);
                if (identifier == "letterspacing") {
                    std::istringstream(value) >> letterSpacing;
                } else if (identifier == "wordspacing") {
                    std::istringstream(value) >> wordSpacing;
                } else if (identifier == "linespacingfactor") {
                    /* don't care */
                } else if (identifier == "author") {
                    /* don't care */
                } else if (identifier == "name") {
                    /* don't care */
                } else if (identifier == "license") {
                    /* don't care */
                } else if (identifier == "encoding") {
                    /* don't care */
                } else if (identifier == "created") {
                    /* don't care */
                }
            } else if(line[0] == '[') {
                // This is a glyph.
                size_t closingPos;
                char32_t chr = std::stoi(line.substr(1), &closingPos, 16);;
                if(line[closingPos + 1] != ']') {
                    std::cerr << "unrecognized character number: " << line << std::endl;
                    currentGlyph = NULL;
                    continue;
                }

                glyphs.emplace_back();
                currentGlyph = &glyphs.back();
                currentGlyph->character = chr;
                currentGlyph->baseCharacter = 0;
            } else if(currentGlyph != NULL) {
                if (line[0] == 'C') {
                    // This is a reference to another glyph.
                    currentGlyph->baseCharacter = std::stoi(line.substr(1), NULL, 16);
                } else {
                    // This is a series of curves.
                    currentGlyph->curves.emplace_back();
                    Curve &curve = currentGlyph->curves.back();

                    std::stringstream ss(line);
                    while (!ss.eof()) {
                        std::string vertex;
                        std::getline(ss, vertex, ';');

                        std::stringstream ssv(vertex);
                        std::string coord;
                        Point p;

                        if(!std::getline(ssv, coord, ',')) continue;
                        p.x = std::stod(coord);

                        if(!std::getline(ssv, coord, ',')) continue;
                        p.y = std::stod(coord);

                        if(!std::getline(ssv, coord, ',') || coord[0] != 'A') {
                            // This is just a point.
                            curve.points.push_back(p);
                        } else {
                            // This is a point with a bulge.
                            double bulge = std::stod(coord.substr(1));
                            createBulge(p, bulge, curve);
                        }
                    }
                }
            } else {
                std::cerr << "unrecognized line: " << line << std::endl;
            }
        }
        gzclose(lfffont);
    }

    void writeCppHeader(const std::string &hName) {
        std::sort(glyphs.begin(), glyphs.end());

        std::ofstream ts(hName, std::ios::out);

        double minX, minY, maxX, maxY;
        getGlyphBound(&minX, &minY, &maxX, &maxY);

        double size  = 32766.0;
        double scale = size / std::max({ fabs(maxX), fabs(minX), fabs(maxY), fabs(minY) });

        // We use tabs for indentation here to make compilation slightly faster
        ts <<
        "/**** This is a generated file - do not edit ****/\n\n"
        "#ifndef __VECTORFONT_TABLE_H\n"
        "#define __VECTORFONT_TABLE_H\n"
        "\n"
        "#define PEN_UP 32767\n"
        "#define UP PEN_UP\n"
        "\n"
        "struct VectorGlyph {\n"
        "\tchar32_t      character;\n"
        "\tchar32_t      baseCharacter;\n"
        "\tint           width;\n"
        "\tconst int16_t *data;\n"
        "};\n"
        "\n"
        "const int16_t VectorFontData[] = {\n"
        "\tUP, UP,\n";

        std::map<char32_t, size_t> glyphIndexes;
        size_t index = 2;
        for(const Glyph &g : glyphs) {
            ts << "\t// U+" << std::hex << g.character << std::dec << "\n";
            glyphIndexes[g.character] = index;
            for(const Curve &c : g.curves) {
                for(const Point &p : c.points) {
                    ts << "\t" << int(floor(p.x * scale)) << ", " <<
                                  int(floor(p.y * scale)) << ",\n";
                    index += 2;
                }
                ts << "\tUP, UP,\n";
                index += 2;
            }
            ts << "\tUP, UP,\n"; // end-of-glyph marker
            index += 2;
        }

        ts <<
        "};\n"
        "\n"
        "const VectorGlyph VectorFont[] = {\n"
        "\t// U+20\n"
        "\t{ 32, 0, " << int(floor(wordSpacing * scale)) << ", &VectorFontData[0] },\n";

        for(const Glyph &g : glyphs) {
            ts << "\t// U+" << std::hex << g.character << std::dec << "\n";
            ts << "\t{ " << g.character << ", "
                         << g.baseCharacter << ", "
                         << int(floor((g.getWidth() + letterSpacing) * scale)) << ", ";
            ts << "&VectorFontData[" << glyphIndexes[g.character] << "] },\n";
        }

        ts <<
        "};\n"
        "\n"
        "#undef UP\n"
        "\n"
        "#endif\n";
    }
};

int main(int argc, char** argv) {
    if(argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <header out> <lff in>\n" << std::endl;
        return 1;
    }

    Font font;
    font.readLff(argv[2]);
    font.writeCppHeader(argv[1]);

    return 0;
}
