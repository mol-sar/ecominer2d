#define _CRT_SECURE_NO_WARNINGS
#if defined(_WIN32)
#include <windows.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "freeglut.lib")
#endif

// Include FreeGLUT
#if __has_include(<GL/freeglut.h>)
#include <GL/freeglut.h>
#elif __has_include(<freeglut.h>)
#include <freeglut.h>
#else
#error "Cannot find freeglut.h. Please install freeglut and ensure your include directories are set."
#endif

#include <cmath>
#include <string>
#include <vector>
#include <ctime>
#include <mmsystem.h>
#include <iostream>

#pragma comment(lib, "winmm.lib")

// ====================== GAME CONFIGURATION ======================
const float GRAVITY = -0.0025f;
const float JUMP_STRENGTH = 0.055f;
const float MOVEMENT_SPEED = 0.015f;
const float FRICTION = 0.85f;
const float MAX_SPEED = 0.03f;
int menuIndex = 0;   // 0 = Start, 1 = Quit
float menuPulse = 0.0f;

// TRANSITION VARIABLES
float fadeAlpha = 0.0f;
bool isFadingOut = false;
bool isFadingIn = false;

// ====================== GAME STATE ======================
enum GameState { MENU, PLAYING, PAUSED, GAMEOVER, VICTORY };
GameState gameState = MENU;

// Input State
bool keys[256] = { false };
bool specialKeysState[256] = { false };
bool isMouseHeld = false;

// ====================== PARTICLE SYSTEM =================
struct Particle {
    float x, y, vx, vy, life, r, g, b;
};
std::vector<Particle> particles;

struct Flower {
    float x, y; 
    int type; };
std::vector<Flower> flowers;

struct Fence {
    float x, y;
    float scale;
};
std::vector<Fence> fences;

struct Tree {
    float x, y;
    float scale;
    int type;           // 0 = Oak/Round, 1 = Pine/Evergreen, 2 = Birch/Tall
    float greenTint;    // Slight variation in foliage color
};
std::vector<Tree> trees;

struct Stalactite {
    float x, y;
    bool active;
};
std::vector<Stalactite> stalactites;

// ====================== CAMERA ==========================
float cameraX = 0.0f;

// ====================== GAME OBJECTS ====================
float groundY = -0.6f;
float playerX = -0.8f;
float playerY = -0.6f;
float playerVelX = 0.0f;
float playerVelY = 0.0f;
bool isJumping = false;
bool facingRight = true;

struct MiningStone {
    float x, y;
    int health;
    bool active;
    bool containsGem;
};
const int MAX_STONES = 10;
MiningStone stones[MAX_STONES];

// Animation
float armAngle = 0.0f;
bool isMining = false;
float pickaxeAngle = 0.0f;
bool pickaxeSwinging = false;

// Level Data
int score = 0;
int level = 1;
const int MAX_LEVEL = 10;
float trashX = 0.0f, trashY = 0.0f;
bool trashActive = false;
float rockX = 0.0f, rockY = 0.0f;

// ====================== HELPER FUNCTIONS =================
float getGroundY(float x) {
    return groundY;
}

void playBackgroundMusic(const char* filename) {
    std::string cmd = "open \"" + std::string(filename) + "\" type waveaudio alias bgm";
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    mciSendStringA("play bgm repeat", NULL, 0, NULL);
}

void stopBackgroundMusic() {
    mciSendStringA("stop bgm", NULL, 0, NULL);
    mciSendStringA("close bgm", NULL, 0, NULL);
}

void playSFX(const char* filename) {
    PlaySoundA(filename, NULL, SND_FILENAME | SND_ASYNC);
}

// ====================== UTILS ===========================
void drawText(float x, float y, std::string text, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (char c : text) glutBitmapCharacter(font, c);
}

void drawCircle(float cx, float cy, float r, int num_segments = 20) {
    glBegin(GL_POLYGON);
    for (int i = 0; i < num_segments; i++) {
        float theta = 2.0f * 3.1415926f * float(i) / float(num_segments);
        glVertex2f(r * cosf(theta) + cx, r * sinf(theta) + cy);
    }
    glEnd();
}

void drawGradientRect(float x1, float y1, float x2, float y2, float r1, float g1, float b1, float r2, float g2, float b2) {
    glBegin(GL_QUADS);
    glColor3f(r1, g1, b1); glVertex2f(x1, y2);
    glColor3f(r1, g1, b1); glVertex2f(x2, y2);
    glColor3f(r2, g2, b2); glVertex2f(x2, y1);
    glColor3f(r2, g2, b2); glVertex2f(x1, y1);
    glEnd();
}

// ====================== PARTICLES =======================
void createMiningEffect(float x, float y, float r, float g, float b) {
    for (int i = 0; i < 10; i++) {
        float vx = ((rand() % 100) / 5000.0f) - 0.01f;
        float vy = ((rand() % 100) / 3000.0f);
        particles.push_back({ x, y, vx, vy, 1.0f, r, g, b });
    }
}

void spawnParticles(float x, float y, float r, float g, float b, int count) {
    for (int i = 0; i < count; ++i) {
        float vx = ((rand() % 100) / 5000.0f) - 0.01f;
        float vy = ((rand() % 100) / 3000.0f);
        particles.push_back({ x, y, vx, vy, 1.0f, r, g, b });
    }
}

void updateParticles() {
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy -= 0.0015f;
        particles[i].life -= 0.03f;
        if (particles[i].life <= 0) {
            particles.erase(particles.begin() + i);
            i--;
        }
    }
}

void updateStalactites() {
    for (auto& st : stalactites) {
        if (!st.active) continue;

        // Apply falling gravity
        st.y -= 0.004f;

        if (st.y < getGroundY(st.x)) {
            st.active = false;
            spawnParticles(st.x, getGroundY(st.x), 0.4f, 0.4f, 0.45f, 12);
        }
    }
}

void drawParticles() {
    glPointSize(4.0f);
    glBegin(GL_POINTS);
    for (auto& p : particles) {
        glColor4f(p.r, p.g, p.b, p.life);
        glVertex2f(p.x, p.y);
    }
    glEnd();
}

void drawFenceSegment(float x, float y, float scale) {
    float w = 0.02f * scale;   // Post width
    float h = 0.2f * scale;    // Post height
    float gap = 0.15f * scale; // Distance between posts

    // Post 1 (Left)
    drawGradientRect(x, y, x + w, y + h,
        0.55f, 0.35f, 0.2f,    // Top Color
        0.35f, 0.20f, 0.1f);   // Bottom Color

    // Post 2 (Right)
    drawGradientRect(x + gap, y, x + gap + w, y + h,
        0.55f, 0.35f, 0.2f,
        0.35f, 0.20f, 0.1f);

    // Horizontal Rails
    glColor3f(0.45f, 0.25f, 0.1f);
    glRectf(x, y + (h * 0.7f), x + gap + w, y + (h * 0.8f));
    glRectf(x, y + (h * 0.3f), x + gap + w, y + (h * 0.4f));

    // Post Caps
    glColor3f(0.6f, 0.4f, 0.25f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x, y + h); glVertex2f(x + w, y + h); glVertex2f(x + (w / 2), y + h + 0.02f);
    glVertex2f(x + gap, y + h); glVertex2f(x + gap + w, y + h); glVertex2f(x + gap + (w / 2), y + h + 0.02f);
    glEnd();
}

// Plant trees squarely on the ground plane
void drawTree(float x, float y, float scale, int type, float tint) {
    if (type == 0) {
        // === TYPE 0: BUSHY OAK (Round layered canopy) ===
        // Trunk
        drawGradientRect(x - 0.022f * scale, y, x + 0.022f * scale, y + 0.22f * scale,
            0.50f, 0.30f, 0.15f, 0.30f, 0.16f, 0.08f);

        // Canopy with randomized green tone
        float g = 0.62f + tint;
        glColor3f(0.10f, g, 0.16f);
        drawCircle(x, y + 0.30f * scale, 0.095f * scale, 14);
        drawCircle(x - 0.065f * scale, y + 0.26f * scale, 0.075f * scale, 14);
        drawCircle(x + 0.065f * scale, y + 0.26f * scale, 0.075f * scale, 14);

        // Highlight top leaf cluster
        glColor3f(0.18f, g + 0.12f, 0.22f);
        drawCircle(x, y + 0.37f * scale, 0.065f * scale, 12);
    }
    else if (type == 1) {
        // === TYPE 1: CONIFEROUS PINE (Pointy tiered triangles) ===
        // Narrow dark trunk
        drawGradientRect(x - 0.015f * scale, y, x + 0.015f * scale, y + 0.16f * scale,
            0.42f, 0.24f, 0.12f, 0.25f, 0.12f, 0.06f);

        // Tier 1 (Bottom branch tier)
        glColor3f(0.08f, 0.38f + tint, 0.18f);
        glBegin(GL_TRIANGLES);
        glVertex2f(x - 0.11f * scale, y + 0.14f * scale);
        glVertex2f(x + 0.11f * scale, y + 0.14f * scale);
        glVertex2f(x, y + 0.32f * scale);
        glEnd();

        // Tier 2 (Middle branch tier)
        glColor3f(0.12f, 0.46f + tint, 0.22f);
        glBegin(GL_TRIANGLES);
        glVertex2f(x - 0.085f * scale, y + 0.24f * scale);
        glVertex2f(x + 0.085f * scale, y + 0.24f * scale);
        glVertex2f(x, y + 0.42f * scale);
        glEnd();

        // Tier 3 (Top needle peak)
        glColor3f(0.16f, 0.54f + tint, 0.26f);
        glBegin(GL_TRIANGLES);
        glVertex2f(x - 0.06f * scale, y + 0.34f * scale);
        glVertex2f(x + 0.06f * scale, y + 0.34f * scale);
        glVertex2f(x, y + 0.52f * scale);
        glEnd();
    }
    else {
        // === TYPE 2: SLENDER BIRCH (Tall white/grey trunk with high compact canopy) ===
        // Tall pale trunk
        drawGradientRect(x - 0.014f * scale, y, x + 0.014f * scale, y + 0.32f * scale,
            0.88f, 0.88f, 0.85f, 0.65f, 0.65f, 0.62f);

        // Birch bark dark notches
        glColor3f(0.2f, 0.2f, 0.2f);
        glRectf(x - 0.014f * scale, y + 0.08f * scale, x + 0.005f * scale, y + 0.09f * scale);
        glRectf(x - 0.005f * scale, y + 0.18f * scale, x + 0.014f * scale, y + 0.19f * scale);

        // Compact high canopy
        float g = 0.58f + tint;
        glColor3f(0.15f, g, 0.15f);
        drawCircle(x, y + 0.38f * scale, 0.075f * scale, 12);
        drawCircle(x - 0.035f * scale, y + 0.35f * scale, 0.055f * scale, 10);
        drawCircle(x + 0.035f * scale, y + 0.35f * scale, 0.055f * scale, 10);
    }
}

// ====================== INITIALIZATION ==================
void initFlowers() {
    flowers.clear();
    for (int i = 0; i < 30; i++) {
        float x = -2.0f + (rand() % 600) / 100.0f;
        float y = groundY + 0.05f + ((rand() % 10) / 100.0f);
        flowers.push_back({ x, y, rand() % 3 });
    }
}

void initFences() {
    fences.clear();
    for (int i = 0; i < 15; i++) {
        float x = -2.0f + (rand() % 600) / 100.0f;
        if (x > -1.0f && x < 1.0f) continue;
        fences.push_back({ x, groundY, 0.8f + ((rand() % 4) / 10.0f) });
    }
}

void initTrees() {
    trees.clear();
    // Spawn 50 trees for a dense forest backdrop
    for (int i = 0; i < 50; i++) {
        // Distribute trees across the entire horizontal level space
        float x = -4.0f + (rand() % 1900) / 100.0f;

        // Generates scales from 0.55 (small sapling) up to 1.65 (giant old-growth tree)
        float scale = 1.5f + ((rand() % 110) / 100.0f);

        int type = rand() % 3;                       // 0 = Oak, 1 = Pine, 2 = Birch
        float tint = ((rand() % 30) - 15) / 100.0f;  // Tint spread from -0.15 to +0.15

        trees.push_back({ x, groundY, scale, type, tint });
    }
}

void initStalactites() {
    stalactites.clear();
    for (int i = 0; i < 16; i++) {
        float x = -1.8f + (rand() % 400) / 100.0f;
        float y = 0.8f + ((rand() % 200) / 1000.0f);
        stalactites.push_back({ x, y, true });
    }
}

void initLevel() {
    playerX = -0.8f;
    playerY = groundY;
    playerVelX = 0;
    playerVelY = 0;
    cameraX = -0.8f;

    initFlowers();
    initFences();
    initTrees();
    initStalactites();

    for (int i = 0; i < MAX_STONES; i++) {
        stones[i].x = -0.5f + static_cast<float>(rand() % 400) / 100.0f;
        stones[i].y = groundY + 0.04f;
        stones[i].health = 3;
        stones[i].active = (rand() % 100 < 80);
        stones[i].containsGem = (rand() % 3 == 0);
    }

    rockX = 0.5f + (rand() % 300) / 100.0f;
    rockY = groundY;
    trashX = 0.0f + (rand() % 300) / 100.0f;
    trashY = -0.4f;
    trashActive = true;
}

void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    initLevel();
}

// ====================== PLAYER DRAWING ===================
void drawPlayer() {
    float walkAnim = 0.0f;
    if (fabs(playerVelX) > 0.001f) walkAnim = sinf(playerX * 30.0f);

    glPushMatrix();
    glTranslatef(playerX, playerY, 0);
    if (!facingRight) glScalef(-1.0f, 1.0f, 1.0f);

    // Backpack
    drawGradientRect(-0.05f, -0.02f, -0.02f, 0.07f, 0.5f, 0.5f, 0.5f, 0.3f, 0.3f, 0.3f);
    glColor3f(0.2f, 0.2f, 0.2f); glRectf(-0.045f, 0.07f, -0.025f, 0.075f);

    // Legs
    glPushMatrix(); glTranslatef(0.0f, -0.05f, 0.0f); glRotatef(-walkAnim * 30.0f, 0, 0, 1);
    drawGradientRect(-0.015f, -0.08f, 0.015f, 0.0f, 0.2f, 0.2f, 0.6f, 0.1f, 0.1f, 0.3f);
    glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f, -0.05f, 0.0f); glRotatef(walkAnim * 30.0f, 0, 0, 1);
    drawGradientRect(-0.015f, -0.08f, 0.015f, 0.0f, 0.2f, 0.2f, 0.6f, 0.1f, 0.1f, 0.3f);
    glPopMatrix();

    // Body
    drawGradientRect(-0.035f, -0.05f, 0.035f, 0.05f, 0.3f, 0.8f, 0.3f, 0.1f, 0.4f, 0.1f);
    glColor3f(0.1f, 0.1f, 0.1f); glRectf(-0.035f, -0.06f, 0.035f, -0.04f);
    glColor3f(0.8f, 0.8f, 0.8f); glRectf(-0.01f, -0.06f, 0.01f, -0.04f);

    // Head
    glColor3f(1.0f, 0.8f, 0.6f); drawCircle(0.0f, 0.08f, 0.04f, 20);
    glColor3f(0.2f, 0.2f, 0.2f); glRectf(-0.04f, 0.08f, 0.03f, 0.10f); // Strap
    glColor3f(0.4f, 0.8f, 1.0f); drawCircle(0.015f, 0.09f, 0.012f, 10);
    glColor3f(0.4f, 0.8f, 1.0f); drawCircle(-0.01f, 0.09f, 0.012f, 10);
    glColor3f(1.0f, 1.0f, 1.0f); drawCircle(0.018f, 0.094f, 0.004f, 5);

    // Helmet
    glBegin(GL_POLYGON);
    for (int i = 0; i <= 20; i++) {
        float angle = 3.14159f * (float)i / 20.0f;
        float yPos = sinf(angle);
        glColor3f(1.0f * yPos, 0.8f * yPos, 0.0f);
        glVertex2f(0.05f * cosf(angle), 0.11f + 0.05f * yPos);
    }
    glEnd();
    glColor3f(1.0f, 1.0f, 0.7f); drawCircle(0.02f, 0.14f, 0.01f, 10);

    // Arm
    glPushMatrix(); glTranslatef(0.0f, 0.02f, 0.0f);
    if (isMining) glRotatef(pickaxeAngle, 0, 0, 1);
    else glRotatef(-walkAnim * 20.0f, 0, 0, 1);
    drawGradientRect(-0.015f, -0.06f, 0.015f, 0.0f, 0.2f, 0.6f, 0.2f, 0.1f, 0.3f, 0.1f);
    glColor3f(1.0f, 0.8f, 0.6f); drawCircle(0.0f, -0.07f, 0.012f, 10);

    // Pickaxe
    glPushMatrix(); glTranslatef(0.0f, -0.07f, 0.0f);
    if (isMining) glRotatef(-90.0f, 0, 0, 1); else glRotatef(-10.0f, 0, 0, 1);
    drawGradientRect(0.0f, -0.01f, 0.14f, 0.01f, 0.5f, 0.3f, 0.1f, 0.3f, 0.1f, 0.0f);
    glBegin(GL_TRIANGLES);
    glColor3f(0.9f, 0.9f, 0.9f); glVertex2f(0.13f, 0.01f);
    glColor3f(0.4f, 0.4f, 0.4f); glVertex2f(0.13f, -0.08f);
    glColor3f(0.6f, 0.6f, 0.6f); glVertex2f(0.09f, 0.0f);
    glEnd();
    glBegin(GL_TRIANGLES);
    glColor3f(0.9f, 0.9f, 0.9f); glVertex2f(0.13f, 0.01f);
    glColor3f(0.5f, 0.5f, 0.5f); glVertex2f(0.13f, 0.04f);
    glColor3f(0.6f, 0.6f, 0.6f); glVertex2f(0.11f, 0.0f);
    glEnd();
    glPopMatrix(); glPopMatrix(); glPopMatrix();
}

// ====================== WORLD ELEMENTS ===================
void drawOrganicRock(float x, float y, float size, int health, bool isGold, int id) {
    srand(id * 500);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 8; i++) {
        float angle = i * (2.0f * 3.14159f / 8.0f);
        float jaggedRadius = size + ((rand() % 40) / 1000.0f);
        float yFactor = sinf(angle);

        if (isGold && health < 3) {
            float brightness = 0.6f + (yFactor * 0.2f);
            glColor3f(brightness, brightness * 0.8f, 0.2f);
        }
        else {
            float brightness = 0.5f + (yFactor * 0.2f);
            glColor3f(brightness, brightness, brightness);
        }
        glVertex2f(x + cosf(angle) * jaggedRadius, y + sinf(angle) * jaggedRadius);
    }
    glEnd();

    if (health <= 2) {
        glColor3f(0.1f, 0.1f, 0.1f); glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(x, y + size / 2); glVertex2f(x, y - size / 4);
        if (health <= 1) { glVertex2f(x, y); glVertex2f(x + size / 2, y + size / 4); }
        glEnd(); glLineWidth(1.0f);
    }
}

void drawCloud(float x, float y, float size) {
    
    glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
    drawCircle(x, y, size, 18);
    drawCircle(x, y + size * 0.35f, size * 0.85f, 18); // Tall top puff
    drawCircle(x - size * 0.85f, y - size * 0.1f, size * 0.75f, 16);
    drawCircle(x + size * 0.85f, y - size * 0.1f, size * 0.75f, 16);
    drawCircle(x - size * 1.4f, y - size * 0.2f, size * 0.55f, 14);
    drawCircle(x + size * 1.4f, y - size * 0.2f, size * 0.55f, 14);
    glRectf(x - size * 1.3f, y - size * 0.65f, x + size * 1.3f, y);
}

void drawStalactites() {
    for (auto& st : stalactites) {
        if (!st.active) continue;

        float brightness = 0.35f + 0.15f * sinf(st.x + (float)level);
        glColor3f(brightness, brightness * 0.8f, 0.35f);

        glBegin(GL_TRIANGLES);
        glVertex2f(st.x, st.y);
        glVertex2f(st.x - 0.03f, st.y + 0.10f);
        glVertex2f(st.x + 0.03f, st.y + 0.10f);
        glEnd();
    }
}

void drawStalactite(float x, float y, float width, float height) {
    glBegin(GL_TRIANGLES);
    glColor3f(0.1f, 0.1f, 0.1f); glVertex2f(x - width, y);
    glColor3f(0.1f, 0.1f, 0.1f); glVertex2f(x + width, y);
    glColor3f(0.3f, 0.3f, 0.35f); glVertex2f(x, y - height);
    glEnd();
}

void drawMountain(float x, float y, float w, float h) {
    glBegin(GL_TRIANGLES);
    glColor3f(0.4f, 0.2f, 0.1f); glVertex2f(x, y + h);
    glColor3f(0.2f, 0.1f, 0.05f); glVertex2f(x - w, y);
    glColor3f(0.15f, 0.05f, 0.05f); glVertex2f(x + w, y);
    glEnd();
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(x, y + h);
    glColor3f(0.9f, 0.9f, 0.9f); glVertex2f(x - w * 0.3f, y + h * 0.7f);
    glColor3f(0.8f, 0.8f, 0.8f); glVertex2f(x + w * 0.3f, y + h * 0.7f);
    glEnd();
}

void drawCaveMenu() {
    // Background Gradient
    glBegin(GL_QUADS);
    glColor3f(0.05f, 0.05f, 0.1f); glVertex2f(-3.0f, 3.0f);
    glColor3f(0.05f, 0.05f, 0.1f); glVertex2f(3.0f, 3.0f);
    glColor3f(0.0f, 0.0f, 0.0f);   glVertex2f(3.0f, -3.0f);
    glColor3f(0.0f, 0.0f, 0.0f);   glVertex2f(-3.0f, -3.0f);
    glEnd();

    // Floating Dust
    srand(42);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 50; i++) {
        float px = (rand() % 200) / 100.0f - 1.0f;
        float py = (rand() % 200) / 100.0f - 1.0f;
        float brightness = (rand() % 100) / 100.0f;
        glColor4f(1.0f, 1.0f, 0.8f, brightness * 0.5f);
        glVertex2f(px, py);
    }
    glEnd();

    // Menu Text
    glColor3f(0.0f, 0.0f, 0.0f);
    drawText(-0.18f, 0.19f, "ECO MINER REFORGED", GLUT_BITMAP_TIMES_ROMAN_24);
    glColor3f(1.0f, 1.0f, 0.2f);
    drawText(-0.19f, 0.20f, "ECO MINER REFORGED", GLUT_BITMAP_TIMES_ROMAN_24);

    static float alpha = 1.0f;
    static bool fading = true;
    if (fading) alpha -= 0.015f; else alpha += 0.015f;
    if (alpha <= 0.2f) fading = false;
    if (alpha >= 1.0f) fading = true;

    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    drawText(-0.22f, 0.0f, "Press ENTER to Start");

    glColor3f(0.7f, 0.7f, 0.7f);
    drawText(-0.22f, -0.3f, "AD / Arrows to Move");
    drawText(-0.45f, -0.4f, "SPACE to Jump | HOLD LEFT CLICK to Mine");
}

// ====================== WORLD DRAWING ===================
void drawWorld() {
    int biome = (level <= 3) ? 0 : (level <= 6 ? 1 : (level <= 8 ? 2 : 3));

    // Fill viewport completely to avoid black bars
    float skyLeft = cameraX - 5.0f;
    float skyRight = cameraX + 5.0f;

    if (biome == 0) drawGradientRect(skyLeft, -2.0f, skyRight, 2.0f, 0.25f, 0.65f, 0.95f, 0.75f, 0.88f, 1.0f);
    else if (biome == 1) drawGradientRect(skyLeft, -2.0f, skyRight, 2.0f, 0.45f, 0.25f, 0.55f, 0.95f, 0.55f, 0.25f);
    else if (biome == 2) drawGradientRect(skyLeft, -2.0f, skyRight, 2.0f, 0.08f, 0.08f, 0.12f, 0.18f, 0.16f, 0.22f);
    else drawGradientRect(skyLeft, -2.0f, skyRight, 2.0f, 0.02f, 0.02f, 0.05f, 0.08f, 0.1f, 0.25f);

    // --- RESTORED SCENERY (Clouds, Mountains & Stalactites) ---
    srand(level * 100);
    for (int i = 0; i < 20; i++) {
        float x = (rand() % 2000) / 100.0f - 5.0f;
        float y = (rand() % 100) / 100.0f;

        if (biome == 0) drawCloud(x, 0.55f + y * 0.35f, 0.16f + (rand() % 12) / 100.0f);
        else if (biome == 1) drawMountain(x, groundY, 0.4f + y * 0.2f, 0.5f + y * 0.5f);
        else if (biome == 2) drawStalactite(x, 1.0f, 0.1f + y * 0.1f, 0.3f + y * 0.5f);
        else {
            glPointSize(2.0f);
            glBegin(GL_POINTS);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex2f(x, -0.5f + y * 1.5f);
            glEnd();
        }
    }

    // Ground Base
    float gR = 0.2f, gG = 0.55f, gB = 0.25f;
    if (biome == 1) { gR = 0.55f; gG = 0.32f; gB = 0.15f; }
    else if (biome == 2) { gR = 0.22f; gG = 0.20f; gB = 0.22f; }
    else if (biome == 3) { gR = 0.75f; gG = 0.85f; gB = 0.95f; }

    glColor3f(gR * 0.5f, gG * 0.5f, gB * 0.5f);
    glRectf(cameraX - 5.0f, groundY, cameraX + 5.0f, -2.0f);
    drawGradientRect(cameraX - 5.0f, groundY - 0.25f, cameraX + 5.0f, groundY, gR, gG, gB, gR * 0.65f, gG * 0.65f, gB * 0.65f);

    // Flowers
    if (biome != 2) {
        for (auto& f : flowers) {
            glColor3f(1.0f, 0.5f, 0.8f); drawCircle(f.x, f.y, 0.02f, 10);
            glColor3f(0.0f, 0.5f, 0.0f); glRectf(f.x - 0.005f, groundY, f.x + 0.005f, f.y);
        }
    }

    // Fences
    for (auto& f : fences) {
        drawFenceSegment(f.x, f.y, f.scale);
    }

    // Trees (planted firmly at groundY)
    if (biome != 2) {
        for (auto& t : trees) {
            drawTree(t.x, t.y, t.scale, t.type, t.greenTint);
        }
    }

    // Dynamic Cave Stalactites
    if (biome == 2) {
        drawStalactites();
    }

    // Recyclable Waste
    if (trashActive) {
        glColor3f(0.5f, 0.5f, 0.5f); drawCircle(trashX, trashY, 0.05f, 15);
        glColor3f(0.2f, 0.2f, 0.2f); glRectf(trashX - 0.01f, trashY + 0.04f, trashX + 0.01f, trashY + 0.07f);
    }

    // Hazard Rock
    glColor3f(0.4f, 0.3f, 0.3f);
    glBegin(GL_POLYGON);
    glVertex2f(rockX - 0.1f, rockY);
    glVertex2f(rockX - 0.05f, rockY + 0.15f);
    glVertex2f(rockX + 0.06f, rockY + 0.12f);
    glVertex2f(rockX + 0.11f, rockY);
    glEnd();

    glColor3f(0.2f, 0.1f, 0.1f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(rockX - 0.02f, rockY + 0.13f);
    glVertex2f(rockX + 0.02f, rockY + 0.02f);
    glEnd();
    glLineWidth(1.0f);

    // Mining Stones
    for (int i = 0; i < MAX_STONES; i++) {
        if (stones[i].active) {
            drawOrganicRock(stones[i].x, stones[i].y, 0.07f, stones[i].health, stones[i].containsGem, i);
        }
    }

    drawParticles();
    drawPlayer();
}

// ====================== HUD & DISPLAY ===================
void drawHUD() {
    glColor3f(1, 1, 1);
    drawText(-2.0f, 1.1f, "Score: " + std::to_string(score));
    drawText(-2.0f, 1.0f, "Level: " + std::to_string(level) + "/" + std::to_string(MAX_LEVEL));

    if (gameState == PLAYING) {
        drawText(1.5f, 1.1f, "AD / Arrows: Move", GLUT_BITMAP_HELVETICA_12);
        drawText(1.5f, 1.0f, "Hold Left Click: Mine | Space: Jump", GLUT_BITMAP_HELVETICA_12);
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (gameState == MENU) {
        drawCaveMenu();
    }
    else if (gameState == PLAYING || gameState == PAUSED) {
        glPushMatrix();
        glTranslatef(-cameraX, 0.0f, 0.0f);
        drawWorld();
        glPopMatrix();
        drawHUD();

        if (gameState == PAUSED) {
            glColor3f(1, 1, 0); drawText(-0.1f, 0.0f, "PAUSED");
        }
    }
    else if (gameState == GAMEOVER) {
        glColor3f(1, 0, 0); drawText(-0.2f, 0.1f, "GAME OVER", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(1, 1, 1); drawText(-0.25f, -0.1f, "Press ENTER to Retry");
    }
    else if (gameState == VICTORY) {
        glColor3f(0, 1, 0); drawText(-0.2f, 0.1f, "VICTORY!", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(1, 1, 1); drawText(-0.25f, -0.1f, "You Won!");
    }

    // Fade Transition Curtain
    if (fadeAlpha > 0.0f) {
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, 1, 0, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glColor4f(0.0f, 0.0f, 0.0f, fadeAlpha);
        glRectf(0, 0, 1, 1);

        if (fadeAlpha > 0.8f) {
            glColor3f(1.0f, 1.0f, 1.0f);
            drawText(0.4f, 0.55f, "Loading Next Level...", GLUT_BITMAP_TIMES_ROMAN_24);
        }

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    }

    glutSwapBuffers();
}

bool checkCollision(float px, float py, float ox, float oy, float width, float height) {
    return (px > ox - width && px < ox + width && py > oy - height && py < oy + height);
}

// ====================== GAME LOGIC =======================
void mineRock() {
    if (isMining) return;
    isMining = true;
    pickaxeAngle = 135.0f;
    pickaxeSwinging = true;

    for (int i = 0; i < MAX_STONES; i++) {
        if (stones[i].active && checkCollision(playerX + (facingRight ? 0.1f : -0.1f), playerY, stones[i].x, stones[i].y, 0.15f, 0.15f)) {
            stones[i].health--;
            createMiningEffect(stones[i].x, stones[i].y, 0.6f, 0.6f, 0.6f);
            playSFX("mining.wav");
            if (stones[i].health <= 0) {
                stones[i].active = false;
                score += stones[i].containsGem ? 50 : 10;
                if (stones[i].containsGem) createMiningEffect(stones[i].x, stones[i].y, 1.0f, 1.0f, 0.0f);
            }
            return;
        }
    }
}

void update(int value) {
    if (isFadingOut || isFadingIn) {
        if (isFadingOut) {
            fadeAlpha += 0.02f;
            if (fadeAlpha >= 1.0f) {
                isFadingOut = false;
                level++;
                if (level > MAX_LEVEL) {
                    gameState = VICTORY;
                    fadeAlpha = 0.0f;
                }
                else {
                    initLevel();
                    isFadingIn = true;
                }
            }
        }
        else {
            fadeAlpha -= 0.02f;
            if (fadeAlpha <= 0.0f) isFadingIn = false;
        }
        glutPostRedisplay();
        glutTimerFunc(16, update, 0);
        return;
    }

    if (gameState == PLAYING) {
        // Horizontal Movement
        if (specialKeysState[GLUT_KEY_LEFT] || keys['a'] || keys['A']) {
            playerVelX -= 0.002f;
            facingRight = false;
        }
        if (specialKeysState[GLUT_KEY_RIGHT] || keys['d'] || keys['D']) {
            playerVelX += 0.002f;
            facingRight = true;
        }

        // Jump
        if ((specialKeysState[GLUT_KEY_UP] || keys[' ']) && !isJumping) {
            playerVelY = JUMP_STRENGTH;
            isJumping = true;
            playSFX("jumping.wav");
        }

        playerVelX *= FRICTION;
        playerVelY += GRAVITY;
        playerX += playerVelX;
        playerY += playerVelY;

        if (playerY <= groundY) {
            playerY = groundY;
            playerVelY = 0;
            isJumping = false;
        }

        // Hazard Rock
        if (checkCollision(playerX, playerY, rockX, rockY, 0.1f, 0.14f)) {
            playSFX("gameover.wav");
            gameState = GAMEOVER;
        }

        // Recycling item
        if (trashActive && checkCollision(playerX, playerY, trashX, trashY, 0.08f, 0.1f)) {
            trashActive = false;
            score += 100;
            playSFX("trash.wav");
            createMiningEffect(trashX, trashY, 0.2f, 0.8f, 0.2f);
        }

        // Smooth Camera Follow
        float targetCamX = playerX + (facingRight ? 0.3f : -0.3f);
        cameraX += (targetCamX - cameraX) * 0.1f;

        // Mining arm animation
        if (isMining) {
            if (pickaxeSwinging) {
                pickaxeAngle -= 15.0f;
                if (pickaxeAngle < -45.0f) pickaxeSwinging = false;
            }
            else {
                pickaxeAngle += 15.0f;
                if (pickaxeAngle > 0.0f) isMining = false;
            }
        }

        if (isMouseHeld) {
            mineRock();
        }

        updateParticles();
        updateStalactites();

        if (playerX > 4.0f && !isFadingOut) isFadingOut = true;
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void mouse(int button, int state, int x, int y) {
    if (gameState == PLAYING && button == GLUT_LEFT_BUTTON) {
        isMouseHeld = (state == GLUT_DOWN);
    }
}

void keyDown(unsigned char key, int x, int y) {
    keys[key] = true;
    if (key == 27) exit(0);
    if (key == 13) {
        if (gameState == MENU || gameState == GAMEOVER || gameState == VICTORY) {
            gameState = PLAYING;
            score = 0;
            level = 1;
            initLevel();
            stopBackgroundMusic();
            playBackgroundMusic("background.wav");
        }
    }
    if ((key == 'm' || key == 'M') && gameState == PLAYING) mineRock();
    if (key == 'p' || key == 'P') gameState = (gameState == PLAYING) ? PAUSED : PLAYING;
}

void keyUp(unsigned char key, int x, int y) { keys[key] = false; }
void specialKeyDown(int key, int x, int y) { specialKeysState[key] = true; }
void specialKeyUp(int key, int x, int y) { specialKeysState[key] = false; }

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)w / (float)h;
    if (w >= h) gluOrtho2D(-1.2 * aspect, 1.2 * aspect, -1.2, 1.2);
    else gluOrtho2D(-1.2, 1.2, -1.2 / aspect, 1.2 / aspect);
    glMatrixMode(GL_MODELVIEW);
}

// ====================== MAIN FUNCTION =====================
int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Eco Miner Reforged");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc(0, update, 0);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialKeyDown);
    glutSpecialUpFunc(specialKeyUp);
    glutMouseFunc(mouse);

    playBackgroundMusic("background.wav");

    glutMainLoop();
    return 0;
}