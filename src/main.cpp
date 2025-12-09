#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include "Shader.h"
#include "stb_easy_font.h"

// Simple 2D movie theater simulation

struct Seat { int state; // 0 free,1 reserved,2 bought
    float x,y,w,h; int row,col; };

struct Person { glm::vec2 pos; glm::vec2 target; int seatIndex; bool seated; bool exiting; };

// Globals
int SCR_W = 1280;
int SCR_H = 720;
const int ROWS = 6;
const int COLS = 9; // 6x9 = 54 seats
std::vector<Seat> seats;
std::vector<Person> people;

bool overlay = true;

Shader* shader = nullptr;
unsigned int quadVAO=0, quadVBO=0;

glm::mat4 proj;

void initQuad(){
    // positions and tex coords
    float verts[] = {
        // x, y,   u, v
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f
    };
    unsigned int indices[] = {0,1,2, 2,3,0};
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    unsigned int EBO;
    glGenBuffers(1, &EBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

void drawQuad(float x, float y, float w, float h, glm::vec4 color){
    shader->use();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x,y,0.0f));
    model = glm::scale(model, glm::vec3(w,h,1.0f));
    shader->setMat4("uProj", &proj[0][0]);
    shader->setMat4("uModel", &model[0][0]);
    shader->setVec4("uColor", color.r, color.g, color.b, color.a);
    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawText(float x, float y, const char* text, glm::vec4 color){
    // generate triangles using stb_easy_font_print
    static std::vector<float> buf;
    buf.clear();
    // We'll request vertex data into a temporary buffer
    const int maxv = 99999;
    static std::vector<char> tmp;
    tmp.resize(maxv);
    // stb_easy_font_print from our minimal header returns number of triangles
    int num_tri = stb_easy_font_print(x, y, text, NULL, tmp.data(), (int)tmp.size());
    if (num_tri <= 0) return;
    // tmp contains floats as in our minimal impl
    // We'll draw using GL_TRIANGLES by uploading to a VBO
    unsigned int tVAO, tVBO;
    glGenVertexArrays(1, &tVAO);
    glGenBuffers(1, &tVBO);
    glBindVertexArray(tVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tVBO);
    glBufferData(GL_ARRAY_BUFFER, num_tri*3*2*sizeof(float), tmp.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    shader->use();
    glm::mat4 model = glm::mat4(1.0f);
    shader->setMat4("uProj", &proj[0][0]);
    shader->setMat4("uModel", &model[0][0]);
    shader->setVec4("uColor", color.r, color.g, color.b, color.a);
    glDrawArrays(GL_TRIANGLES, 0, num_tri*3);
    glBindVertexArray(0);
    glDeleteBuffers(1, &tVBO);
    glDeleteVertexArrays(1, &tVAO);
}

int screenToGLY(double y){ return SCR_H - (int)y; }

void setupSeats(){
    seats.clear();
    float marginX = 120.0f;
    float marginY = 140.0f;
    float areaW = SCR_W - 2*marginX;
    float areaH = SCR_H - 2*marginY;
    float seatW = areaW / (float)COLS * 0.75f;
    float seatH = areaH / (float)ROWS * 0.5f;
    float spacingX = (areaW - seatW*COLS) / (COLS-1);
    float spacingY = (areaH - seatH*ROWS) / (ROWS-1);
    for(int r=0;r<ROWS;++r){
        for(int c=0;c<COLS;++c){
            float x = marginX + c * (seatW + spacingX);
            float y = marginY + r * (seatH + spacingY);
            Seat s; s.state = 0; s.x = x; s.y = y; s.w = seatW; s.h = seatH; s.row=r; s.col=c;
            seats.push_back(s);
        }
    }
}

int seatAtPos(double mx, double my){
    int myg = screenToGLY(my);
    for(int i=0;i<(int)seats.size();++i){
        Seat &s = seats[i];
        if (mx >= s.x && mx <= s.x + s.w && myg >= s.y && myg <= s.y + s.h) return i;
    }
    return -1;
}

void toggleSeat(int idx){ if (idx<0) return; if (seats[idx].state==0) seats[idx].state=1; else if (seats[idx].state==1) seats[idx].state=0; }

void buyNSeats(int N){
    int r = ROWS-1; // last row
    // start from rightmost seat
    for(int start = COLS - N; start >= 0; --start){
        bool ok = true;
        for(int c=0;c<N;++c){
            int idx = r*COLS + (start+c);
            if (seats[idx].state==2) { ok=false; break; }
        }
        if (ok){
            for(int c=0;c<N;++c){ seats[r*COLS + (start+c)].state = 2; }
            return;
        }
    }
}

bool simulationRunning = false;
float filmTime = 20.0f; // seconds
float filmTimer = 0.0f;
int frameCounter = 0;
glm::vec4 filmColor = glm::vec4(0.05f,0.05f,0.2f,1.0f);

void startSimulation(){
    // collect bought seats
    people.clear();
    for(int i=0;i<(int)seats.size();++i){
        if (seats[i].state==2){
            Person p; p.seated=false; p.exiting=false; p.seatIndex=i;
            p.pos = glm::vec2(SCR_W/2.0f, -30.0f);
            Seat &s = seats[i];
            p.target = glm::vec2(s.x + s.w*0.5f, s.y + s.h*0.5f);
            people.push_back(p);
            // mark seat as reserved during simulation for visual
            seats[i].state = 2; // keep as bought
        }
    }
    if (!people.empty()){
        simulationRunning = true;
        filmTimer = 0.0f;
        frameCounter = 0;
    }
}

void updateSimulation(float dt){
    if (!simulationRunning) return;
    bool allSeated = true;
    for(auto &p : people){
        if (!p.seated && !p.exiting){
            // move towards target
            glm::vec2 dir = p.target - p.pos;
            float dist = glm::length(dir);
            if (dist < 2.0f){ p.seated = true; p.pos = p.target; }
            else { dir = glm::normalize(dir); p.pos += dir * 200.0f * dt; }
        }
        if (!p.seated) allSeated = false;
    }
    if (allSeated){
        // run film
        filmTimer += dt;
        frameCounter++;
        if (frameCounter % 20 == 0){
            filmColor = glm::vec4((rand()%100)/200.0f + 0.1f, (rand()%100)/200.0f + 0.05f, (rand()%100)/200.0f + 0.1f, 1.0f);
        }
        if (filmTimer >= filmTime){
            // start exiting
            for(auto &p: people){ p.exiting = true; p.seated = false; p.target = glm::vec2(SCR_W/2.0f, -30.0f); }
        }
    }
    bool allGone = true;
    for(auto &p: people){
        if (p.exiting){
            glm::vec2 dir = p.target - p.pos;
            float dist = glm::length(dir);
            if (dist < 2.0f) { p.pos = p.target; }
            else { dir = glm::normalize(dir); p.pos += dir * 220.0f * dt; }
        }
        if (!(p.pos.y < -20.0f)) allGone = false;
    }
    if (allGone && filmTimer >= filmTime){
        // reset
        people.clear();
        for(auto &s: seats) s.state = 0;
        simulationRunning = false;
        overlay = true;
    }
}

void renderScene(){
    // background
    drawQuad(0,0,(float)SCR_W,(float)SCR_H, glm::vec4(0.02f,0.02f,0.05f,1.0f));
    // screen (at top)
    drawQuad(SCR_W*0.25f, SCR_H - 160.0f, SCR_W*0.5f, 100.0f, filmColor);
    // seats
    for(int i=0;i<(int)seats.size();++i){
        Seat &s = seats[i];
        glm::vec4 color = glm::vec4(0.2f,0.4f,0.9f,1.0f); // free blue
        if (s.state==1) color = glm::vec4(0.9f,0.9f,0.2f,1.0f); // reserved yellow
        if (s.state==2) color = glm::vec4(0.9f,0.2f,0.2f,1.0f); // bought red
        drawQuad(s.x, s.y, s.w, s.h, color);
    }
    // people
    for(auto &p: people){
        drawQuad(p.pos.x - 8.0f, p.pos.y - 12.0f, 16.0f, 24.0f, glm::vec4(0.2f,0.8f,0.2f,1.0f));
        // head
        drawQuad(p.pos.x - 6.0f, p.pos.y + 12.0f, 12.0f, 12.0f, glm::vec4(1.0f,0.8f,0.6f,1.0f));
    }
    // overlay
    if (overlay){
        drawQuad(0,0,(float)SCR_W,(float)SCR_H, glm::vec4(0.0f,0.0f,0.0f,0.6f));
        drawText(40, SCR_H-60, "Press Enter to start simulation, Left-click to reserve seats.", glm::vec4(1,1,1,1));
    }
    // student info
    drawQuad(8,8,260,60, glm::vec4(0.0f,0.0f,0.0f,0.5f));
    drawText(16, 28, "Ristic Andjela - 2D Movie Theater (Tema 7)", glm::vec4(1,1,1,1));

    // draw custom cursor - a simple film camera icon
    double mx, my; glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
    int myg = screenToGLY(my);
    float cx = (float)mx, cy = (float)myg;
    // camera body
    drawQuad(cx - 12, cy - 8, 18, 12, glm::vec4(0.1f,0.1f,0.1f,1.0f));
    // lens
    drawQuad(cx + 6, cy - 6, 10, 8, glm::vec4(0.2f,0.2f,0.2f,1.0f));
    // reels
    drawQuad(cx - 18, cy + 2, 8, 8, glm::vec4(0.2f,0.2f,0.2f,1.0f));
    drawQuad(cx - 6, cy + 6, 8, 8, glm::vec4(0.2f,0.2f,0.2f,1.0f));
}

int main(){
    srand((unsigned int)time(NULL));
    if (!glfwInit()){ std::cerr<<"Failed to init GLFW\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    SCR_W = mode->width; SCR_H = mode->height;

    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "2D Movie Theater", primary, NULL);
    if (!window){ std::cerr<<"Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr<<"Failed to init GLAD\n"; return -1; }

    glViewport(0,0,SCR_W,SCR_H);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader = new Shader("shaders/quad.vert", "shaders/quad.frag");
    initQuad();

    proj = glm::ortho(0.0f, (float)SCR_W, 0.0f, (float)SCR_H, -1.0f, 1.0f);

    setupSeats();

    // hide system cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    auto lastTime = std::chrono::high_resolution_clock::now();
    const double targetFrame = 1.0/75.0;

    while(!glfwWindowShouldClose(window)){
        auto start = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(start - lastTime).count();
        lastTime = start;

        // input handling
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        if (!simulationRunning){
            for(int k=GLFW_KEY_1;k<=GLFW_KEY_9;++k){ if (glfwGetKey(window,k)==GLFW_PRESS){ int n = k - GLFW_KEY_0; buyNSeats(n); overlay=false; } }
            if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS){ startSimulation(); overlay=false; }
        }

        // mouse click handling
        static bool wasLeft=false;
        int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        if (state==GLFW_PRESS && !wasLeft){
            double mx,my; glfwGetCursorPos(window, &mx, &my);
            int idx = seatAtPos(mx,my);
            if (idx>=0){ toggleSeat(idx); overlay=false; }
        }
        wasLeft = (state==GLFW_PRESS);

        // update simulation
        updateSimulation((float)elapsed);

        // render
        glClearColor(0.02f,0.02f,0.06f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderScene();

        glfwSwapBuffers(window);

        // simple frame limiter
        auto end = std::chrono::high_resolution_clock::now();
        double frameTime = std::chrono::duration<double>(end - start).count();
        double sleepTime = targetFrame - frameTime;
        if (sleepTime > 0) std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
    }

    delete shader;
    glfwTerminate();
    return 0;
}
