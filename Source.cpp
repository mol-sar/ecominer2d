#define _CRT_SECURE_NO_WARNINGS
#include <GL/freeglut.h>
#include <cmath>
#include <string>
#include <vector>
#include <ctime>
#include <windows.h>
#include <mmsystem.h>
#include <iostream>

// Link libraries automatically (Visual Studio specific)
#pragma comment(lib, "winmm.lib")

// ====================== GAME CONFIGURATION ======================
const float GRAVITY = -0.0025f;
const float JUMP_STRENGTH = 0.055f;
const float MOVEMENT_SPEED = 0.015f;
const float FRICTION = 0.85f;
const float MAX_SPEED = 0.03f;
int menuIndex = 0;   // 0 = Start, 1 = Quit
float menuPulse = 0.0f;

// TRANSITION VARIABLES (New)
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

struct Flower { float x, y; int type; };
std::vector<Flower> flowers;

struct Fence {
    float x, y;
    float scale; // Random variation in size
};
std::vector<Fence> fences;

// ====================== CAMERA ==========================
float cameraX = 0.0f;

// ====================== GAME OBJECTS ====================
float playerX = -0.8f;
float playerY = -0.6f;
float playerVelX = 0.0f;
float playerVelY = 0.0f;
bool isJumping = false;
bool facingRight = true;
float groundY = -0.6f;

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

// ====================== AUDIO ENGINE ====================
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

void updateParticles() {
    for (int i = 0; i < particles.size(); i++) {
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
    float w = 0.02f * scale;  // Post width
    float h = 0.2f * scale;   // Post height
    float gap = 0.15f * scale; // Distance between posts

    // Post 1 (Left)
    drawGradientRect(x, y, x + w, y + h,
        0.55f, 0.35f, 0.2f,   // Top Color (Light Brown)
        0.35f, 0.20f, 0.1f);  // Bottom Color (Dark Brown)

    // Post 2 (Right)
    drawGradientRect(x + gap, y, x + gap + w, y + h,
        0.55f, 0.35f, 0.2f,
        0.35f, 0.20f, 0.1f);

    // Horizontal Rails
    glColor3f(0.45f, 0.25f, 0.1f); // Darker wood for rails
    // Top Rail
    glRectf(x, y + (h * 0.7f), x + gap + w, y + (h * 0.8f));
    // Bottom Rail
    glRectf(x, y + (h * 0.3f), x + gap + w, y + (h * 0.4f));

    // Optional: Add a small "cap" to the posts for detail
    glColor3f(0.6f, 0.4f, 0.25f);
    glBegin(GL_TRIANGLES);
    // Post 1 Cap
    glVertex2f(x, y + h); glVertex2f(x + w, y + h); glVertex2f(x + (w / 2), y + h + 0.02f);
    // Post 2 Cap
    glVertex2f(x + gap, y + h); glVertex2f(x + gap + w, y + h); glVertex2f(x + gap + (w / 2), y + h + 0.02f);
    glEnd();
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
    // Create 15 fence segments scattered across the level
    for (int i = 0; i < 15; i++) {
        float x = -2.0f + (rand() % 600) / 100.0f; // Random X position

        // Don't put fences too close to the start point (optional cleanup)
        if (x > -1.0f && x < 1.0f) continue;

        fences.push_back({ x, groundY, 0.8f + ((rand() % 4) / 10.0f) });
    }
}
void initLevel() {
    playerX = -0.8f; playerY = groundY;
    playerVelX = 0; playerVelY = 0;
    cameraX = -0.8f;

    initFlowers();
    initFences();

    for (int i = 0; i < MAX_STONES; i++) {
        stones[i].x = -0.5f + static_cast<float>(rand() % 400) / 100.0f;
        stones[i].y = -0.6f;
        stones[i].health = 3;
        stones[i].active = (rand() % 100 < 80);
        stones[i].containsGem = (rand() % 3 == 0);
    }

    rockX = 0.5f + (rand() % 300) / 100.0f;
    rockY = -0.6f;
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
    if (fabs(playerVelX) > 0.001f) walkAnim = sin(playerX * 30.0f);

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
    glColor3f(0.4f, 0.8f, 1.0f); drawCircle(0.015f, 0.09f, 0.012f, 10); // Lens
    glColor3f(0.4f, 0.8f, 1.0f); drawCircle(-0.01f, 0.09f, 0.012f, 10);
    glColor3f(1.0f, 1.0f, 1.0f); drawCircle(0.018f, 0.094f, 0.004f, 5); // Shine

    // Helmet
    glBegin(GL_POLYGON);
    for (int i = 0; i <= 20; i++) {
        float angle = 3.14159f * (float)i / 20.0f;
        float yPos = sin(angle);
        glColor3f(1.0f * yPos, 0.8f * yPos, 0.0f);
        glVertex2f(0.05f * cos(angle), 0.11f + 0.05f * yPos);
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
        float yFactor = sin(angle);

        if (isGold && health < 3) {
            float brightness = 0.6f + (yFactor * 0.2f);
            glColor3f(brightness, brightness * 0.8f, 0.2f);
        }
        else {
            float brightness = 0.5f + (yFactor * 0.2f);
            glColor3f(brightness, brightness, brightness);
        }
        glVertex2f(x + cos(angle) * jaggedRadius, y + sin(angle) * jaggedRadius);
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
    glColor3f(1.0f, 1.0f, 1.0f);
    drawCircle(x, y, size, 10);
    drawCircle(x + size, y + size * 0.5f, size * 0.8f, 10);
    drawCircle(x - size, y + size * 0.5f, size * 0.8f, 10);
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
void drawLightGlow(float x, float y, float radius, float r, float g, float b) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for "glow" look

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, 0.6f); // Center: Bright and semi-transparent
    glVertex2f(x, y);

    glColor4f(r, g, b, 0.0f); // Edge: Completely transparent
    for (int i = 0; i <= 30; i++) {
        float angle = 2.0f * 3.14159f * float(i) / 30.0f;
        glVertex2f(x + cosf(angle) * radius, y + sinf(angle) * radius);
    }
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blending
}
void drawCaveMenu() {
    // 1. Background Gradient (Deep Dark Cave)
    glBegin(GL_QUADS);
    glColor3f(0.05f, 0.05f, 0.1f); glVertex2f(-3.0f, 3.0f);  // Top Left (Dark Blue)
    glColor3f(0.05f, 0.05f, 0.1f); glVertex2f(3.0f, 3.0f);   // Top Right
    glColor3f(0.0f, 0.0f, 0.0f);   glVertex2f(3.0f, -3.0f);  // Bottom Right (Black)
    glColor3f(0.0f, 0.0f, 0.0f);   glVertex2f(-3.0f, -3.0f); // Bottom Left
    glEnd();
  
    // 5. Floating Dust Particles (Atmosphere)
    srand((unsigned int)time(NULL) / 100); // Slow moving random seed
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 50; i++) {
        float px = (rand() % 200) / 100.0f - 1.0f;
        float py = (rand() % 200) / 100.0f - 1.0f;
        float brightness = (rand() % 100) / 100.0f;
        glColor4f(1.0f, 1.0f, 0.8f, brightness * 0.5f); // Faint yellow dust
        glVertex2f(px, py);
    }
    glEnd();

    // 6. DRAW TEXT
    // Title Shadow
    glColor3f(0.0f, 0.0f, 0.0f);
    drawText(-0.18f, 0.19f, "Urm MINERRZZZZ", GLUT_BITMAP_TIMES_ROMAN_24);

    // Title Main
    glColor3f(1.0f, 1.0f, 0.2f); // Gold
    drawText(-0.19f, 0.2f, "MINERRRZZZ", GLUT_BITMAP_TIMES_ROMAN_24);

    // Subtitle
    glColor3f(1.0f, 1.0f, 1.0f);

    // Blinking effect for "Press Start"
    static float alpha = 1.0f;
    static bool fading = true;
    if (fading) alpha -= 0.01f; else alpha += 0.01f;
    if (alpha <= 0.2f) fading = false;
    if (alpha >= 1.0f) fading = true;

    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    drawText(-0.22f, 0.0f, "Press ENTER to Start");

    // Controls
    glColor3f(0.7f, 0.7f, 0.7f);
    drawText(-0.22f, -0.3f, "AD / Arrows to Move");
    drawText(-0.45f, -0.4f, "SPACE to Jump | HOLD LEFT CLICK to Mine");
}
// ====================== WORLD DRAWING ===================
void drawWorld() {
    int biome = 0;
    if (level >= 4 && level <= 6) biome = 1;
    else if (level >= 7 && level <= 8) biome = 2;
    else if (level >= 9) biome = 3;

    float skyLeft = cameraX - 2.0f;
    float skyRight = cameraX + 2.0f;

    if (biome == 0) drawGradientRect(skyLeft, -1.0f, skyRight, 1.0f, 0.0f, 0.7f, 1.0f, 0.7f, 0.9f, 1.0f);
    else if (biome == 1) drawGradientRect(skyLeft, -1.0f, skyRight, 1.0f, 0.4f, 0.2f, 0.5f, 1.0f, 0.6f, 0.2f);
    else if (biome == 2) drawGradientRect(skyLeft, -1.0f, skyRight, 1.0f, 0.1f, 0.1f, 0.1f, 0.2f, 0.15f, 0.1f);
    else drawGradientRect(skyLeft, -1.0f, skyRight, 1.0f, 0.0f, 0.0f, 0.1f, 0.05f, 0.05f, 0.3f);

    srand(level * 100);
    for (int i = 0; i < 20; i++) {
        float x = (rand() % 2000) / 100.0f - 5.0f;
        float y = (rand() % 100) / 100.0f;

        if (biome == 0) drawCloud(x, 0.5f + y * 0.4f, 0.05f + y * 0.05f);
        else if (biome == 1) drawMountain(x, groundY, 0.4f + y * 0.2f, 0.5f + y * 0.5f);
        else if (biome == 2) drawStalactite(x, 1.0f, 0.1f + y * 0.1f, 0.3f + y * 0.5f);
        else { glPointSize(2.0f); glBegin(GL_POINTS); glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(x, -0.5f + y * 1.5f); glEnd(); }
    }

    float gR, gG, gB;
    if (biome == 0) { gR = 0.2f; gG = 0.6f; gB = 0.2f; }
    else if (biome == 1) { gR = 0.5f; gG = 0.3f; gB = 0.1f; }
    else if (biome == 2) { gR = 0.2f; gG = 0.15f; gB = 0.1f; }
    else { gR = 0.8f; gG = 0.9f; gB = 1.0f; }

    glColor3f(gR * 0.5f, gG * 0.5f, gB * 0.5f);
    glRectf(cameraX - 2.0f, groundY, cameraX + 2.0f, -1.0f);
    drawGradientRect(cameraX - 2.0f, groundY - 0.2f, cameraX + 2.0f, groundY, gR, gG, gB, gR * 0.6f, gG * 0.6f, gB * 0.6f);

    if (biome != 2) {
        for (auto& f : flowers) {
            glColor3f(1.0f, 0.5f, 0.8f); drawCircle(f.x, f.y, 0.02f, 10);
            glColor3f(0.0f, 0.5f, 0.0f); glRectf(f.x - 0.005f, groundY, f.x + 0.005f, f.y);
        }
    }

    if (trashActive) {
        glColor3f(0.5f, 0.5f, 0.5f); drawCircle(trashX, trashY, 0.05f, 15);
        glColor3f(0.2f, 0.2f, 0.2f); glRectf(trashX - 0.01f, trashY + 0.04f, trashX + 0.01f, trashY + 0.07f);
    }
    for (auto& f : fences) {
        drawFenceSegment(f.x, f.y, f.scale);
    }
    // === CHANGE 2: Larger, more visible obstacle rock ===
    // Made color reddish-grey and significantly increased vertex offsets for size
    glColor3f(0.4f, 0.3f, 0.3f); // Reddish grey body
    glBegin(GL_POLYGON);
    // Wider base and much taller peaks
    glVertex2f(rockX - 0.1f, rockY);
    glVertex2f(rockX - 0.05f, rockY + 0.15f); // Peak 1
    glVertex2f(rockX + 0.06f, rockY + 0.12f); // Peak 2
    glVertex2f(rockX + 0.11f, rockY);
    glEnd();

    // Add a jagged crack to make it look dangerous
    glColor3f(0.2f, 0.1f, 0.1f); // Dark red crack
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(rockX - 0.02f, rockY + 0.13f);
    glVertex2f(rockX + 0.02f, rockY + 0.02f);
    glEnd();
    glLineWidth(1.0f);

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
    drawText(-0.95f, 0.9f, "Score: " + std::to_string(score));
    drawText(-0.95f, 0.8f, "Level: " + std::to_string(level) + "/" + std::to_string(MAX_LEVEL));

    if (gameState == PLAYING) {
        drawText(0.5f, 0.9f, "AD / Arrows: Move", GLUT_BITMAP_HELVETICA_12);
        // === CHANGED ===
        drawText(0.4f, 0.85f, " Hold Left Click: Mine | Space: Jump", GLUT_BITMAP_HELVETICA_12);
    }
}

// ====================== MAIN DISPLAY FUNCTION ===================
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (gameState == MENU) {
        // === CHANGED: Call the new fancy menu function ===
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
        glColor3f(1, 0, 0); drawText(-0.2f, 0.1f, "Tf lol u suck", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(1, 1, 1); drawText(-0.25f, -0.1f, "Press ENTER to Retry");
    }
    else if (gameState == VICTORY) {
        glColor3f(0, 1, 0); drawText(-0.2f, 0.1f, "VICTORY!", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(1, 1, 1); drawText(-0.3f, -0.1f, "You fucking won!");
    }

    // --- DRAW FADE OVERLAY ---
    if (fadeAlpha > 0.0f) {
        // Set up a temporary 0-to-1 coordinate system for the overlay
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, 1, 0, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        // 1. Draw the Black Curtain
        glColor4f(0.0f, 0.0f, 0.0f, fadeAlpha);
        glRectf(0, 0, 1, 1);

        // 2. Draw Loading Text (Only if screen is mostly black)
        if (fadeAlpha > 0.8f) {
            glColor3f(1.0f, 1.0f, 1.0f); // White text

            // Calculate a simple "pulse" for the text using sine wave
            float pulse = (sin(fadeAlpha * 15.0f) + 1.0f) * 0.5f;

           
            drawText(0.4f, 0.55f, "Loading Next Level...", GLUT_BITMAP_TIMES_ROMAN_24);
        }

        // Restore original coordinate system
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
    isMining = true; pickaxeAngle = 135.0f; pickaxeSwinging = true;

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
    // --- FADE TRANSITION LOGIC ---
    if (isFadingOut || isFadingIn) {
        if (isFadingOut) {
            fadeAlpha += 0.02f;
            if (fadeAlpha >= 1.0f) {
                isFadingOut = false; level++;
                if (level > MAX_LEVEL) { gameState = VICTORY; fadeAlpha = 0.0f; }
                else { initLevel(); isFadingIn = true; }
            }
        }
        else {
            fadeAlpha -= 0.02f;
            if (fadeAlpha <= 0.0f) isFadingIn = false;
        }
        glutPostRedisplay(); glutTimerFunc(16, update, 0); return;
    }

    if (gameState == PLAYING) {
        // === CHANGED: ADDED WASD SUPPORT ===
        // Move Left (Arrow Left OR 'a' OR 'A')
        if (specialKeysState[GLUT_KEY_LEFT] || keys['a'] || keys['A']) {
            playerVelX -= 0.002f;
            facingRight = false;
        }

        // Move Right (Arrow Right OR 'd' OR 'D')
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

        if (playerY <= groundY) { playerY = groundY; playerVelY = 0; isJumping = false; }

        if (checkCollision(playerX, playerY, rockX, rockY, 0.1f, 0.14f)) { playSFX("gameover.wav"); gameState = GAMEOVER; }

        if (trashActive && checkCollision(playerX, playerY, trashX, trashY, 0.08f, 0.1f)) {
            trashActive = false; score += 100; playSFX("trash.wav");
            createMiningEffect(trashX, trashY, 0.2f, 0.8f, 0.2f);
        }

        float targetCamX = playerX + (facingRight ? 0.3f : -0.3f);
        cameraX += (targetCamX - cameraX) * 0.1f;

        if (isMining) {
            if (pickaxeSwinging) { pickaxeAngle -= 15.0f; if (pickaxeAngle < -45.0f) pickaxeSwinging = false; }
            else { pickaxeAngle += 15.0f; if (pickaxeAngle > 0.0f) isMining = false; }
        }
        if (isMouseHeld) {
            mineRock();
        }
        updateParticles();

        if (playerX > 4.0f && !isFadingOut) isFadingOut = true;
    }
    glutPostRedisplay(); glutTimerFunc(16, update, 0);
}
void mouse(int button, int state, int x, int y) {
    if (gameState == PLAYING) {
        if (button == GLUT_LEFT_BUTTON) {
            if (state == GLUT_DOWN) isMouseHeld = true;
            else if (state == GLUT_UP) isMouseHeld = false;
        }
    }
}
void keyDown(unsigned char key, int x, int y) {
    keys[key] = true;
    if (key == 27) exit(0);
    if (key == 13) {
        if (gameState == MENU || gameState == GAMEOVER || gameState == VICTORY) {
            gameState = PLAYING; score = 0; level = 1; initLevel();
            stopBackgroundMusic(); playBackgroundMusic("background.wav");
        }
    }
    if (( key == 'm' || key == 'M') && gameState == PLAYING) mineRock();
    if (key == 'p' || key == 'P') gameState = (gameState == PLAYING) ? PAUSED : PLAYING;
}
void keyUp(unsigned char key, int x, int y) { keys[key] = false; }
void specialKeyDown(int key, int x, int y) { specialKeysState[key] = true; }
void specialKeyUp(int key, int x, int y) { specialKeysState[key] = false; }

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float aspect = (float)w / h;
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
    glutDisplayFunc(display); glutReshapeFunc(reshape); glutTimerFunc(0, update, 0);
    glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialKeyDown); glutSpecialUpFunc(specialKeyUp);
    glutMouseFunc(mouse);
    playBackgroundMusic("background.wav");
    glutMainLoop();
    return 0;
}