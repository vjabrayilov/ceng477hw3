#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <GL/glew.h>   // The GL Header File
#include <GL/gl.h>   // The GL Header File
#include <GLFW/glfw3.h> // The GLFW header
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

using namespace std;

GLuint gProgram[6];
GLint gIntensityLoc;
string objName;
int scalingFactor = 0;
bool isAnimating = false;
bool isSliding = false;
bool isExplosionChecked = false;
float angle = 0.0;
float gIntensity = 1000;
float cursorX, cursorY;
int gWidth = 640, gHeight = 600;
int row = 0, col = 0;
int moves = 0, score = 0;

struct Vertex
{
    Vertex(GLfloat inX, GLfloat inY, GLfloat inZ) : x(inX), y(inY), z(inZ) { }
    GLfloat x, y, z;
};

struct Texture
{
    Texture(GLfloat inU, GLfloat inV) : u(inU), v(inV) { }
    GLfloat u, v;
};

struct Normal
{
    Normal(GLfloat inX, GLfloat inY, GLfloat inZ) : x(inX), y(inY), z(inZ) { }
    GLfloat x, y, z;
};

struct Face
{
	Face(int v[], int t[], int n[]) {
		vIndex[0] = v[0];
		vIndex[1] = v[1];
		vIndex[2] = v[2];
		tIndex[0] = t[0];
		tIndex[1] = t[1];
		tIndex[2] = t[2];
		nIndex[0] = n[0];
		nIndex[1] = n[1];
		nIndex[2] = n[2];
	}
    GLuint vIndex[3], tIndex[3], nIndex[3];
};

enum objectColors {red = 0, green = 1, blue = 2, yellow = 3, torqoise = 4};
bool **explodingGrid;
int **yDistance;
objectColors **colorGrid;
vector<Vertex> gVertices;
vector<Texture> gTextures;
vector<Normal> gNormals;
vector<Face> gFaces;

GLuint gVertexAttribBuffer, gTextVBO, gIndexBuffer;
GLint gInVertexLoc, gInNormalLoc;
int gVertexDataSizeInBytes, gNormalDataSizeInBytes;

/// Holds all state information relevant to a character as loaded using FreeType
struct Character {
    GLuint TextureID;   // ID handle of the glyph texture
    glm::ivec2 Size;    // Size of glyph
    glm::ivec2 Bearing;  // Offset from baseline to left/top of glyph
    GLuint Advance;    // Horizontal offset to advance to next glyph
};

std::map<GLchar, Character> Characters;


bool ParseObj(const string& fileName)
{
    fstream myfile;

    // Open the input 
    myfile.open(fileName.c_str(), std::ios::in);

    if (myfile.is_open())
    {
        string curLine;

        while (getline(myfile, curLine))
        {
            stringstream str(curLine);
            GLfloat c1, c2, c3;
            GLuint index[9];
            string tmp;

            if (curLine.length() >= 2)
            {
                if (curLine[0] == '#') // comment
                {
                    continue;
                }
                else if (curLine[0] == 'v')
                {
                    if (curLine[1] == 't') // texture
                    {
                        str >> tmp; // consume "vt"
                        str >> c1 >> c2;
                        gTextures.push_back(Texture(c1, c2));
                    }
                    else if (curLine[1] == 'n') // normal
                    {
                        str >> tmp; // consume "vn"
                        str >> c1 >> c2 >> c3;
                        gNormals.push_back(Normal(c1, c2, c3));
                    }
                    else // vertex
                    {
                        str >> tmp; // consume "v"
                        str >> c1 >> c2 >> c3;
                        gVertices.push_back(Vertex(c1, c2, c3));
                    }
                }
                else if (curLine[0] == 'f') // face
                {
                    str >> tmp; // consume "f"
					char c;
					int vIndex[3],  nIndex[3], tIndex[3];
					str >> vIndex[0]; str >> c >> c; // consume "//"
					str >> nIndex[0]; 
					str >> vIndex[1]; str >> c >> c; // consume "//"
					str >> nIndex[1]; 
					str >> vIndex[2]; str >> c >> c; // consume "//"
					str >> nIndex[2]; 

					assert(vIndex[0] == nIndex[0] &&
						   vIndex[1] == nIndex[1] &&
						   vIndex[2] == nIndex[2]); // a limitation for now

					// make indices start from 0
					for (int c = 0; c < 3; ++c)
					{
						vIndex[c] -= 1;
						nIndex[c] -= 1;
						tIndex[c] -= 1;
					}

                    gFaces.push_back(Face(vIndex, tIndex, nIndex));
                }
                else
                {
                    cout << "Ignoring unidentified line in obj file: " << curLine << endl;
                }
            }

            //data += curLine;
            if (!myfile.eof())
            {
                //data += "\n";
            }
        }

        myfile.close();
    }
    else
    {
        return false;
    }

	assert(gVertices.size() == gNormals.size());

    return true;
}

bool ReadDataFromFile(
    const string& fileName, ///< [in]  Name of the shader file
    string&       data)     ///< [out] The contents of the file
{
    fstream myfile;

    // Open the input 
    myfile.open(fileName.c_str(), std::ios::in);

    if (myfile.is_open())
    {
        string curLine;

        while (getline(myfile, curLine))
        {
            data += curLine;
            if (!myfile.eof())
            {
                data += "\n";
            }
        }

        myfile.close();
    }
    else
    {
        return false;
    }

    return true;
}

void createVS(GLuint& program, const string& filename)
{
    string shaderSource;

    if (!ReadDataFromFile(filename, shaderSource))
    {
        cout << "Cannot find file name: " + filename << endl;
        exit(-1);
    }

    GLint length = shaderSource.length();
    const GLchar* shader = (const GLchar*) shaderSource.c_str();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &shader, &length);
    glCompileShader(vs);

    char output[1024] = {0};
    glGetShaderInfoLog(vs, 1024, &length, output);
    printf("VS compile log: %s\n", output);

    glAttachShader(program, vs);
}

void createFS(GLuint& program, const string& filename)
{
    string shaderSource;

    if (!ReadDataFromFile(filename, shaderSource))
    {
        cout << "Cannot find file name: " + filename << endl;
        exit(-1);
    }

    GLint length = shaderSource.length();
    const GLchar* shader = (const GLchar*) shaderSource.c_str();

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &shader, &length);
    glCompileShader(fs);

    char output[1024] = {0};
    glGetShaderInfoLog(fs, 1024, &length, output);
    printf("FS compile log: %s\n", output);

    glAttachShader(program, fs);
}

void initShaders()
{
    gProgram[0] = glCreateProgram();
    gProgram[1] = glCreateProgram();
    gProgram[2] = glCreateProgram();
    gProgram[3] = glCreateProgram();
    gProgram[4] = glCreateProgram();
    gProgram[5] = glCreateProgram();

    createVS(gProgram[0], "vert0.glsl");
    createFS(gProgram[0], "frag0.glsl");

    createVS(gProgram[1], "vert1.glsl");
    createFS(gProgram[1], "frag1.glsl");

    createVS(gProgram[2], "vert2.glsl");
    createFS(gProgram[2], "frag2.glsl");

    createVS(gProgram[4], "vert3.glsl");
    createFS(gProgram[4], "frag3.glsl");

    createVS(gProgram[5], "vert4.glsl");
    createFS(gProgram[5], "frag4.glsl");

    createVS(gProgram[3], "vert_text.glsl");
    createFS(gProgram[3], "frag_text.glsl");

    glBindAttribLocation(gProgram[0], 0, "inVertex");
    glBindAttribLocation(gProgram[0], 1, "inNormal");
    glBindAttribLocation(gProgram[1], 0, "inVertex");
    glBindAttribLocation(gProgram[1], 1, "inNormal");
    glBindAttribLocation(gProgram[2], 0, "inVertex");
    glBindAttribLocation(gProgram[2], 1, "inNormal");
    glBindAttribLocation(gProgram[4], 0, "inVertex");
    glBindAttribLocation(gProgram[4], 1, "inNormal");
    glBindAttribLocation(gProgram[5], 0, "inVertex");
    glBindAttribLocation(gProgram[5], 1, "inNormal");
    glBindAttribLocation(gProgram[3], 2, "vertex");

    glLinkProgram(gProgram[0]);
    glLinkProgram(gProgram[1]);
    glLinkProgram(gProgram[2]);
    glLinkProgram(gProgram[3]);
    glLinkProgram(gProgram[4]);
    glLinkProgram(gProgram[5]);
    glUseProgram(gProgram[0]);

    gIntensityLoc = glGetUniformLocation(gProgram[0], "intensity");
    cout << "gIntensityLoc = " << gIntensityLoc << endl;
    glUniform1f(gIntensityLoc, gIntensity);
}

void initVBO()
{
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    assert(glGetError() == GL_NONE);

    glGenBuffers(1, &gVertexAttribBuffer);
    glGenBuffers(1, &gIndexBuffer);

    assert(gVertexAttribBuffer > 0 && gIndexBuffer > 0);

    glBindBuffer(GL_ARRAY_BUFFER, gVertexAttribBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);

    gVertexDataSizeInBytes = gVertices.size() * 3 * sizeof(GLfloat);
    gNormalDataSizeInBytes = gNormals.size() * 3 * sizeof(GLfloat);
    int indexDataSizeInBytes = gFaces.size() * 3 * sizeof(GLuint);
    GLfloat* vertexData = new GLfloat [gVertices.size() * 3];
    GLfloat* normalData = new GLfloat [gNormals.size() * 3];
    GLuint* indexData = new GLuint [gFaces.size() * 3];

    float minX = 1e6, maxX = -1e6;
    float minY = 1e6, maxY = -1e6;
    float minZ = 1e6, maxZ = -1e6;

    for (int i = 0; i < gVertices.size(); ++i)
    {
        vertexData[3*i] = gVertices[i].x;
        vertexData[3*i+1] = gVertices[i].y;
        vertexData[3*i+2] = gVertices[i].z;

        minX = std::min(minX, gVertices[i].x);
        maxX = std::max(maxX, gVertices[i].x);
        minY = std::min(minY, gVertices[i].y);
        maxY = std::max(maxY, gVertices[i].y);
        minZ = std::min(minZ, gVertices[i].z);
        maxZ = std::max(maxZ, gVertices[i].z);
    }

    std::cout << "minX = " << minX << std::endl;
    std::cout << "maxX = " << maxX << std::endl;
    std::cout << "minY = " << minY << std::endl;
    std::cout << "maxY = " << maxY << std::endl;
    std::cout << "minZ = " << minZ << std::endl;
    std::cout << "maxZ = " << maxZ << std::endl;

    for (int i = 0; i < gNormals.size(); ++i)
    {
        normalData[3*i] = gNormals[i].x;
        normalData[3*i+1] = gNormals[i].y;
        normalData[3*i+2] = gNormals[i].z;
    }

    for (int i = 0; i < gFaces.size(); ++i)
    {
        indexData[3*i] = gFaces[i].vIndex[0];
        indexData[3*i+1] = gFaces[i].vIndex[1];
        indexData[3*i+2] = gFaces[i].vIndex[2];
    }


    glBufferData(GL_ARRAY_BUFFER, gVertexDataSizeInBytes + gNormalDataSizeInBytes, 0, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, gVertexDataSizeInBytes, vertexData);
    glBufferSubData(GL_ARRAY_BUFFER, gVertexDataSizeInBytes, gNormalDataSizeInBytes, normalData);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexDataSizeInBytes, indexData, GL_STATIC_DRAW);

    // done copying; can free now
    delete[] vertexData;
    delete[] normalData;
    delete[] indexData;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(gVertexDataSizeInBytes));

}

void initFonts(int windowWidth, int windowHeight)
{
    // Set OpenGL options
    //glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(windowWidth), 0.0f, static_cast<GLfloat>(windowHeight));
    glUseProgram(gProgram[3]);
    glUniformMatrix4fv(glGetUniformLocation(gProgram[3], "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // FreeType
    FT_Library ft;
    // All functions return a value different than 0 whenever an error occurred
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    }

    // Load font as face
    FT_Face face;
    if (FT_New_Face(ft, "/usr/share/fonts/truetype/liberation/LiberationSerif-Italic.ttf", 0, &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
    }

    // Set size to load glyphs as
    FT_Set_Pixel_Sizes(face, 0, 48);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    // Load first 128 characters of ASCII set
    for (GLubyte c = 0; c < 128; c++)
    {
        // Load character glyph 
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }
        // Generate texture
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
                );
        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Now store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x
        };
        Characters.insert(std::pair<GLchar, Character>(c, character));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    // Destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    //
    // Configure VBO for texture quads
    //
    glGenBuffers(1, &gTextVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gTextVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void initExplodingGrid() {
    explodingGrid = new bool*[row];

    for(int i = 0; i < row; i++) {
        explodingGrid[i] = new bool[col];

        for(int j = 0; j < col; j++) {
            explodingGrid[i][j] = false;
        }
    }
}

void initColorGrid() {

    colorGrid = new objectColors*[row];

    for(int i = 0; i < row; i++) {
        colorGrid[i] = new objectColors[col];

        for(int j = 0; j < col; j++) {
            colorGrid[i][j] = objectColors(rand() % 5);
        }
    }
}

void initYDistance() {

    yDistance = new int*[row];

    for(int i = 0; i < row; i++) {
        yDistance[i] = new int[col];

        for(int j = 0; j < col; j++) {
            yDistance[i][j] = 0;
        }
    }

}

void init() 
{
	//ParseObj("armadillo.obj");
	ParseObj(objName.c_str());

    glEnable(GL_DEPTH_TEST);
    initShaders();
    initFonts(gWidth, gHeight);
    initVBO();
    initColorGrid();
    initExplodingGrid();
    initYDistance();
}

void drawModel()
{
	glBindBuffer(GL_ARRAY_BUFFER, gVertexAttribBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(gVertexDataSizeInBytes));

	glDrawElements(GL_TRIANGLES, gFaces.size() * 3, GL_UNSIGNED_INT, 0);
}

void renderText(const std::string& text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color)
{
    // Activate corresponding render state	
    glUseProgram(gProgram[3]);
    glUniform3f(glGetUniformLocation(gProgram[3], "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) 
    {
        Character ch = Characters[*c];

        GLfloat xpos = x + ch.Bearing.x * scale;
        GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        GLfloat w = ch.Size.x * scale;
        GLfloat h = ch.Size.y * scale;

        // Update VBO for each character
        GLfloat vertices[6][4] = {
            { xpos,     ypos + h,   0.0, 0.0 },            
            { xpos,     ypos,       0.0, 1.0 },
            { xpos + w, ypos,       1.0, 1.0 },

            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos + w, ypos,       1.0, 1.0 },
            { xpos + w, ypos + h,   1.0, 0.0 }           
        };

        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        // Update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, gTextVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // Be sure to use glBufferSubData and not glBufferData

        //glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)

        x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void resetYDistance() {
    for(int i = 0; i < row; i++) {
        for(int j = 0; j < col; j++) {
            yDistance[i][j] = 0;
        }
    }
}

void repaintGrid(){
    for(int i = 0; i < row; i++) {
        for(int j = 0; j < col; j++) {
            colorGrid[i][j] = objectColors(rand() % 5);
        }
    }
}

void resetExplosionGrid(){
    for(int i = 0; i < row; i++){
        for(int j = 0; j < col; j++){
            explodingGrid[i][j] = false;
        }
    }
}

void display()
{
    glClearColor(0, 0, 0, 1);
    glClearDepth(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    float dy = 19.f/row;
    float dx = 20.f/col;
    int programIdx = 0;

    int isExploding = 0;
    
    for(int i = 0; i < row; i++) {
        for(int j = 0; j < col; j++) {
            isExploding += explodingGrid[i][j] ? 1 : 0;
        }
    }

    isAnimating = isExploding > 0;

    if(scalingFactor == 50) {
        if(isExploding >= 3) {
            score += isExploding;
        }

        scalingFactor = 0;
        
        for(int j = 0; j < col; j++) {

            bool isShifting = false;
            int numShift = 1;

            for(int i = row - 1; i >= 0; i--) {
                
                if(explodingGrid[i][j] == false && isShifting == false) {
                    yDistance[i][j] = 0;
                    continue;
                }

                isShifting = true;
                for(; (i - numShift) >= 0; numShift++) {
                    if(explodingGrid[i - numShift][j] == false) {
                        colorGrid[i][j] = colorGrid[i - numShift][j];
                        break;
                    }
                }

                yDistance[i][j] = (numShift * 20.f)/(0.05f * row);
                if(i - numShift < 0) {
                    colorGrid[i][j] = objectColors(rand() % 5);
                }
            }
        }

        /*
        std::cout << "Shifting Matrix" << std::endl;
        for(int i = 0; i < row; i++) {
            for(int j = 0; j < col; j++) {
                std::cout << yDistance[i][j] << ' ';
            }
            std::cout << std::endl;
        } std::cout << std::endl;
        */

        resetExplosionGrid();
        isSliding = true;
    }

    scalingFactor += (isExploding > 0) ? 1 : 0;
    
    int numZeros = 0;
    for(int i = 0; i < row; i++) {
        for(int j = 0; j < col; j++) {

            switch (colorGrid[i][j])
            {
            case red:
                programIdx = 0;
                break;
            case green:
                programIdx = 1;
                break;
            case blue:
                programIdx = 2;
                break;
            case yellow:
                programIdx = 4;
                break;
            case torqoise:
                programIdx = 5;
                break;
            }

            glUseProgram(gProgram[programIdx]);

            glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(3.5f / max(col, row) ) );
            
            if(explodingGrid[i][j] == true) {
                glm::mat4 extraScale = glm::scale(glm::mat4(1.f), glm::vec3(1.f + 0.01f * scalingFactor));
                S = extraScale * S;
            }
            
            float shiftingFactor = 0;

            if(yDistance[i][j] > 0) {
                shiftingFactor = 0.05 * yDistance[i][j];
                yDistance[i][j]--;
            } else {
                numZeros++;
            }

            glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(-10.f + dx*(j + 0.5f), 10.f - dy*(i + 0.5f) + shiftingFactor, -10.f));
            glm::mat4 R = glm::rotate(glm::mat4(1.f), glm::radians(angle), glm::vec3(0, 1, 0));
            glm::mat4 modelMat =  T * R * S;
            glm::mat4 modelMatInv = glm::transpose(glm::inverse(modelMat));
            glm::mat4 perspMat = glm::ortho(-10.f, 10.f, -10.f, 10.f, -20.f, 20.f);

            glUniformMatrix4fv(glGetUniformLocation(gProgram[programIdx], "modelingMat"), 1, GL_FALSE, glm::value_ptr(modelMat));
            glUniformMatrix4fv(glGetUniformLocation(gProgram[programIdx], "modelingMatInvTr"), 1, GL_FALSE, glm::value_ptr(modelMatInv));
            glUniformMatrix4fv(glGetUniformLocation(gProgram[programIdx], "perspectiveMat"), 1, GL_FALSE, glm::value_ptr(perspMat));

            drawModel();

            assert(glGetError() == GL_NO_ERROR);
        }
    }

    if(numZeros == row*col && isSliding == true)
    {
        isExplosionChecked = false;
        isSliding = false;
    }
    
    string displayText = string("Moves: ") + to_string(moves) + string(" Score: ") + to_string(score);

    renderText(displayText, 0, 0, 1, glm::vec3(1, 1, 0));

    assert(glGetError() == GL_NO_ERROR);

	angle += 0.5;
}

void reshape(GLFWwindow* window, int w, int h)
{
    w = w < 1 ? 1 : w;
    h = h < 1 ? 1 : h;

    gWidth = w;
    gHeight = h;

    glViewport(0, 0, w, h);
}

static void cursor(GLFWwindow* window, double xpos, double ypos) {
    cursorX = xpos;
    cursorY = ypos;
}

static void button(GLFWwindow* window, int button, int action, int mods) {

    //std::cout << isSliding << ' ' << isExplosionChecked << std::endl;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && isSliding == false && isExplosionChecked == true && isAnimating == false) {
        int x = cursorX/gWidth * col;
        int y = cursorY/(gHeight*0.95f) * row;

        /*
        std::cout << row << ' ' << col << std::endl;
        std::cout << gWidth << ' ' << gHeight << std::endl;
        std::cout << cursorX << ' ' << cursorY << std::endl;
        std::cout << x << ' ' << y << std::endl;
        */

        moves++;
        explodingGrid[y][x] = true;
    }
}


static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    else if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        repaintGrid();
        resetExplosionGrid();
        resetYDistance();
        isExplosionChecked = false;
        isAnimating = false;
        isSliding = false;
        scalingFactor = 0;
        angle = 0;
        moves = 0;
        score = 0;
    }
}

void printGridCheck(){

    std::cout << "Color Matrix" << std::endl;
    for(int i = 0; i < row; i++) {
        for(int j = 0; j < col; j++) {
            std::cout << colorGrid[i][j] << ' ';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "Exploding Matrix" << std::endl;
    for(int i = 0; i < row; i++) {
        for(int j = 0; j < col; j++) {
            std::cout << explodingGrid[i][j] << ' ';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void checkExplosion() {
    
    if(isExplosionChecked){
        return;
    }

    objectColors currColor, lastColor;
    int numSameColor = 0;

    for(int i = 0; i < row; i++) {
        lastColor = colorGrid[i][0];
        numSameColor++;
        
        for(int j = 1; j < col; j++) {
            currColor = colorGrid[i][j];

            if(currColor == lastColor) {
                numSameColor++;

                if(numSameColor >= 3) {
                    for(int k = j-numSameColor+1; k <= j; k++) {
                        explodingGrid[i][k] = true;
                    }
                }

            } else {
                numSameColor = 1;
                lastColor = currColor;
            }
        }
        numSameColor = 0;
    }

    for(int j = 0; j < col; j++) {
        lastColor = colorGrid[0][j];
        numSameColor++;

        for(int i = 1; i < row; i++) {
            currColor = colorGrid[i][j];

            if(currColor == lastColor) {
                numSameColor++;

                if(numSameColor >= 3) {
                    for(int k = i-numSameColor+1; k <= i; k++) {
                        explodingGrid[k][j] = true;
                    }
                }

            } else {
                numSameColor = 1;
                lastColor = currColor;
            }
        }
        numSameColor = 0;
    }

    //printGridCheck();

    isExplosionChecked = true;
}

void mainLoop(GLFWwindow* window) {
    while (!glfwWindowShouldClose(window))
    {
        display();
        glfwSwapBuffers(window);
        glfwPollEvents();
        checkExplosion();
    }
}



int main(int argc, char** argv)   // Create Main Function For Bringing It All Together
{
    col = stoi(string(argv[1]));
    row = stoi(string(argv[2]));
    objName = argv[3];

    GLFWwindow* window;
    if (!glfwInit())
    {
        exit(-1);
    }

    std::cout << red << ' ' << green << ' ' << blue << std::endl;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(gWidth, gHeight, "Simple Example", NULL, NULL);

    if (!window)
    {
        glfwTerminate();
        exit(-1);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Initialize GLEW to setup the OpenGL Function pointers
    if (GLEW_OK != glewInit())
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return EXIT_FAILURE;
    }

    char rendererInfo[512] = {0};
    strcpy(rendererInfo, (const char*) glGetString(GL_RENDERER));
    strcat(rendererInfo, " - ");
    strcat(rendererInfo, (const char*) glGetString(GL_VERSION));
    glfwSetWindowTitle(window, rendererInfo);

    init();

    glfwSetKeyCallback(window, keyboard);
    glfwSetMouseButtonCallback(window, button);
    glfwSetCursorPosCallback(window, cursor);
    glfwSetWindowSizeCallback(window, reshape);

    reshape(window, gWidth, gHeight); // need to call this once ourselves
    mainLoop(window); // this does not return unless the window is closed

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

