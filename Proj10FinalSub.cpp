// Proj10FinalSub.cpp
// Project 10 — Advanced Mapping Shaders
// Builds on Project 9 and adds:
//   Sphere   -> Bump-Picture.jpg diffuse texture + Blinn-Phong
//   Cube     -> Environment Mapping (Cube Map from Humus)
//   Cylinder -> Bump Mapping using Bump-Map.jpg
//
// Compile:
//   g++ Proj10FinalSub.cpp -o run -lglfw -lGL -lGLEW -lSOIL -lassimp
//
// Authors: Jared Walker & James Hohn
// Course:  CST-310 — Computer Graphics
// School:  Grand Canyon University
//
// Required files in same directory as executable:
//   Bump-Map.jpg       (greyscale height map — cylinder bump mapping)
//   Bump-Picture.jpg   (color brick texture  — sphere diffuse)
//   posx.jpg, negx.jpg (cube map +X / -X faces from Humus)
//   posy.jpg, negy.jpg (cube map +Y / -Y faces from Humus)
//   posz.jpg, negz.jpg (cube map +Z / -Z faces from Humus)
//
// How to get cube map images:
//   wget "https://www.humus.name/Textures/Sorsele3.zip" -O cubemap.zip
//   unzip cubemap.zip
//   (produces posx.jpg, negx.jpg, posy.jpg, negy.jpg, posz.jpg, negz.jpg)
//
// Controls:
//   Arrow L/R        — yaw (look left / right)
//   Arrow U/D        — pitch (look up / down)
//   Shift+Up/Down    — move forward / backward
//   W / S            — move up / down
//   A / D            — strafe left / right
//   Q / E            — roll
//   R                — reset camera
//   ESC              — quit

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <SOIL/SOIL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdio>
#include <vector>

const float PI = 3.14159265f;
int winW = 900, winH = 600;

// ─── Vertex shader (shared by all objects) ────────────────────────────────────
// Outputs: world position, world normal, UV coords, and a TBN matrix.
// TBN (Tangent-Bitangent-Normal) allows the fragment shader to convert
// tangent-space normals (from a bump map) into world space for lighting.
const char* vertSrc = R"GLSL(
#version 120
attribute vec3 aPos;
attribute vec3 aNormal;
attribute vec2 aUV;
attribute vec3 aTangent;
varying vec3 fragPos;
varying vec3 fragNormal;
varying vec2 fragUV;
varying mat3 TBN;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
void main() {
    vec4 world  = model * vec4(aPos, 1.0);
    fragPos     = world.xyz;
    fragNormal  = normalize(mat3(model) * aNormal);
    fragUV      = aUV;
    vec3 T = normalize(mat3(model) * aTangent);
    vec3 N = fragNormal;
    T = normalize(T - dot(T,N)*N);
    vec3 B = cross(N,T);
    TBN = mat3(T,B,N);
    gl_Position = proj * view * world;
}
)GLSL";

// ─── Fragment shader: Sphere — diffuse texture ────────────────────────────────
// Samples Bump-Picture.jpg as the color, then applies Blinn-Phong shading
// using the geometric surface normal. The result looks like a textured sphere.
const char* fragSphereSrc = R"GLSL(
#version 120
varying vec3 fragPos;
varying vec3 fragNormal;
varying vec2 fragUV;
uniform sampler2D diffuseTex;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
void main() {
    vec3 color    = texture2D(diffuseTex, fragUV).rgb;
    vec3 norm     = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 viewDir  = normalize(viewPos  - fragPos);
    vec3 halfDir  = normalize(lightDir + viewDir);
    float diff    = max(dot(norm, lightDir), 0.0);
    float spec    = pow(max(dot(norm, halfDir), 0.0), 48.0);
    vec3 result   = (0.25 + diff + 0.4*spec) * color * lightColor;
    gl_FragColor  = vec4(result, 1.0);
}
)GLSL";

// ─── Fragment shader: Cube — Environment (Cube) Map ──────────────────────────
// Computes the reflection vector R = reflect(I, N), where I is the incident
// view ray and N is the surface normal. R is used to index into the cube map,
// sampling the environment color that would be reflected toward the camera.
// A specular highlight is added on top for realism.
const char* fragEnvSrc = R"GLSL(
#version 120
varying vec3 fragPos;
varying vec3 fragNormal;
uniform vec3        viewPos;
uniform samplerCube cubemap;
void main() {
    vec3 I = normalize(fragPos - viewPos);
    vec3 R = reflect(I, normalize(fragNormal));
    vec4 envColor = textureCube(cubemap, R);
    vec3 lightDir = normalize(vec3(6,8,6) - fragPos);
    vec3 halfDir  = normalize(lightDir - I);
    float spec    = pow(max(dot(normalize(fragNormal), halfDir), 0.0), 64.0);
    vec3 result   = envColor.rgb + 0.3 * spec;
    gl_FragColor  = vec4(clamp(result, 0.0, 1.0), 1.0);
}
)GLSL";

// ─── Fragment shader: Cylinder — Bump Mapping ────────────────────────────────
// Samples the height map (Bump-Map.jpg) at the current UV and at small offsets
// in U and V to estimate the surface gradient (slope in each direction).
// That gradient forms a perturbed normal in tangent space, which TBN converts
// to world space. Lighting is then computed with that perturbed normal, giving
// the appearance of surface relief without any extra geometry.
const char* fragBumpSrc = R"GLSL(
#version 120
varying vec3 fragPos;
varying vec3 fragNormal;
varying vec2 fragUV;
varying mat3 TBN;
uniform vec3      objectColor;
uniform vec3      lightPos;
uniform vec3      lightColor;
uniform vec3      viewPos;
uniform float     shininess;
uniform sampler2D bumpMap;
void main() {
    float d  = 1.0 / 512.0;
    float h0 = texture2D(bumpMap, fragUV).r;
    float hU = texture2D(bumpMap, fragUV + vec2(d, 0.0)).r;
    float hV = texture2D(bumpMap, fragUV + vec2(0.0, d)).r;
    vec3 tN  = normalize(vec3((h0-hU)*3.0, (h0-hV)*3.0, 1.0));
    vec3 norm = normalize(TBN * tN);
    vec3  lightDir = normalize(lightPos - fragPos);
    vec3  viewDir  = normalize(viewPos  - fragPos);
    vec3  halfDir  = normalize(lightDir + viewDir);
    float diff = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(norm, halfDir), 0.0), shininess);
    vec3 result = (0.2 + diff + 0.6*spec) * objectColor * lightColor;
    gl_FragColor = vec4(result, 1.0);
}
)GLSL";

// ─── Checkerboard floor (unchanged from Project 9) ───────────────────────────
const char* fragBoardSrc = R"GLSL(
#version 120
varying vec3 fragPos;
varying vec3 fragNormal;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
void main() {
    float chk = mod(floor(fragPos.x) + floor(fragPos.z), 2.0);
    vec3 color = (chk == 0.0) ? vec3(0.8,0,0.8) : vec3(0.9,0.9,0.9);
    vec3  norm     = normalize(fragNormal);
    vec3  lightDir = normalize(lightPos - fragPos);
    vec3  viewDir  = normalize(viewPos  - fragPos);
    vec3  halfDir  = normalize(lightDir + viewDir);
    float diff     = max(dot(norm, lightDir), 0.0);
    float spec     = pow(max(dot(norm, halfDir), 0.0), 16.0);
    gl_FragColor = vec4((0.28 + diff + 0.15*spec) * color * lightColor, 1.0);
}
)GLSL";

// ─── Shader helpers ───────────────────────────────────────────────────────────
GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s,1,&src,nullptr); glCompileShader(s);
    int ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(s,512,nullptr,log); fprintf(stderr,"Shader error:\n%s\n",log); }
    return s;
}
GLuint buildProgram(const char* vs, const char* fs) {
    GLuint v=compileShader(GL_VERTEX_SHADER,vs);
    GLuint f=compileShader(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram();
    glAttachShader(p,v); glAttachShader(p,f);
    glBindAttribLocation(p,0,"aPos");
    glBindAttribLocation(p,1,"aNormal");
    glBindAttribLocation(p,2,"aUV");
    glBindAttribLocation(p,3,"aTangent");
    glLinkProgram(p);
    int ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){ char log[512]; glGetProgramInfoLog(p,512,nullptr,log); fprintf(stderr,"Link error:\n%s\n",log); }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

GLuint sphereProg, envProg, bumpProg, boardProg;

// ─── Matrix helpers ───────────────────────────────────────────────────────────
glm::mat4 myPerspective(float fovDeg, float aspect, float zn, float zf) {
    float f=1.0f/tanf(fovDeg*PI/360.0f);
    glm::mat4 m(0.0f);
    m[0][0]=f/aspect; m[1][1]=f;
    m[2][2]=(zf+zn)/(zn-zf); m[2][3]=-1.0f;
    m[3][2]=(2.0f*zf*zn)/(zn-zf);
    return m;
}
glm::mat4 myLookAt(glm::vec3 eye, glm::vec3 target, glm::vec3 up) {
    glm::vec3 f=glm::normalize(target-eye);
    glm::vec3 r=glm::normalize(glm::cross(f,glm::normalize(up)));
    glm::vec3 u=glm::cross(r,f);
    glm::mat4 m(1.0f);
    m[0][0]=r.x; m[1][0]=r.y; m[2][0]=r.z;
    m[0][1]=u.x; m[1][1]=u.y; m[2][1]=u.z;
    m[0][2]=-f.x;m[1][2]=-f.y;m[2][2]=-f.z;
    m[3][0]=-glm::dot(r,eye); m[3][1]=-glm::dot(u,eye); m[3][2]=glm::dot(f,eye);
    return m;
}

// ─── Mesh struct — 11 floats per vertex: pos(3) normal(3) uv(2) tangent(3) ───
struct Mesh {
    GLuint vao=0,vbo=0,ebo=0; int indexCount=0;
    void upload(const std::vector<float>& verts, const std::vector<unsigned int>& idx) {
        indexCount=(int)idx.size();
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glGenBuffers(1,&ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,verts.size()*4,verts.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*4,idx.data(),GL_STATIC_DRAW);
        int st=11*4;
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,st,(void*)0);   glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,st,(void*)12);  glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,st,(void*)24);  glEnableVertexAttribArray(2);
        glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,st,(void*)32);  glEnableVertexAttribArray(3);
        glBindVertexArray(0);
    }
    void draw(){ glBindVertexArray(vao); glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,0); glBindVertexArray(0); }
};

// ─── Sphere — spherical UV, tangent = d(pos)/d(theta) ────────────────────────
Mesh buildSphere(int stacks, int slices) {
    std::vector<float> v; std::vector<unsigned int> idx;
    for(int i=0;i<=stacks;i++){
        float phi=PI*i/stacks;
        for(int j=0;j<=slices;j++){
            float theta=2*PI*j/slices;
            float x=sinf(phi)*cosf(theta),y=cosf(phi),z=sinf(phi)*sinf(theta);
            float u=(float)j/slices, vv=(float)i/stacks;
            float tx=-sinf(theta),ty=0,tz=cosf(theta);
            v.insert(v.end(),{x,y,z, x,y,z, u,vv, tx,ty,tz});
        }
    }
    for(int i=0;i<stacks;i++)
        for(int j=0;j<slices;j++){
            unsigned int tl=i*(slices+1)+j,tr=tl+1,bl=tl+(slices+1),br=bl+1;
            idx.insert(idx.end(),{tl,bl,tr,tr,bl,br});
        }
    Mesh m; m.upload(v,idx); return m;
}

// ─── Cylinder — UV wraps once around barrel (U=angle, V=height) ──────────────
Mesh buildCylinder(int slices, float h) {
    std::vector<float> v; std::vector<unsigned int> idx;
    float hh=h/2.0f;
    for(int j=0;j<=slices;j++){
        float theta=2*PI*j/slices,cx=cosf(theta),cz=sinf(theta);
        float u=(float)j/slices,tx=-sinf(theta),tz=cosf(theta);
        v.insert(v.end(),{cx,-hh,cz, cx,0,cz, u,0, tx,0,tz});
        v.insert(v.end(),{cx, hh,cz, cx,0,cz, u,1, tx,0,tz});
    }
    for(int j=0;j<slices;j++){
        unsigned int b0=j*2,t0=b0+1,b1=b0+2,t1=b0+3;
        idx.insert(idx.end(),{b0,b1,t0,t0,b1,t1});
    }
    unsigned int cBot=(unsigned int)(v.size()/11);
    v.insert(v.end(),{0,-hh,0, 0,-1,0, 0.5f,0.5f, 1,0,0});
    for(int j=0;j<=slices;j++){float t=2*PI*j/slices,cx=cosf(t),cz=sinf(t);v.insert(v.end(),{cx,-hh,cz,0,-1,0,cx*0.5f+0.5f,cz*0.5f+0.5f,1,0,0});}
    for(int j=0;j<slices;j++) idx.insert(idx.end(),{cBot,cBot+j+2,cBot+j+1});
    unsigned int cTop=(unsigned int)(v.size()/11);
    v.insert(v.end(),{0,hh,0, 0,1,0, 0.5f,0.5f, 1,0,0});
    for(int j=0;j<=slices;j++){float t=2*PI*j/slices,cx=cosf(t),cz=sinf(t);v.insert(v.end(),{cx,hh,cz,0,1,0,cx*0.5f+0.5f,cz*0.5f+0.5f,1,0,0});}
    for(int j=0;j<slices;j++) idx.insert(idx.end(),{cTop,cTop+j+1,cTop+j+2});
    Mesh m; m.upload(v,idx); return m;
}

// ─── Cube — each face UV (0,0)-(1,1), tangent = face right direction ──────────
Mesh buildCube() {
    std::vector<float> v = {
        // Front  (+Z, tangent +X)
        -1,-1, 1, 0,0,1, 0,0, 1,0,0,   1,-1, 1, 0,0,1, 1,0, 1,0,0,
         1, 1, 1, 0,0,1, 1,1, 1,0,0,  -1, 1, 1, 0,0,1, 0,1, 1,0,0,
        // Back   (-Z, tangent -X)
         1,-1,-1, 0,0,-1, 0,0,-1,0,0, -1,-1,-1, 0,0,-1, 1,0,-1,0,0,
        -1, 1,-1, 0,0,-1, 1,1,-1,0,0,  1, 1,-1, 0,0,-1, 0,1,-1,0,0,
        // Left   (-X, tangent -Z)
        -1,-1,-1,-1,0,0, 0,0, 0,0,-1, -1,-1, 1,-1,0,0, 1,0, 0,0,-1,
        -1, 1, 1,-1,0,0, 1,1, 0,0,-1, -1, 1,-1,-1,0,0, 0,1, 0,0,-1,
        // Right  (+X, tangent +Z)
         1,-1, 1, 1,0,0, 0,0, 0,0,1,   1,-1,-1, 1,0,0, 1,0, 0,0,1,
         1, 1,-1, 1,0,0, 1,1, 0,0,1,   1, 1, 1, 1,0,0, 0,1, 0,0,1,
        // Top    (+Y, tangent +X)
        -1, 1, 1, 0,1,0, 0,0, 1,0,0,   1, 1, 1, 0,1,0, 1,0, 1,0,0,
         1, 1,-1, 0,1,0, 1,1, 1,0,0,  -1, 1,-1, 0,1,0, 0,1, 1,0,0,
        // Bottom (-Y, tangent +X)
        -1,-1,-1, 0,-1,0, 0,0, 1,0,0,  1,-1,-1, 0,-1,0, 1,0, 1,0,0,
         1,-1, 1, 0,-1,0, 1,1, 1,0,0, -1,-1, 1, 0,-1,0, 0,1, 1,0,0,
    };
    std::vector<unsigned int> idx;
    for(int f=0;f<6;f++){unsigned int b=f*4;idx.insert(idx.end(),{b,b+1,b+2,b,b+2,b+3});}
    Mesh m; m.upload(v,idx); return m;
}

// ─── Checkerboard floor (pos+normal only, stride 6 floats) ───────────────────
GLuint boardVAO=0,boardVBO=0;
void buildBoard() {
    std::vector<float> v={
        -4,0,-4,0,1,0,  4,0,-4,0,1,0,  4,0,4,0,1,0,
        -4,0,-4,0,1,0,  4,0, 4,0,1,0, -4,0,4,0,1,0,
    };
    glGenVertexArrays(1,&boardVAO); glGenBuffers(1,&boardVBO);
    glBindVertexArray(boardVAO);
    glBindBuffer(GL_ARRAY_BUFFER,boardVBO);
    glBufferData(GL_ARRAY_BUFFER,v.size()*4,v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,(void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,24,(void*)12); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ─── Texture loading via SOIL ────────────────────────────────────────────────
GLuint loadTexture2D(const char* path) {
    GLuint tex = SOIL_load_OGL_texture(path,
        SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_NTSC_SAFE_RGB);
    if(!tex){
        fprintf(stderr,"WARNING: Could not load '%s': %s\n",path,SOIL_last_result());
        glGenTextures(1,&tex);
        glBindTexture(GL_TEXTURE_2D,tex);
        unsigned char grey[3]={128,128,128};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,1,1,0,GL_RGB,GL_UNSIGNED_BYTE,grey);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        return tex;
    }
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    return tex;
}

// Loads 6 face images into a GL cube map.
// Humus naming: posx.jpg negx.jpg posy.jpg negy.jpg posz.jpg negz.jpg
GLuint loadCubeMap(const char* faces[6]) {
    GLuint tex; glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP,tex);
    for(int i=0;i<6;i++){
        int w,h,ch;
        unsigned char* data=SOIL_load_image(faces[i],&w,&h,&ch,SOIL_LOAD_RGB);
        if(!data){
            fprintf(stderr,"WARNING: Cube face missing: %s (%s)\n",faces[i],SOIL_last_result());
            unsigned char sky[3]={100,149,237};
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB,1,1,0,GL_RGB,GL_UNSIGNED_BYTE,sky);
        } else {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,data);
            SOIL_free_image_data(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    return tex;
}

// ─── Camera ───────────────────────────────────────────────────────────────────
struct Camera {
    float x=0,y=2,z=10,yaw=-90,pitch=-10,roll=0;
    glm::vec3 forward(){
        float yr=yaw*PI/180,pr=pitch*PI/180;
        return glm::normalize(glm::vec3(cosf(pr)*cosf(yr),sinf(pr),cosf(pr)*sinf(yr)));
    }
    void moveRight(float d){ glm::vec3 f=forward(); x+=f.z*d; z-=f.x*d; }
    glm::mat4 viewMatrix(){
        glm::vec3 f=forward();
        float rr=roll*PI/180;
        glm::vec3 up(-sinf(rr),cosf(rr),0);
        return myLookAt(glm::vec3(x,y,z),glm::vec3(x,y,z)+f,up);
    }
    void reset(){ x=0;y=2;z=10;yaw=-90;pitch=-10;roll=0; }
} cam;

// ─── Input ───────────────────────────────────────────────────────────────────
void processInput(GLFWwindow* win, float dt) {
    float mv=3.0f*dt,rt=60.0f*dt,fn=30.0f*dt;
    bool sh=(glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS);
    bool ct=(glfwGetKey(win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_RIGHT_CONTROL)==GLFW_PRESS);
    if(glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win,true);
    if(glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS) cam.y+=mv;
    if(glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS) cam.y-=mv;
    if(glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS) cam.moveRight(-mv);
    if(glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS) cam.moveRight(mv);
    if(glfwGetKey(win,GLFW_KEY_Q)==GLFW_PRESS) cam.roll-=rt;
    if(glfwGetKey(win,GLFW_KEY_E)==GLFW_PRESS) cam.roll+=rt;
    if(glfwGetKey(win,GLFW_KEY_R)==GLFW_PRESS) cam.reset();
    if(sh){
        if(glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS)   cam.z-=mv;
        if(glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS) cam.z+=mv;
    } else if(ct){
        if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS)  cam.yaw-=fn;
        if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS) cam.yaw+=fn;
        if(glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS)  {cam.pitch+=fn;if(cam.pitch>89)cam.pitch=89;}
        if(glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){cam.pitch-=fn;if(cam.pitch<-89)cam.pitch=-89;}
    } else {
        if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS)  cam.yaw-=rt;
        if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS) cam.yaw+=rt;
        if(glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS)  {cam.pitch+=rt;if(cam.pitch>89)cam.pitch=89;}
        if(glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){cam.pitch-=rt;if(cam.pitch<-89)cam.pitch=-89;}
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    if(!glfwInit()){fprintf(stderr,"GLFW failed\n");return -1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    GLFWwindow* win=glfwCreateWindow(winW,winH,"Project 10 — Mapping Shaders",NULL,NULL);
    if(!win){fprintf(stderr,"Window failed\n");glfwTerminate();return -1;}
    glfwMakeContextCurrent(win);
    glewExperimental=GL_TRUE; glewInit();
    glEnable(GL_DEPTH_TEST);

    sphereProg = buildProgram(vertSrc, fragSphereSrc);
    envProg    = buildProgram(vertSrc, fragEnvSrc);
    bumpProg   = buildProgram(vertSrc, fragBumpSrc);
    boardProg  = buildProgram(vertSrc, fragBoardSrc);

    Mesh sphereMesh   = buildSphere(32,32);
    Mesh cylinderMesh = buildCylinder(32,2.0f);
    Mesh cubeMesh     = buildCube();
    buildBoard();

    // Bump-Map.jpg = greyscale height map for cylinder bump mapping
    // Bump-Picture.jpg = color brick photo for sphere diffuse
    GLuint bumpTex    = loadTexture2D("Bump-Map.jpg");
    GLuint pictureTex = loadTexture2D("Bump-Picture.jpg");

    // Cube map: 6 face images from Humus (posx/negx/posy/negy/posz/negz)
    const char* faces[6]={"posx.jpg","negx.jpg","posy.jpg","negy.jpg","posz.jpg","negz.jpg"};
    GLuint cubeMapTex = loadCubeMap(faces);

    float lastTime=0;
    while(!glfwWindowShouldClose(win)){
        float now=(float)glfwGetTime(),dt=now-lastTime; lastTime=now;
        processInput(win,dt);

        int w,h; glfwGetFramebufferSize(win,&w,&h);
        glViewport(0,0,w,h);
        glClearColor(0.1f,0.12f,0.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj=myPerspective(60.0f,(float)w/h,0.1f,200.0f);
        glm::mat4 view=cam.viewMatrix();
        float lx=6,ly=8,lz=6,cx=cam.x,cy=cam.y,cz=cam.z;

        // ── Checkerboard floor ─────────────────────────────────────────────
        glUseProgram(boardProg);
        glm::mat4 bm=glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(boardProg,"model"),1,GL_FALSE,glm::value_ptr(bm));
        glUniformMatrix4fv(glGetUniformLocation(boardProg,"view"), 1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(boardProg,"proj"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform3f(glGetUniformLocation(boardProg,"lightPos"),lx,ly,lz);
        glUniform3f(glGetUniformLocation(boardProg,"lightColor"),1,1,1);
        glUniform3f(glGetUniformLocation(boardProg,"viewPos"),cx,cy,cz);
        glBindVertexArray(boardVAO); glDrawArrays(GL_TRIANGLES,0,6); glBindVertexArray(0);

        // ── Sphere (LEFT) — Bump-Picture.jpg diffuse ──────────────────────
        glUseProgram(sphereProg);
        glm::mat4 sm=glm::translate(glm::mat4(1.0f),glm::vec3(-2.5f,1.2f,0));
        sm=glm::scale(sm,glm::vec3(1.2f));
        glUniformMatrix4fv(glGetUniformLocation(sphereProg,"model"),1,GL_FALSE,glm::value_ptr(sm));
        glUniformMatrix4fv(glGetUniformLocation(sphereProg,"view"), 1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sphereProg,"proj"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform3f(glGetUniformLocation(sphereProg,"lightPos"),lx,ly,lz);
        glUniform3f(glGetUniformLocation(sphereProg,"lightColor"),1,1,1);
        glUniform3f(glGetUniformLocation(sphereProg,"viewPos"),cx,cy,cz);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,pictureTex);
        glUniform1i(glGetUniformLocation(sphereProg,"diffuseTex"),0);
        sphereMesh.draw();

        // ── Cube (CENTER) — Environment (Cube) Map ────────────────────────
        glUseProgram(envProg);
        glm::mat4 cm=glm::translate(glm::mat4(1.0f),glm::vec3(0,1.0f,0));
        glUniformMatrix4fv(glGetUniformLocation(envProg,"model"),1,GL_FALSE,glm::value_ptr(cm));
        glUniformMatrix4fv(glGetUniformLocation(envProg,"view"), 1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(envProg,"proj"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform3f(glGetUniformLocation(envProg,"viewPos"),cx,cy,cz);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP,cubeMapTex);
        glUniform1i(glGetUniformLocation(envProg,"cubemap"),0);
        cubeMesh.draw();

        // ── Cylinder (RIGHT) — Bump Map (Bump-Map.jpg) ───────────────────
        glUseProgram(bumpProg);
        glm::mat4 cym=glm::translate(glm::mat4(1.0f),glm::vec3(2.5f,1.0f,0));
        cym=glm::scale(cym,glm::vec3(1.0f,1.5f,1.0f));
        glUniformMatrix4fv(glGetUniformLocation(bumpProg,"model"),1,GL_FALSE,glm::value_ptr(cym));
        glUniformMatrix4fv(glGetUniformLocation(bumpProg,"view"), 1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(bumpProg,"proj"), 1,GL_FALSE,glm::value_ptr(proj));
        glUniform3f(glGetUniformLocation(bumpProg,"objectColor"),0.8f,0.75f,0.7f);
        glUniform3f(glGetUniformLocation(bumpProg,"lightPos"),lx,ly,lz);
        glUniform3f(glGetUniformLocation(bumpProg,"lightColor"),1,1,1);
        glUniform3f(glGetUniformLocation(bumpProg,"viewPos"),cx,cy,cz);
        glUniform1f(glGetUniformLocation(bumpProg,"shininess"),32.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,bumpTex);
        glUniform1i(glGetUniformLocation(bumpProg,"bumpMap"),0);
        cylinderMesh.draw();

        glfwSwapBuffers(win); glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
