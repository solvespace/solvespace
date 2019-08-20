//-----------------------------------------------------------------------------
// OpenGL ES 2.0 and OpenGL 3.0 shader interface.
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "gl3shader.h"

namespace SolveSpace {

//-----------------------------------------------------------------------------
// Floating point data structures
//-----------------------------------------------------------------------------

Vector2f Vector2f::From(float x, float y) {
    return { x, y };
}

Vector2f Vector2f::From(double x, double y) {
    return { (float)x, (float)y };
}

Vector2f Vector2f::FromInt(uint32_t x, uint32_t y) {
    return { (float)x, (float)y };
}

Vector3f Vector3f::From(float x, float y, float z) {
    return { x, y, z };
}

Vector3f Vector3f::From(const Vector &v) {
    return { (float)v.x, (float)v.y, (float)v.z };
}

Vector3f Vector3f::From(const RgbaColor &c) {
    return { c.redF(), c.greenF(), c.blueF() };
}

Vector4f Vector4f::From(float x, float y, float z, float w) {
    return { x, y, z, w };
}

Vector4f Vector4f::From(const Vector &v, float w) {
    return { (float)v.x, (float)v.y, (float)v.z, w };
}

Vector4f Vector4f::FromInt(uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    return { (float)x, (float)y, (float)z, (float)w };
}

Vector4f Vector4f::From(const RgbaColor &c) {
    return { c.redF(), c.greenF(), c.blueF(), c.alphaF() };
}

//-----------------------------------------------------------------------------
// Shader manipulation
//-----------------------------------------------------------------------------

static GLuint CompileShader(const std::string &res, GLenum type) {
    size_t size;
    const char *resData = (const char *)Platform::LoadResource(res, &size);

    // Sigh, here we go... We want to deploy to four platforms: Linux, Windows, OS X, mobile+web.
    // These platforms are basically disjunctive in the OpenGL versions and profiles that they
    // support: mobile+web support GLES2, Windows can only be guaranteed to support GL1 without
    // vendor's drivers installed but supports D3D9+ natively, Linux supports GL3.2+ and/or
    // GLES2+ depending on whether we run on X11 or Wayland, and OS X supports either a legacy
    // profile or a GL3.2 core profile or (on 10.9+) a GL4.1 core profile.
    // The platforms barely have a common subset of features:
    //  * mobile+web and Windows (D3D9 through ANGLE) are strictly GLES2/GLSL1.0;
    //  * OS X legacy compatibility profile has GLSL1.2 only shaders, and GL3.2 core profile
    //    that has GLSL1.0 shaders compatible with GLES2 makes mandatory the use of vertex array
    //    objects, which cannot be used in GLES2 at all; similarly GL3.2 core has GL_RED but not
    //    GL_ALPHA whereas GLES2 has GL_ALPHA but not GL_RED.
    //  * GTK does not work on anything prior to GL3.0/GLES2.0; it does not permit explicitly
    //    asking for a compatibility profile, i.e. you can only ask for 3.2+; and it does not
    //    permit asking for a GLES profile prior to GTK 3.22, which will get into Ubuntu
    //    no earlier than late 2017. This is despite the fact that if only GTK defaulted
    //    to the compatibility profile, everything would have just worked as Mesa is
    //    very permissive.
    // While we're at it, let's remember that GLES2 has *only* glDepthRangef, GL3.2 has *only*
    // glDepthRange, and GL4.1+ has both glDepthRangef and glDepthRange. Also, that GLSL1.0
    // makes `precision highp float;` mandatory in fragment shaders, and GLSL1.2 removes
    // the `precision` keyword entirely, because that's clearly how minor versions work.
    // Christ, what a trash fire.

    const char *prelude;
#if defined(HAVE_GLES)
    prelude = R"(
#version 100
#define TEX_ALPHA a
precision highp float;
)";
#else
    prelude = R"(
#version 120
#define TEX_ALPHA r
)";
#endif
    std::string src(resData, size);
    src = prelude + src;

    GLuint shader = glCreateShader(type);
    ssassert(shader != 0, "glCreateShader failed");

    const GLint   glSize[]   = { (int)src.length() };
    const GLchar* glSource[] = { src.c_str() };
    glShaderSource(shader, 1, glSource, glSize);
    glCompileShader(shader);

    GLint infoLen;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
    if(infoLen > 1) {
        std::string infoStr(infoLen, '\0');
        glGetShaderInfoLog(shader, infoLen, NULL, &infoStr[0]);
        dbp(infoStr.c_str());
    }

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if(!compiled) {
        dbp("Failed to compile shader:\n"
            "----8<----8<----8<----8<----8<----\n"
            "%s\n"
            "----8<----8<----8<----8<----8<----\n",
            src.c_str());
    }
    ssassert(compiled, "Cannot compile shader");

    return shader;
}

void Shader::Init(const std::string &vertexRes, const std::string &fragmentRes,
                  const std::vector<std::pair<GLuint, std::string> > &locations) {
    GLuint vert = CompileShader(vertexRes, GL_VERTEX_SHADER);
    GLuint frag = CompileShader(fragmentRes, GL_FRAGMENT_SHADER);

    program = glCreateProgram();
    ssassert(program != 0, "glCreateProgram failed");

    glAttachShader(program, vert);
    glAttachShader(program, frag);
    for(const auto &l : locations) {
        glBindAttribLocation(program, l.first, l.second.c_str());
    }
    glLinkProgram(program);

    GLint infoLen;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
    if(infoLen > 1) {
        std::string infoStr(infoLen, '\0');
        glGetProgramInfoLog(program, infoLen, NULL, &infoStr[0]);
        dbp(infoStr.c_str());
    }

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    ssassert(linked, "Cannot link shader");
}

void Shader::Clear() {
    glDeleteProgram(program);
}

void Shader::SetUniformMatrix(const char *name, double *md) {
    Enable();
    float mf[16];
    for(int i = 0; i < 16; i++) mf[i] = (float)md[i];
    glUniformMatrix4fv(glGetUniformLocation(program, name), 1, false, mf);
}

void Shader::SetUniformVector(const char *name, const Vector &v) {
    Enable();
    glUniform3f(glGetUniformLocation(program, name), (float)v.x, (float)v.y, (float)v.z);
}

void Shader::SetUniformVector(const char *name, const Vector4f &v) {
    Enable();
    glUniform4f(glGetUniformLocation(program, name), v.x, v.y, v.z, v.w);
}

void Shader::SetUniformColor(const char *name, RgbaColor c) {
    Enable();
    glUniform4f(glGetUniformLocation(program, name), c.redF(), c.greenF(), c.blueF(), c.alphaF());
}

void Shader::SetUniformFloat(const char *name, float v) {
    Enable();
    glUniform1f(glGetUniformLocation(program, name), v);
}

void Shader::SetUniformInt(const char *name, GLint v) {
    Enable();
    glUniform1i(glGetUniformLocation(program, name), v);
}

void Shader::SetUniformTextureUnit(const char *name, GLint index) {
    Enable();
    glUniform1i(glGetUniformLocation(program, name), index);
}

void Shader::Enable() const {
    glUseProgram(program);
}

void Shader::Disable() const {
    glUseProgram(0);
}

//-----------------------------------------------------------------------------
// Mesh rendering
//-----------------------------------------------------------------------------

void MeshRenderer::Init() {
    lightShader.Init(
        "shaders/mesh.vert", "shaders/mesh.frag",
        {
            { ATTRIB_POS, "pos" },
            { ATTRIB_NOR, "nor" },
            { ATTRIB_COL, "col" },
        }
    );

    fillShader.Init(
        "shaders/mesh_fill.vert", "shaders/mesh_fill.frag",
        {
            { ATTRIB_POS, "pos" },
        }
    );
    fillShader.SetUniformTextureUnit("texture_", 0);

    selectedShader = &lightShader;
}

void MeshRenderer::Clear() {
    lightShader.Clear();
    fillShader.Clear();
}

MeshRenderer::Handle MeshRenderer::Add(const SMesh &m, bool dynamic) {
    Handle handle;
    glGenBuffers(1, &handle.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);

    MeshVertex *vertices = new MeshVertex[m.l.n * 3];
    for(int i = 0; i < m.l.n; i++) {
        const STriangle &t = m.l[i];
        vertices[i * 3 + 0].pos = Vector3f::From(t.a);
        vertices[i * 3 + 1].pos = Vector3f::From(t.b);
        vertices[i * 3 + 2].pos = Vector3f::From(t.c);

        if(t.an.EqualsExactly(Vector::From(0, 0, 0))) {
            Vector3f normal = Vector3f::From(t.Normal());
            vertices[i * 3 + 0].nor = normal;
            vertices[i * 3 + 1].nor = normal;
            vertices[i * 3 + 2].nor = normal;
        } else {
            vertices[i * 3 + 0].nor = Vector3f::From(t.an);
            vertices[i * 3 + 1].nor = Vector3f::From(t.bn);
            vertices[i * 3 + 2].nor = Vector3f::From(t.cn);
        }

        for(int j = 0; j < 3; j++) {
            vertices[i * 3 + j].col = Vector4f::From(t.meta.color);
        }

    }
    glBufferData(GL_ARRAY_BUFFER, m.l.n * 3 * sizeof(MeshVertex),
                 vertices, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    handle.size = m.l.n * 3;
    delete []vertices;

    return handle;
}

void MeshRenderer::Remove(const MeshRenderer::Handle &handle) {
    glDeleteBuffers(1, &handle.vertexBuffer);
}

void MeshRenderer::Draw(const MeshRenderer::Handle &handle,
                        bool useColors, RgbaColor overrideColor) {
    if(handle.size == 0) return;

    selectedShader->Enable();

    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);

    glEnableVertexAttribArray(ATTRIB_POS);
    glVertexAttribPointer(ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void *)offsetof(MeshVertex, pos));

    if(selectedShader == &lightShader) {
        glEnableVertexAttribArray(ATTRIB_NOR);
        glVertexAttribPointer(ATTRIB_NOR, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                              (void *)offsetof(MeshVertex, nor));
        if(useColors) {
            glEnableVertexAttribArray(ATTRIB_COL);
            glVertexAttribPointer(ATTRIB_COL, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                  (void *)offsetof(MeshVertex, col));
        } else {
            glVertexAttrib4f(ATTRIB_COL, overrideColor.redF(), overrideColor.greenF(), overrideColor.blueF(), overrideColor.alphaF());
        }
    }

    glDrawArrays(GL_TRIANGLES, 0, handle.size);

    glDisableVertexAttribArray(ATTRIB_POS);
    if(selectedShader == &lightShader) {
        glDisableVertexAttribArray(ATTRIB_NOR);
        if(useColors) glDisableVertexAttribArray(ATTRIB_COL);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    selectedShader->Disable();
}

void MeshRenderer::Draw(const SMesh &mesh, bool useColors, RgbaColor overrideColor) {
    Handle handle = Add(mesh, /*dynamic=*/true);
    Draw(handle, useColors, overrideColor);
    Remove(handle);
}

void MeshRenderer::SetModelview(double *matrix) {
    lightShader.SetUniformMatrix("modelview", matrix);
    fillShader.SetUniformMatrix("modelview", matrix);
}

void MeshRenderer::SetProjection(double *matrix) {
    lightShader.SetUniformMatrix("projection", matrix);
    fillShader.SetUniformMatrix("projection", matrix);
}

void MeshRenderer::UseShaded(const Lighting &lighting) {
    Vector dir0 = lighting.lightDirection[0];
    Vector dir1 = lighting.lightDirection[1];
    dir0.z = -dir0.z;
    dir1.z = -dir1.z;

    lightShader.SetUniformVector("lightDir0", dir0);
    lightShader.SetUniformFloat("lightInt0", (float)lighting.lightIntensity[0]);
    lightShader.SetUniformVector("lightDir1", dir1);
    lightShader.SetUniformFloat("lightInt1", (float)lighting.lightIntensity[1]);
    lightShader.SetUniformFloat("ambient", (float)lighting.ambientIntensity);
    selectedShader = &lightShader;
}

void MeshRenderer::UseFilled(const Canvas::Fill &fill) {
    fillShader.SetUniformColor("color", fill.color);
    selectedShader = &fillShader;
}

//-----------------------------------------------------------------------------
// Arrangement of stipple patterns into textures
//-----------------------------------------------------------------------------

static double Frac(double x) {
    return x - floor(x);
}

static RgbaColor EncodeLengthAsFloat(double v) {
    v = max(0.0, min(1.0, v));
    double er = v;
    double eg = Frac(255.0 * v);
    double eb = Frac(65025.0 * v);
    double ea = Frac(160581375.0 * v);

    double r = er - eg / 255.0;
    double g = eg - eb / 255.0;
    double b = eb - ea / 255.0;
    return RgbaColor::From((int)floor( r * 255.0 + 0.5),
                           (int)floor( g * 255.0 + 0.5),
                           (int)floor( b * 255.0 + 0.5),
                           (int)floor(ea * 255.0 + 0.5));
}

GLuint Generate(const std::vector<double> &pattern) {
    double patternLen = 0.0;
    for(double s : pattern) {
        patternLen += s;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    GLint size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
    RgbaColor *textureData = new RgbaColor[size];

    int mipCount = (int)log2(size) + 1;
    for(int mip = 0; mip < mipCount; mip++) {
        int dashI = 0;
        double dashT = 0.0;
        for(int i = 0; i < size; i++) {
            if(pattern.empty()) {
                textureData[i] = EncodeLengthAsFloat(0.0);
                continue;
            }

            double t = (double)i / (double)(size - 1);
            while(t - LENGTH_EPS > dashT + pattern[dashI] / patternLen) {
                dashT += pattern[dashI] / patternLen;
                dashI++;
            }
            double dashW = pattern[dashI] / patternLen;
            if(dashI % 2 == 0) {
                textureData[i] = EncodeLengthAsFloat(0.0);
            } else {
                double value;
                if(t - dashT < pattern[dashI] / patternLen / 2.0) {
                    value = t - dashT;
                } else {
                    value = dashT + dashW - t;
                }
                value = value * patternLen;
                textureData[i] = EncodeLengthAsFloat(value);
            }
        }
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA, size, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     textureData);
        size /= 2;
    }

    delete []textureData;
    return texture;
}

void StippleAtlas::Init() {
    for(uint32_t i = 0; i <= (uint32_t)StipplePattern::LAST; i++) {
        patterns.push_back(Generate(StipplePatternDashes((StipplePattern)i)));
    }
}

void StippleAtlas::Clear() {
    for(GLuint p : patterns) {
        glDeleteTextures(1, &p);
    }
}

GLint StippleAtlas::GetTexture(StipplePattern pattern) const {
    return patterns[(uint32_t)pattern];
}

double StippleAtlas::GetLength(StipplePattern pattern) const {
    if(pattern == StipplePattern::CONTINUOUS) {
        return 1.0;
    }
    return StipplePatternLength(pattern);
}

//-----------------------------------------------------------------------------
// Edge rendering
//-----------------------------------------------------------------------------

void EdgeRenderer::Init(const StippleAtlas *a) {
    atlas = a;
    shader.Init(
        "shaders/edge.vert", "shaders/edge.frag",
        {
            { ATTRIB_POS, "pos" },
            { ATTRIB_LOC, "loc" },
            { ATTRIB_TAN, "tgt" }
        }
    );
}

void EdgeRenderer::Clear() {
    shader.Clear();
}

EdgeRenderer::Handle EdgeRenderer::Add(const SEdgeList &edges, bool dynamic) {
    Handle handle;
    glGenBuffers(1, &handle.vertexBuffer);
    glGenBuffers(1, &handle.indexBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle.indexBuffer);

    EdgeVertex *vertices = new EdgeVertex[edges.l.n * 8];
    uint32_t *indices = new uint32_t[edges.l.n * 6 * 3];
    double phase = 0.0;
    uint32_t curVertex = 0;
    uint32_t curIndex = 0;
    for(int i = 0; i < edges.l.n; i++) {
        const SEdge &curr = edges.l[i];
        const SEdge &next = edges.l[(i + 1) % edges.l.n];

        // 3d positions
        Vector3f a = Vector3f::From(curr.a);
        Vector3f b = Vector3f::From(curr.b);

        // tangent
        Vector3f tan = Vector3f::From(curr.b.Minus(curr.a));

        // length
        double len = curr.b.Minus(curr.a).Magnitude();

        // make line start cap
        for(int j = 0; j < 2; j++) {
            vertices[curVertex + j].pos = a;
            vertices[curVertex + j].tan = tan;
        }
        vertices[curVertex + 0].loc = Vector3f::From(-1.0f, -1.0f, float(phase));
        vertices[curVertex + 1].loc = Vector3f::From(-1.0f, +1.0f, float(phase));

        indices[curIndex++] = curVertex + 0;
        indices[curIndex++] = curVertex + 1;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 1;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 3;

        curVertex += 2;

        // make line body
        vertices[curVertex +  0].pos = a;
        vertices[curVertex +  1].pos = a;
        vertices[curVertex +  2].pos = b;
        vertices[curVertex +  3].pos = b;

        for(int j = 0; j < 4; j++) {
            vertices[curVertex + j].tan = tan;
        }

        vertices[curVertex +  0].loc = Vector3f::From( 0.0f, -1.0f, float(phase));
        vertices[curVertex +  1].loc = Vector3f::From( 0.0f, +1.0f, float(phase));
        vertices[curVertex +  2].loc = Vector3f::From( 0.0f, +1.0f, float(phase + len));
        vertices[curVertex +  3].loc = Vector3f::From( 0.0f, -1.0f, float(phase + len));

        indices[curIndex++] = curVertex + 0;
        indices[curIndex++] = curVertex + 1;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 0;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 3;

        curVertex += 4;

        // make line end cap
        for(int j = 0; j < 2; j++) {
            vertices[curVertex + j].pos = b;
            vertices[curVertex + j].tan = tan;
        }

        vertices[curVertex + 0].loc = Vector3f::From(+1.0, +1.0, float(phase + len));
        vertices[curVertex + 1].loc = Vector3f::From(+1.0, -1.0, float(phase + len));

        indices[curIndex++] = curVertex - 2;
        indices[curIndex++] = curVertex - 1;
        indices[curIndex++] = curVertex;
        indices[curIndex++] = curVertex - 1;
        indices[curIndex++] = curVertex;
        indices[curIndex++] = curVertex + 1;

        curVertex += 2;

        // phase stitching
        if(curr.a.EqualsExactly(next.a) ||
           curr.a.EqualsExactly(next.b) ||
           curr.b.EqualsExactly(next.a) ||
           curr.b.EqualsExactly(next.b))
        {
            phase += len;
        } else {
            phase = 0.0;
        }
    }
    handle.size = curIndex;
    GLenum mode = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    glBufferData(GL_ARRAY_BUFFER, curVertex * sizeof(EdgeVertex), vertices, mode);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, curIndex * sizeof(uint32_t), indices, mode);
    delete []vertices;
    delete []indices;

    return handle;
}

void EdgeRenderer::Remove(const EdgeRenderer::Handle &handle) {
    glDeleteBuffers(1, &handle.vertexBuffer);
    glDeleteBuffers(1, &handle.indexBuffer);
}

void EdgeRenderer::Draw(const EdgeRenderer::Handle &handle) {
    if(handle.size == 0) return;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, atlas->GetTexture(pattern));
    shader.SetUniformTextureUnit("pattern", 1);
    shader.SetUniformFloat("patternLen", (float)atlas->GetLength(pattern));

    shader.Enable();

    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle.indexBuffer);

    glEnableVertexAttribArray(ATTRIB_POS);
    glEnableVertexAttribArray(ATTRIB_LOC);
    glEnableVertexAttribArray(ATTRIB_TAN);

    glVertexAttribPointer(ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(EdgeVertex), (void *)offsetof(EdgeVertex, pos));
    glVertexAttribPointer(ATTRIB_LOC, 3, GL_FLOAT, GL_FALSE, sizeof(EdgeVertex), (void *)offsetof(EdgeVertex, loc));
    glVertexAttribPointer(ATTRIB_TAN, 3, GL_FLOAT, GL_FALSE, sizeof(EdgeVertex), (void *)offsetof(EdgeVertex, tan));
    glDrawElements(GL_TRIANGLES, handle.size, GL_UNSIGNED_INT, NULL);

    glDisableVertexAttribArray(ATTRIB_POS);
    glDisableVertexAttribArray(ATTRIB_LOC);
    glDisableVertexAttribArray(ATTRIB_TAN);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    shader.Disable();
}

void EdgeRenderer::Draw(const SEdgeList &edges) {
    Handle handle = Add(edges, /*dynamic=*/true);
    Draw(handle);
    Remove(handle);
}

void EdgeRenderer::SetModelview(double *matrix) {
    shader.SetUniformMatrix("modelview", matrix);
}

void EdgeRenderer::SetProjection(double *matrix) {
    shader.SetUniformMatrix("projection", matrix);
}

void EdgeRenderer::SetStroke(const Canvas::Stroke &stroke, double pixel) {
    double unitScale = stroke.unit == Canvas::Unit::PX ? pixel : 1.0;
    shader.SetUniformFloat("width", float(stroke.width * unitScale / 2.0));
    shader.SetUniformColor("color", stroke.color);
    shader.SetUniformFloat("patternScale", float(stroke.stippleScale * unitScale * 2.0));
    shader.SetUniformFloat("pixel", (float)pixel);
    pattern = stroke.stipplePattern;
}

//-----------------------------------------------------------------------------
// Outline rendering
//-----------------------------------------------------------------------------

void OutlineRenderer::Init(const StippleAtlas *a) {
    atlas = a;
    shader.Init(
        "shaders/outline.vert", "shaders/edge.frag",
        {
            { ATTRIB_POS, "pos" },
            { ATTRIB_LOC, "loc" },
            { ATTRIB_TAN, "tgt" },
            { ATTRIB_NOL, "nol" },
            { ATTRIB_NOR, "nor" }
        }
    );
}

void OutlineRenderer::Clear() {
    shader.Clear();
}

OutlineRenderer::Handle OutlineRenderer::Add(const SOutlineList &outlines, bool dynamic) {
    Handle handle;
    glGenBuffers(1, &handle.vertexBuffer);
    glGenBuffers(1, &handle.indexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle.indexBuffer);

    OutlineVertex *vertices = new OutlineVertex[outlines.l.n * 8];
    uint32_t *indices = new uint32_t[outlines.l.n * 6 * 3];
    double phase = 0.0;
    uint32_t curVertex = 0;
    uint32_t curIndex = 0;

    for(int i = 0; i < outlines.l.n; i++) {
        const SOutline &curr = outlines.l[i];
        const SOutline &next = outlines.l[(i + 1) % outlines.l.n];

        // 3d positions
        Vector3f a = Vector3f::From(curr.a);
        Vector3f b = Vector3f::From(curr.b);
        Vector3f nl = Vector3f::From(curr.nl);
        Vector3f nr = Vector3f::From(curr.nr);

        // tangent
        Vector3f tan = Vector3f::From(curr.b.Minus(curr.a));

        // length
        double len = curr.b.Minus(curr.a).Magnitude();
        float tag = (float)curr.tag;

        // make line start cap
        for(int j = 0; j < 2; j++) {
            vertices[curVertex + j].pos = a;
            vertices[curVertex + j].nol = nl;
            vertices[curVertex + j].nor = nr;
            vertices[curVertex + j].tan = tan;
        }

        vertices[curVertex + 0].loc = Vector4f::From(-1.0f, -1.0f, float(phase), (float)tag);
        vertices[curVertex + 1].loc = Vector4f::From(-1.0f, +1.0f, float(phase), (float)tag);

        indices[curIndex++] = curVertex;
        indices[curIndex++] = curVertex + 1;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 1;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 3;

        curVertex += 2;

        // make line body
        vertices[curVertex +  0].pos = a;
        vertices[curVertex +  1].pos = a;
        vertices[curVertex +  2].pos = b;
        vertices[curVertex +  3].pos = b;

        for(int j = 0; j < 4; j++) {
            vertices[curVertex + j].nol = nl;
            vertices[curVertex + j].nor = nr;
            vertices[curVertex + j].tan = tan;
        }

        vertices[curVertex +  0].loc = Vector4f::From( 0.0f, -1.0f, float(phase), (float)tag);
        vertices[curVertex +  1].loc = Vector4f::From( 0.0f, +1.0f, float(phase), (float)tag);
        vertices[curVertex +  2].loc = Vector4f::From( 0.0f, +1.0f,
                                                      float(phase + len), (float)tag);
        vertices[curVertex +  3].loc = Vector4f::From( 0.0f, -1.0f,
                                                      float(phase + len), (float)tag);

        indices[curIndex++] = curVertex + 0;
        indices[curIndex++] = curVertex + 1;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 0;
        indices[curIndex++] = curVertex + 2;
        indices[curIndex++] = curVertex + 3;

        curVertex += 4;

        // make line end cap
        for(int j = 0; j < 2; j++) {
            vertices[curVertex + j].pos = b;
            vertices[curVertex + j].nol = nl;
            vertices[curVertex + j].nor = nr;
            vertices[curVertex + j].tan = tan;
        }

        vertices[curVertex + 0].loc = Vector4f::From(+1.0f, +1.0f, float(phase + len), (float)tag);
        vertices[curVertex + 1].loc = Vector4f::From(+1.0f, -1.0f, float(phase + len), (float)tag);

        indices[curIndex++] = curVertex - 2;
        indices[curIndex++] = curVertex - 1;
        indices[curIndex++] = curVertex;
        indices[curIndex++] = curVertex - 1;
        indices[curIndex++] = curVertex;
        indices[curIndex++] = curVertex + 1;

        curVertex += 2;

        // phase stitching
        if(curr.a.EqualsExactly(next.a) ||
           curr.a.EqualsExactly(next.b) ||
           curr.b.EqualsExactly(next.a) ||
           curr.b.EqualsExactly(next.b))
        {
            phase += len;
        } else {
            phase = 0.0;
        }
    }
    handle.size = curIndex;
    GLenum mode = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    glBufferData(GL_ARRAY_BUFFER, curVertex * sizeof(OutlineVertex), vertices, mode);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, curIndex * sizeof(uint32_t), indices, mode);

    delete []vertices;
    delete []indices;
    return handle;
}

void OutlineRenderer::Remove(const OutlineRenderer::Handle &handle) {
    glDeleteBuffers(1, &handle.vertexBuffer);
    glDeleteBuffers(1, &handle.indexBuffer);
}

void OutlineRenderer::Draw(const OutlineRenderer::Handle &handle, Canvas::DrawOutlinesAs mode) {
    if(handle.size == 0) return;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, atlas->GetTexture(pattern));
    shader.SetUniformTextureUnit("pattern", 1);
    shader.SetUniformFloat("patternLen", (float)atlas->GetLength(pattern));
    shader.SetUniformInt("mode", (GLint)mode);

    shader.Enable();

    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle.indexBuffer);

    glEnableVertexAttribArray(ATTRIB_POS);
    glEnableVertexAttribArray(ATTRIB_LOC);
    glEnableVertexAttribArray(ATTRIB_TAN);
    glEnableVertexAttribArray(ATTRIB_NOL);
    glEnableVertexAttribArray(ATTRIB_NOR);

    glVertexAttribPointer(ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(OutlineVertex),
                          (void *)offsetof(OutlineVertex, pos));
    glVertexAttribPointer(ATTRIB_LOC, 4, GL_FLOAT, GL_FALSE, sizeof(OutlineVertex),
                          (void *)offsetof(OutlineVertex, loc));
    glVertexAttribPointer(ATTRIB_TAN, 3, GL_FLOAT, GL_FALSE, sizeof(OutlineVertex),
                          (void *)offsetof(OutlineVertex, tan));
    glVertexAttribPointer(ATTRIB_NOL, 3, GL_FLOAT, GL_FALSE, sizeof(OutlineVertex),
                          (void *)offsetof(OutlineVertex, nol));
    glVertexAttribPointer(ATTRIB_NOR, 3, GL_FLOAT, GL_FALSE, sizeof(OutlineVertex),
                          (void *)offsetof(OutlineVertex, nor));
    glDrawElements(GL_TRIANGLES, handle.size, GL_UNSIGNED_INT, NULL);

    glDisableVertexAttribArray(ATTRIB_POS);
    glDisableVertexAttribArray(ATTRIB_LOC);
    glDisableVertexAttribArray(ATTRIB_TAN);
    glDisableVertexAttribArray(ATTRIB_NOL);
    glDisableVertexAttribArray(ATTRIB_NOR);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    shader.Disable();
}

void OutlineRenderer::Draw(const SOutlineList &outlines, Canvas::DrawOutlinesAs drawAs) {
    Handle handle = Add(outlines, /*dynamic=*/true);
    Draw(handle, drawAs);
    Remove(handle);
}

void OutlineRenderer::SetModelview(double *matrix) {
    shader.SetUniformMatrix("modelview", matrix);
}

void OutlineRenderer::SetProjection(double *matrix) {
    shader.SetUniformMatrix("projection", matrix);
}

void OutlineRenderer::SetStroke(const Canvas::Stroke &stroke, double pixel) {
    double unitScale = (stroke.unit == Canvas::Unit::PX) ? pixel : 1.0;
    shader.SetUniformFloat("width", (float)(stroke.width * unitScale / 2.0));
    shader.SetUniformColor("color", stroke.color);
    shader.SetUniformFloat("patternScale", (float)(stroke.stippleScale * unitScale * 2.0));
    shader.SetUniformFloat("pixel", (float)pixel);
    pattern = stroke.stipplePattern;
}

//-----------------------------------------------------------------------------
// Indexed mesh storage
//-----------------------------------------------------------------------------

void SIndexedMesh::AddPoint(const Vector &p) {
    uint32_t vstart = vertices.size();
    vertices.resize(vertices.size() + 4);

    vertices[vstart + 0].pos = Vector3f::From(p);
    vertices[vstart + 0].tex = Vector2f::From(-1.0f, -1.0f);
    vertices[vstart + 1].pos = Vector3f::From(p);
    vertices[vstart + 1].tex = Vector2f::From(+1.0f, -1.0f);
    vertices[vstart + 2].pos = Vector3f::From(p);
    vertices[vstart + 2].tex = Vector2f::From(+1.0f, +1.0f);
    vertices[vstart + 3].pos = Vector3f::From(p);
    vertices[vstart + 3].tex = Vector2f::From(-1.0f, +1.0f);

    size_t istart = indices.size();
    indices.resize(indices.size() + 6);

    indices[istart + 0] = vstart + 0;
    indices[istart + 1] = vstart + 1;
    indices[istart + 2] = vstart + 2;
    indices[istart + 3] = vstart + 0;
    indices[istart + 4] = vstart + 2;
    indices[istart + 5] = vstart + 3;
}

void SIndexedMesh::AddQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d) {
    uint32_t vstart = vertices.size();
    vertices.resize(vertices.size() + 4);

    vertices[vstart + 0].pos = Vector3f::From(a);
    vertices[vstart + 1].pos = Vector3f::From(b);
    vertices[vstart + 2].pos = Vector3f::From(c);
    vertices[vstart + 3].pos = Vector3f::From(d);

    size_t istart = indices.size();
    indices.resize(indices.size() + 6);

    indices[istart + 0] = vstart + 0;
    indices[istart + 1] = vstart + 1;
    indices[istart + 2] = vstart + 2;
    indices[istart + 3] = vstart + 0;
    indices[istart + 4] = vstart + 2;
    indices[istart + 5] = vstart + 3;
}

void SIndexedMesh::AddPixmap(const Vector &o, const Vector &u, const Vector &v,
                             const Point2d &ta, const Point2d &tb) {
    uint32_t vstart = vertices.size();
    vertices.resize(vertices.size() + 4);

    vertices[vstart + 0].pos = Vector3f::From(o);
    vertices[vstart + 0].tex = Vector2f::From(ta.x, ta.y);

    vertices[vstart + 1].pos = Vector3f::From(o.Plus(v));
    vertices[vstart + 1].tex = Vector2f::From(ta.x, tb.y);

    vertices[vstart + 2].pos = Vector3f::From(o.Plus(u).Plus(v));
    vertices[vstart + 2].tex = Vector2f::From(tb.x, tb.y);

    vertices[vstart + 3].pos = Vector3f::From(o.Plus(u));
    vertices[vstart + 3].tex = Vector2f::From(tb.x, ta.y);

    size_t istart = indices.size();
    indices.resize(indices.size() + 6);

    indices[istart + 0] = vstart + 0;
    indices[istart + 1] = vstart + 1;
    indices[istart + 2] = vstart + 2;
    indices[istart + 3] = vstart + 0;
    indices[istart + 4] = vstart + 2;
    indices[istart + 5] = vstart + 3;
}

void SIndexedMesh::Clear() {
    vertices.clear();
    indices.clear();
}

//-----------------------------------------------------------------------------
// Indexed mesh rendering
//-----------------------------------------------------------------------------

void IndexedMeshRenderer::Init() {
    colShader.Init(
        "shaders/imesh.vert", "shaders/imesh.frag",
        {
            { ATTRIB_POS, "pos" }
        }
    );
    texShader.Init(
        "shaders/imesh_tex.vert", "shaders/imesh_tex.frag",
        {
            { ATTRIB_POS, "pos" },
            { ATTRIB_TEX, "tex" }
        }
    );
    texaShader.Init(
        "shaders/imesh_tex.vert", "shaders/imesh_texa.frag",
        {
            { ATTRIB_POS, "pos" },
            { ATTRIB_TEX, "tex" }
        }
    );
    pointShader.Init(
        "shaders/imesh_point.vert", "shaders/imesh_point.frag",
        {
            { ATTRIB_POS, "pos" },
            { ATTRIB_TEX, "loc" }
        }
    );

    texShader.SetUniformTextureUnit("texture_", 0);
    texaShader.SetUniformTextureUnit("texture_", 0);
    selectedShader = &colShader;
}

void IndexedMeshRenderer::Clear() {
    texShader.Clear();
    texaShader.Clear();
    colShader.Clear();
    pointShader.Clear();
}

IndexedMeshRenderer::Handle IndexedMeshRenderer::Add(const SIndexedMesh &m, bool dynamic) {
    Handle handle;
    glGenBuffers(1, &handle.vertexBuffer);
    glGenBuffers(1, &handle.indexBuffer);

    GLenum mode = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, m.vertices.size() * sizeof(SIndexedMesh::Vertex),
                 m.vertices.data(), mode);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(uint32_t),
                 m.indices.data(), mode);
    handle.size = m.indices.size();
    return handle;
}

void IndexedMeshRenderer::Remove(const IndexedMeshRenderer::Handle &handle) {
    glDeleteBuffers(1, &handle.vertexBuffer);
    glDeleteBuffers(1, &handle.indexBuffer);
}

void IndexedMeshRenderer::Draw(const IndexedMeshRenderer::Handle &handle) {
    if(handle.size == 0) return;

    selectedShader->Enable();

    glBindBuffer(GL_ARRAY_BUFFER, handle.vertexBuffer);
    glEnableVertexAttribArray(ATTRIB_POS);
    glVertexAttribPointer(ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(SIndexedMesh::Vertex),
                          (void *)offsetof(SIndexedMesh::Vertex, pos));
    if(NeedsTexture()) {
        glEnableVertexAttribArray(ATTRIB_TEX);
        glVertexAttribPointer(ATTRIB_TEX, 2, GL_FLOAT, GL_FALSE, sizeof(SIndexedMesh::Vertex),
                              (void *)offsetof(SIndexedMesh::Vertex, tex));
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle.indexBuffer);
    glDrawElements(GL_TRIANGLES, handle.size, GL_UNSIGNED_INT, NULL);

    glDisableVertexAttribArray(ATTRIB_POS);
    if(NeedsTexture()) glDisableVertexAttribArray(ATTRIB_TEX);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    selectedShader->Disable();
}

void IndexedMeshRenderer::Draw(const SIndexedMesh &mesh) {
    Handle handle = Add(mesh, /*dynamic=*/true) ;
    Draw(handle);
    Remove(handle);
}

bool IndexedMeshRenderer::NeedsTexture() const {
    return selectedShader == &texShader ||
        selectedShader == &texaShader ||
        selectedShader == &pointShader;
}

void IndexedMeshRenderer::SetModelview(double *matrix) {
    colShader.SetUniformMatrix("modelview", matrix);
    texShader.SetUniformMatrix("modelview", matrix);
    texaShader.SetUniformMatrix("modelview", matrix);
    pointShader.SetUniformMatrix("modelview", matrix);
}

void IndexedMeshRenderer::SetProjection(double *matrix) {
    colShader.SetUniformMatrix("projection", matrix);
    texShader.SetUniformMatrix("projection", matrix);
    texaShader.SetUniformMatrix("projection", matrix);
    pointShader.SetUniformMatrix("projection", matrix);
}

void IndexedMeshRenderer::UseFilled(const Canvas::Fill &fill) {
    if(fill.texture) {
        selectedShader = (fill.texture->format == Pixmap::Format::A) ? &texaShader : &texShader;
    } else {
        selectedShader = &colShader;
    }
    selectedShader->SetUniformColor("color", fill.color);
}

void IndexedMeshRenderer::UsePoint(const Canvas::Stroke &stroke, double pixel) {
    pointShader.SetUniformColor("color", stroke.color);
    pointShader.SetUniformFloat("width", (float)(stroke.width * pixel / 2.0));
    pointShader.SetUniformFloat("pixel", (float)pixel);
    selectedShader = &pointShader;
}

}
