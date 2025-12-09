#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <random>
#include <ctime>
#include <cstdlib>

#include "../Shader.h"
#include "../stb_easy_font.h"

// Simple 2D movie theater simulation

struct Seat {
    int state; // 0 free,1 reserved,2 bought
    float x, y, w, h;
    int row, col;
};

struct Person {
    glm::vec2 pos;
    glm::vec2 target;      // current overall movement target (used for exiting)
    int seatIndex;
    bool seated;
    bool exiting;
    bool reachedRow;
    glm::vec2 rowTarget;   // intermediate: same X as entrance, Y of row
    glm::vec2 finalTarget; // center of seat
};

// Globals
int SCR_W = 1280;
int SCR_H = 720;
const int ROWS = 6;
const int COLS = 9; // 6x9 = 54 seats
std::vector<Seat> seats;
std::vector<Person> people;

bool overlay = true;

Shader* shader = nullptr;
unsigned int quadVAO = 0, quadVBO = 0, quadEBO = 0;

glm::mat4 proj;

void initQuad() {
    // positions and tex coords
    float verts[] = {
        // x, y,   u, v
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f
    };
    unsigned int indices[] = { 0,1,2, 2,3,0 };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &quadEBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void drawQuad(float x, float y, float w, float h, glm::vec4 color) {
    shader->use();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    // If your Shader::setMat4 expects glm::mat4 directly, pass proj/model instead of &proj[0][0]
    shader->setMat4("uProj", &proj[0][0]);
    shader->setMat4("uModel", &model[0][0]);
    shader->setVec4("uColor", color.r, color.g, color.b, color.a);
    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawText(float x, float y, const char* text, glm::vec4 color) {
    const int maxv = 99999;
    std::vector<char> tmp(maxv);
    int num_quads = stb_easy_font_print((float)x, (float)y, (char*)text, NULL, tmp.data(), (int)tmp.size());
    if (num_quads <= 0) return;

    float* fv = (float*)tmp.data();
    std::vector<float> triVerts;
    triVerts.reserve(num_quads * 6 * 2);
    for (int q = 0; q < num_quads; ++q) {
        float* v0 = fv + (q * 4 + 0) * 2;
        float* v1 = fv + (q * 4 + 1) * 2;
        float* v2 = fv + (q * 4 + 2) * 2;
        float* v3 = fv + (q * 4 + 3) * 2;
        // tri 1
        triVerts.push_back(v0[0]); triVerts.push_back(v0[1]);
        triVerts.push_back(v1[0]); triVerts.push_back(v1[1]);
        triVerts.push_back(v2[0]); triVerts.push_back(v2[1]);
        // tri 2
        triVerts.push_back(v2[0]); triVerts.push_back(v2[1]);
        triVerts.push_back(v3[0]); triVerts.push_back(v3[1]);
        triVerts.push_back(v0[0]); triVerts.push_back(v0[1]);
    }

    unsigned int tVAO, tVBO;
    glGenVertexArrays(1, &tVAO);
    glGenBuffers(1, &tVBO);
    glBindVertexArray(tVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tVBO);
    glBufferData(GL_ARRAY_BUFFER, triVerts.size() * sizeof(float), triVerts.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    shader->use();
    glm::mat4 model = glm::mat4(1.0f);
    shader->setMat4("uProj", &proj[0][0]);
    shader->setMat4("uModel", &model[0][0]);
    shader->setVec4("uColor", color.r, color.g, color.b, color.a);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(triVerts.size() / 2));
    glBindVertexArray(0);
    glDeleteBuffers(1, &tVBO);
    glDeleteVertexArrays(1, &tVAO);
}

int screenToGLY(double y) { return SCR_H - (int)y; }

void setupSeats() {
    seats.clear();
    float marginX = 120.0f;
    float marginY = 140.0f;
    float areaW = SCR_W - 2 * marginX;
    float areaH = SCR_H - 2 * marginY;
    float seatW = (areaW / (float)COLS) * 0.75f;
    float seatH = (areaH / (float)ROWS) * 0.5f;
    float spacingX = (areaW - seatW * COLS) / std::max(1, COLS - 1);
    float spacingY = (areaH - seatH * ROWS) / std::max(1, ROWS - 1);
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            float x = marginX + c * (seatW + spacingX);
            float y = marginY + r * (seatH + spacingY);
            Seat s; s.state = 0; s.x = x; s.y = y; s.w = seatW; s.h = seatH; s.row = r; s.col = c;
            seats.push_back(s);
        }
    }
}

int seatAtPos(double mx, double my) {
    int myg = screenToGLY(my);
    for (int i = 0; i < (int)seats.size(); ++i) {
        Seat& s = seats[i];
        if (mx >= s.x && mx <= s.x + s.w && myg >= s.y && myg <= s.y + s.h) return i;
    }
    return -1;
}

void toggleSeat(int idx) {
    if (idx < 0) return;
    if (seats[idx].state == 0) seats[idx].state = 1;
    else if (seats[idx].state == 1) seats[idx].state = 0;
}

void buyNSeats(int N) {
    if (N <= 0 || N > COLS) return; // invalid request or impossible to fit in a row

    // Search rows from last (closest to screen bottom) to first (top)
    for (int r = ROWS - 1; r >= 0; --r) {
        // j will be the right-most index of the candidate block
        for (int j = COLS - 1; j >= 0; --j) {
            int start = j - (N - 1); // left-most index of block
            if (start < 0) continue; // block would be out of bounds on the left

            bool ok = true;
            for (int c = start; c <= j; ++c) {
                int idx = r * COLS + c;
                if (seats[idx].state != 0) { // must be FREE (state == 0)
                    ok = false;
                    break;
                }
            }
            if (ok) {
                // Mark the contiguous block as bought
                for (int c = start; c <= j; ++c) {
                    int idx = r * COLS + c;
                    seats[idx].state = 2; // bought
                }
                return; // we stop after first block found (per spec)
            }
        }
    }
    // If we reach here, no suitable contiguous block was found anywhere.
}

// Simulation state
bool simulationRunning = false;
float filmTime = 20.0f; // seconds
float filmTimer = 0.0f;
int frameCounter = 0;
glm::vec4 filmColor = glm::vec4(0.05f, 0.05f, 0.2f, 1.0f);

// Entrance coordinates (top-left region)
glm::vec2 entrancePos;

void startSimulation() {
    people.clear();
    std::vector<int> seatIndices;
    for (int i = 0; i < (int)seats.size(); ++i) {
        if (seats[i].state == 1 || seats[i].state == 2) {
            seatIndices.push_back(i);
        }
    }
    if (seatIndices.empty()) return;

    int totalPossible = (int)seatIndices.size();
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(seatIndices.begin(), seatIndices.end(), g);

    std::uniform_int_distribution<int> distNum(1, totalPossible);
    int numPeople = distNum(g);

    for (int i = 0; i < numPeople; ++i) {
        int si = seatIndices[i];
        Person p;
        p.seated = false; p.exiting = false; p.seatIndex = si;
        // entrance at top-left small margin
        p.pos = entrancePos;
        Seat& s = seats[si];
        p.finalTarget = glm::vec2(s.x + s.w * 0.5f, s.y + s.h * 0.5f);
        // rowTarget: keep entrance X, target Y = row's center (move vertically toward row Y)
        p.rowTarget = glm::vec2(p.pos.x, p.finalTarget.y);
        p.reachedRow = false;
        p.target = p.finalTarget; // not used until exiting
        people.push_back(p);
    }

    if (!people.empty()) {
        simulationRunning = true;
        filmTimer = 0.0f;
        frameCounter = 0;
        overlay = false;
    }
}

void updateSimulation(float dt) {
    if (!simulationRunning) return;

    bool allSeated = true;
    // Move people toward their seats
    for (auto& p : people) {
        if (!p.seated && !p.exiting) {
            allSeated = false;
            if (!p.reachedRow) {
                // move vertically to rowTarget.y (keeping x = entrance.x)
                glm::vec2 dir = p.rowTarget - p.pos;
                float dist = glm::length(dir);
                if (dist < 2.0f) {
                    p.reachedRow = true;
                    p.pos = p.rowTarget;
                }
                else {
                    dir = glm::normalize(dir);
                    p.pos += dir * 200.0f * dt;
                }
            }
            else {
                // move horizontally (and small vertical correction) to seat finalTarget
                glm::vec2 dir = p.finalTarget - p.pos;
                float dist = glm::length(dir);
                if (dist < 2.0f) {
                    p.seated = true;
                    p.pos = p.finalTarget;
                }
                else {
                    dir = glm::normalize(dir);
                    p.pos += dir * 200.0f * dt;
                }
            }
        }
    }

    // Only if all are seated, run film timer
    if (allSeated) {
        filmTimer += dt;
        frameCounter++;
        if (frameCounter % 20 == 0) {
            // randomize color periodically
            std::random_device rd;
            std::mt19937 g(rd());
            std::uniform_real_distribution<float> d(0.1f, 0.7f);
            filmColor = glm::vec4(d(g), d(g), d(g), 1.0f);
        }
        if (filmTimer >= filmTime) {
            // start exiting: set target to entrance for each person
            for (auto& p : people) {
                p.exiting = true;
                p.seated = false;
                p.target = entrancePos;
                p.reachedRow = false;
            }
        }
    }

    // Handle exiting movement
    bool allGone = true;
    for (auto& p : people) {
        if (p.exiting) {
            glm::vec2 dir = p.target - p.pos;
            float dist = glm::length(dir);
            if (dist < 2.0f) {
                p.pos = p.target;
            }
            else {
                dir = glm::normalize(dir);
                p.pos += dir * 220.0f * dt;
            }
        }
        // person is considered gone if above top or at entrance exactly
        if (!(p.pos == p.target && p.exiting && glm::length(p.pos - p.target) < 0.01f)) {
            // if any person is still not at target (entrance) or still moving, not all gone
            if (!(p.exiting && glm::length(p.pos - p.target) < 2.0f)) allGone = false;
        }
    }

    if (allGone && filmTimer >= filmTime) {
        people.clear();
        for (auto& s : seats) s.state = 0;
        simulationRunning = false;
        overlay = true;
        // reset film color
        filmColor = glm::vec4(0.05f, 0.05f, 0.2f, 1.0f);
    }
}

void renderScene() {
    // background
    drawQuad(0, 0, (float)SCR_W, (float)SCR_H, glm::vec4(0.02f, 0.02f, 0.05f, 1.0f));
    // screen (at top)
    drawQuad(SCR_W * 0.25f, SCR_H - 160.0f, SCR_W * 0.5f, 100.0f, filmColor);
    // seats
    for (int i = 0; i < (int)seats.size(); ++i) {
        Seat& s = seats[i];
        glm::vec4 color = glm::vec4(0.2f, 0.4f, 0.9f, 1.0f); // free blue
        if (s.state == 1) color = glm::vec4(0.9f, 0.9f, 0.2f, 1.0f); // reserved yellow
        if (s.state == 2) color = glm::vec4(0.9f, 0.2f, 0.2f, 1.0f); // bought red
        drawQuad(s.x, s.y, s.w, s.h, color);
    }
    // people (body + head)
    for (auto& p : people) {
        drawQuad(p.pos.x - 8.0f, p.pos.y - 12.0f, 16.0f, 24.0f, glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
        drawQuad(p.pos.x - 6.0f, p.pos.y + 12.0f, 12.0f, 12.0f, glm::vec4(1.0f, 0.8f, 0.6f, 1.0f));
    }
    // overlay
    if (overlay) {
        drawQuad(0, 0, (float)SCR_W, (float)SCR_H, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
        drawText(40, SCR_H - 60, "Press Enter to start simulation, Left-click to reserve seats, 1-9 to buy.", glm::vec4(1, 1, 1, 1));
    }
    // student info
    drawQuad(8, 8, 360, 60, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    drawText(16, 28, "Ristic Andjela - 2D Movie Theater (Tema 7)", glm::vec4(1, 1, 1, 1));

    // draw custom cursor - a simple film camera icon
    double mx, my; glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
    int myg = screenToGLY(my);
    float cx = (float)mx, cy = (float)myg;
    // camera body
    drawQuad(cx - 12, cy - 8, 18, 12, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
    // lens
    drawQuad(cx + 6, cy - 6, 10, 8, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
    // reels
    drawQuad(cx - 18, cy + 2, 8, 8, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
    drawQuad(cx - 6, cy + 6, 8, 8, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
}

int main() {
    std::srand((unsigned int)std::time(nullptr));
    if (!glfwInit()) { std::cerr << "Failed to init GLFW\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    SCR_W = mode->width; SCR_H = mode->height;

    // create window in fullscreen (primary monitor)
    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "2D Movie Theater", primary, NULL);
    if (!window) { std::cerr << "Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n"; return -1;
    }

    glViewport(0, 0, SCR_W, SCR_H);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader = new Shader("shaders/quad.vert", "shaders/quad.frag");
    initQuad();

    proj = glm::ortho(0.0f, (float)SCR_W, 0.0f, (float)SCR_H, -1.0f, 1.0f);

    setupSeats();

    // define entrance (top-left small margin)
    entrancePos = glm::vec2(30.0f, SCR_H - 30.0f);

    // hide system cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    // --- ADDED: key edge trackers to prevent repeated actions while key is held ---
    bool keyWasPressed[10] = { false }; // index 0..9 corresponds to keys '0'..'9'
    bool enterWasPressed = false;
    // ---------------------------------------------------------------------------

    auto lastTime = std::chrono::high_resolution_clock::now();
    const double targetFrame = 1.0 / 75.0;

    while (!glfwWindowShouldClose(window)) {
        auto start = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(start - lastTime).count();
        lastTime = start;

        // input handling
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

        if (!simulationRunning) {
            // handle number keys with edge-detection (press-once behavior)
            for (int k = GLFW_KEY_1; k <= GLFW_KEY_9; ++k) {
                int state = glfwGetKey(window, k);
                int idx = k - GLFW_KEY_0; // maps GLFW_KEY_1->1, ... GLFW_KEY_9->9
                if (state == GLFW_PRESS && !keyWasPressed[idx]) {
                    // rising edge -> process once
                    keyWasPressed[idx] = true;
                    int n = idx; // number of seats requested
                    if (n >= 1 && n <= 9) { buyNSeats(n); overlay = false; }
                }
                else if (state == GLFW_RELEASE) {
                    // key released -> allow next press to trigger
                    keyWasPressed[idx] = false;
                }
            }

            // Enter: rising edge only
            int entState = glfwGetKey(window, GLFW_KEY_ENTER);
            if (entState == GLFW_PRESS && !enterWasPressed) {
                enterWasPressed = true;
                startSimulation();
                overlay = false;
            }
            else if (entState == GLFW_RELEASE) {
                enterWasPressed = false;
            }
        }

        // mouse click handling (left-click edge detection)
        static bool wasLeft = false;
        int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        if (state == GLFW_PRESS && !wasLeft) {
            double mx, my; glfwGetCursorPos(window, &mx, &my);
            int idx = seatAtPos(mx, my);
            if (idx >= 0 && !simulationRunning) { toggleSeat(idx); overlay = false; }
        }
        wasLeft = (state == GLFW_PRESS);

        // update simulation
        updateSimulation((float)elapsed);

        // render
        glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
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
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (quadEBO) glDeleteBuffers(1, &quadEBO);

    glfwTerminate();
    return 0;
}