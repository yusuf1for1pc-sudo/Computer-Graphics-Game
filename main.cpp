// Geometry Dash Clone - Computer Graphics mini project
// C++ with OpenGL (fixed-function pipeline) and FreeGLUT.
//
// The file is organised in the same steps used to build the game:
//   STEP 1  window, viewport and orthographic projection
//   STEP 2  output primitives: drawing the cube, ground, blocks and spikes
//   STEP 3  animation loop with a fixed physics time step (jumping)
//   STEP 4  rotation about the cube's own centre (composite transformation)
//   STEP 5  the level map, built from obstacle patterns, and the scrolling camera
//   STEP 6  collision detection (boxes and box-vs-triangle)
//   STEP 7  scan conversion, parallax, blending, particles and scene themes
//   STEP 8  HUD, home page and buttons
//
// The run has 8 stages of 15 seconds (2 minutes in total). Every stage is faster,
// has new obstacles, shorter gaps and a new colour theme:
//   stage 1 spikes and blocks        stage 2 jump pads
//   stage 3 double jump: tall walls  stage 4 jump orbs
//   stage 5 slam: low needle ceilings stage 6 falling needles
//   stages 7-8 everything mixed
// Dying restarts the current stage.
//
// Controls: Space / Up / left click = jump (hold to keep jumping; press again in
//           the air to double jump), Down / S = slam, P = pause,
//           R = restart stage, H = home, Esc = home / quit.
//           On the home page, 1-8 starts practice at that stage.

#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const int   WORLD_W = 960, WORLD_H = 540;   // logical resolution
const float TILE = 40.0f;                   // one level cell, in pixels
const float GROUND_Y = 120.0f;              // height of the ground line
const float PLAYER_SIZE = 40.0f;
const float PLAYER_SCREEN_X = 220.0f;       // where the cube sits on screen
const float GRAVITY = 2800.0f;              // px per second^2
const float JUMP_V = 820.0f;                // px per second
const float DOUBLE_JUMP_V = 740.0f;
const float SLAM_V = -1500.0f;
const float PAD_V = 1150.0f;                // jump pad launch speed
const float ORB_V = 820.0f;                 // jump orb bounce speed
const float ORB_R = 34.0f;                  // reach of an orb, centre to cube centre
const float NEEDLE_START = 600.0f;          // falling needles start above the screen
const float NEEDLE_G = 2600.0f;             // and accelerate down at this rate
const float NEEDLE_LEAD = 0.62f;            // they start when the player is this many seconds away
const float SPIN = 310.0f;                  // degrees per second while in the air
const float STEP = 1.0f / 120.0f;           // fixed physics step
const float PLAYER_INSET = 5.0f;            // forgiving hitbox against spikes
const float SPIKE_INSET = 8.0f;

const int   STAGES = 8;
const float STAGE_SECONDS = 15.0f;
const int   DOUBLE_JUMP_STAGE = 2;          // 0-based: unlocks in stage 3
const int   SLAM_STAGE = 4;                 // 0-based: unlocks in stage 5
const int   LEVEL_ROWS = 6;                 // obstacles can be up to 6 tiles high

float stageSpeed(int k) { return 400.0f + 40.0f * k; }   // 400 ... 680 px/s

// ----------------------------------------------------------------------------
// STEP 7: scene themes, one per stage
// ----------------------------------------------------------------------------
struct Color { float r, g, b; };
struct Theme { const char* name; Color bg, panel, groundTop, groundBottom, grid; };

const Theme THEMES[STAGES] = {
    {"Neon Purple", {0.34f, 0.13f, 0.95f}, {0.29f, 0.09f, 0.84f}, {0.16f, 0.06f, 0.78f}, {0.08f, 0.02f, 0.43f}, {0.35f, 0.17f, 0.88f}},
    {"Deep Blue",   {0.10f, 0.38f, 0.95f}, {0.07f, 0.30f, 0.82f}, {0.05f, 0.22f, 0.72f}, {0.02f, 0.08f, 0.40f}, {0.20f, 0.50f, 0.98f}},
    {"Jungle",      {0.12f, 0.68f, 0.35f}, {0.08f, 0.56f, 0.28f}, {0.05f, 0.44f, 0.22f}, {0.02f, 0.20f, 0.10f}, {0.25f, 0.88f, 0.45f}},
    {"Ocean",       {0.05f, 0.62f, 0.72f}, {0.03f, 0.50f, 0.60f}, {0.02f, 0.38f, 0.50f}, {0.01f, 0.16f, 0.24f}, {0.20f, 0.85f, 0.92f}},
    {"Lava",        {0.86f, 0.14f, 0.20f}, {0.72f, 0.09f, 0.15f}, {0.55f, 0.05f, 0.10f}, {0.25f, 0.02f, 0.05f}, {1.00f, 0.40f, 0.30f}},
    {"Sunset",      {0.96f, 0.52f, 0.10f}, {0.86f, 0.40f, 0.06f}, {0.70f, 0.28f, 0.04f}, {0.35f, 0.12f, 0.02f}, {1.00f, 0.75f, 0.30f}},
    {"Candy",       {0.90f, 0.22f, 0.65f}, {0.78f, 0.15f, 0.55f}, {0.60f, 0.10f, 0.45f}, {0.28f, 0.04f, 0.22f}, {1.00f, 0.55f, 0.85f}},
    {"Midnight",    {0.12f, 0.10f, 0.20f}, {0.20f, 0.17f, 0.30f}, {0.16f, 0.13f, 0.26f}, {0.05f, 0.04f, 0.09f}, {0.95f, 0.78f, 0.25f}},
};

Color mix(Color a, Color b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
Theme mixTheme(const Theme& a, const Theme& b, float t) {
    return {b.name, mix(a.bg, b.bg, t), mix(a.panel, b.panel, t), mix(a.groundTop, b.groundTop, t),
            mix(a.groundBottom, b.groundBottom, t), mix(a.grid, b.grid, t)};
}
void setColor(Color c, float a = 1.0f) { glColor4f(c.r, c.g, c.b, a); }

// ----------------------------------------------------------------------------
// STEP 5: obstacle patterns. Each column is written bottom to top:
//   '#' block    '^' spike    'v' hanging needle (points down)
//   'p' jump pad 'o' jump orb '!' falling needle    '.' or missing = empty
// Each pattern appears from minStage to maxStage. A simulation of this exact
// physics checked that every pattern is passable at every stage it appears in,
// and that the ones marked "needs" cannot be passed without that ability.
// ----------------------------------------------------------------------------
struct Pattern { int minStage, maxStage; std::vector<std::string> cols; };

const std::vector<Pattern> PATTERNS = {
    {0, 2, {"^"}},  // spike
    {0, 3, {"^", "^"}},  // spike2
    {0, 2, {"#"}},  // block
    {0, 4, {"#", "", "", "^"}},  // blockSpike
    {1, 7, {"^", "^", "^"}},  // spike3
    {1, 4, {"#", "#", "#"}},  // platform
    {1, 5, {"#", "#", "##", "##"}},  // stairs
    {1, 7, {"p", "", "", "^..#", "^..#", "^..#", "^..#"}},  // padHigh
    {1, 6, {"p", "^", "^", "^", "^", "^", "^", "^"}},  // padLong
    {1, 6, {"#", "#", "#^", "#", "#"}},  // platSpike
    {2, 7, {"", "", "", "####", "####"}},  // wall4  needs double jump
    {2, 7, {"^", "^", "^", "^", "^", "^", "^", "^"}},  // pit8  needs double jump
    {2, 7, {"", "", "", "####", "####", "^", "^", "^"}},  // wallSpikes  needs double jump
    {2, 7, {"^", "^", "^.#", "^.#", "^", "^", "^", "^..#", "^..#", "^", "^", "^"}},  // stones
    {3, 7, {"^", "^", "^", "^..o", "^", "^", "^", "^", "^"}},  // orbPit
    {3, 7, {"^", "^", "^", "^..o", "^", "^", "^", "^", "^..o", "^", "^", "^", "^"}},  // orbChain
    {3, 4, {"", "..o", "", "#####", "#####"}},  // orbWall
    {4, 7, {"...v#", "...v#", "...v#", "^..v#", "...v#", "...v#", "...v#"}},  // lowCeil  needs slam
    {4, 7, {"^", "^", "^", "^", "", "", "^", "^", "^", "^"}},  // gapField
    {4, 7, {"...v#", "...v#", "^..v#", "...v#", "...v#", "...v#", "^..v#", "...v#", "...v#"}},  // lowCeil2  needs slam
    {5, 7, {"", "", "!", "", ""}},  // needle1
    {5, 7, {"!", "", "", "!", "", "", "!"}},  // needle3
    {5, 7, {"^", "^", "!", "^", "^"}},  // needlePit
    {5, 7, {"", "", "", "####", "", "", "", "", "####"}},  // doubleWall
    {6, 7, {"^", "^", "", "#", "#^", "#", "", "^", "^", "^"}},  // gauntlet
    {6, 7, {"p", "", "", "", ".v#", ".v#", ".v#", ".v#", ".v#"}},  // padCeil
};

// ----------------------------------------------------------------------------
// Game state
// ----------------------------------------------------------------------------
enum State { MENU, PLAYING, DEAD, WIN };

struct Player {
    float x, y;        // bottom-left corner in world coordinates
    float vy;          // vertical velocity
    float angle;       // rotation in degrees
    bool onGround;
    bool usedDouble;   // double jump already used in this jump
};

struct Particle {
    float x, y, vx, vy;
    float life, maxLife, size;
    Color c;
    bool gravity;
};

struct Box { float x, y, w, h; };
struct Vec2 { float x, y; };

std::vector<std::string> level(LEVEL_ROWS);   // level[h] = row at height h
int levelCols = 0;
int stageStartCol[STAGES + 1];

Player player;
std::vector<Particle> particles;
State state = MENU;
bool paused = false;
bool jumpHeld = false, jumpPressed = false, slamPressed = false;
int attempts = 1;
int currentStage = 0;
int bestStage = 0;
float deathTimer = 0.0f;
float bannerTimer = 0.0f;
std::string bannerTitle, bannerText;
float cameraX = 0.0f;
float accumulator = 0.0f;
float elapsed = 0.0f;          // total time, used for animation on the home page
int lastTime = 0;
int trailCounter = 0;

// window and viewport, used to size points and to map mouse clicks
int winW = WORLD_W, winH = WORLD_H;
int vpX = 0, vpY = 0, vpW = WORLD_W, vpH = WORLD_H;

float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }

char tileAt(int col, int h) {
    if (col < 0 || col >= levelCols || h < 0 || h >= LEVEL_ROWS) return '.';
    return level[h][col];
}
float tileX(int col) { return col * TILE; }
float tileY(int h) { return GROUND_Y + h * TILE; }

int stageAtX(float x) {
    for (int k = STAGES - 1; k >= 0; k--)
        if (x >= stageStartCol[k] * TILE) return k;
    return 0;
}
bool doubleJumpUnlocked() { return currentStage >= DOUBLE_JUMP_STAGE; }
bool slamUnlocked() { return currentStage >= SLAM_STAGE; }

// ============================================================================
// STEP 5: build the whole 2-minute level from patterns.
// A fixed seed means the level is the same every time, like a real level.
// ============================================================================
unsigned int rngState = 2026;
int rng(int n) { rngState = rngState * 1664525u + 1013904223u; return (int)((rngState >> 16) % n); }

void addColumn(const std::string& col) {
    for (int h = 0; h < LEVEL_ROWS; h++)
        level[h] += (h < (int)col.size() && col[h] != '.') ? col[h] : '.';
    levelCols++;
}

void buildLevel() {
    rngState = 2026;
    for (auto& r : level) r.clear();
    levelCols = 0;
    for (int k = 0; k < STAGES; k++) {
        stageStartCol[k] = levelCols;
        int stageCols = (int)roundf(STAGE_SECONDS * stageSpeed(k) / TILE);
        // landing room after a pattern: one full jump plus a margin
        int tail = (int)ceilf(stageSpeed(k) * 2 * JUMP_V / GRAVITY / TILE) + 1;
        int used = 0, last = -1;
        for (int i = 0; i < 12; i++) { addColumn(""); used++; }   // checkpoint run-up
        // every obstacle introduced in this stage is shown once, early on
        std::vector<int> fresh;
        for (int p = 0; p < (int)PATTERNS.size(); p++)
            if (PATTERNS[p].minStage == k) fresh.push_back(p);
        int count = 0;
        while (true) {
            // pick a pattern: new ones in this stage are much more likely,
            // and the same pattern never appears twice in a row
            std::vector<int> pool;
            for (int p = 0; p < (int)PATTERNS.size(); p++) {
                if (k < PATTERNS[p].minStage || k > PATTERNS[p].maxStage || p == last) continue;
                int weight = PATTERNS[p].minStage == k ? 5 : PATTERNS[p].minStage == k - 1 ? 2 : 1;
                for (int w = 0; w < weight; w++) pool.push_back(p);
            }
            last = pool[rng((int)pool.size())];
            if (count % 2 == 1 && !fresh.empty()) {          // every second slot
                int i = rng((int)fresh.size());
                if (fresh[i] != last || fresh.size() == 1) last = fresh[i];
            }
            fresh.erase(std::remove(fresh.begin(), fresh.end(), last), fresh.end());
            count++;
            const Pattern& pat = PATTERNS[last];
            // run-up before the pattern: shorter in later stages, with random
            // variation and an occasional longer breather so the rhythm changes
            int lead = std::max(4, 9 - k) + rng(4) + (rng(5) == 0 ? 6 : 0);
            int len = lead + (int)pat.cols.size() + tail;
            if (used + len > stageCols) break;
            for (int i = 0; i < lead; i++) addColumn("");
            for (const auto& c : pat.cols) addColumn(c);
            for (int i = 0; i < tail; i++) addColumn("");
            used += len;
        }
        while (used < stageCols) { addColumn(""); used++; }
    }
    stageStartCol[STAGES] = levelCols;
}

// ============================================================================
// STEP 7a: scan conversion. Our own line and circle algorithms, plotted as points.
// ============================================================================
void bresenhamLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    glBegin(GL_POINTS);
    while (true) {
        glVertex2i(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    glEnd();
}

void midpointCircle(int xc, int yc, int r) {
    int x = 0, y = r, p = 1 - r;
    glBegin(GL_POINTS);
    while (x <= y) {
        // one computed point gives eight points by symmetry
        int pts[8][2] = {{x, y}, {y, x}, {-x, y}, {-y, x}, {x, -y}, {y, -x}, {-x, -y}, {-y, -x}};
        for (auto& q : pts) glVertex2i(xc + q[0], yc + q[1]);
        x++;
        if (p < 0) p += 2 * x + 1;
        else { y--; p += 2 * (x - y) + 1; }
    }
    glEnd();
}

// Points are one real pixel; when the window is scaled up, make them bigger
// so lines drawn with GL_POINTS stay solid.
void setRasterPointSize() {
    float scale = vpW / (float)WORLD_W;
    glPointSize(scale < 1.0f ? 1.0f : ceilf(scale));
}

// ============================================================================
// STEP 2: output primitives
// ============================================================================
void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

void drawRectOutline(float x, float y, float w, float h) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

void drawDisc(float cx, float cy, float r) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 48; i++) {
        float a = i * 6.2832f / 48;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();
}

// The cube: nested filled squares (black, green, black, purple, black, cyan)
void drawCube(float x, float y, float s) {
    struct Layer { float inset, r, g, b; };
    const Layer layers[] = {
        {0.000f, 0.03f, 0.01f, 0.10f},
        {0.050f, 0.55f, 0.89f, 0.17f},
        {0.217f, 0.03f, 0.01f, 0.10f},
        {0.258f, 0.42f, 0.23f, 0.91f},
        {0.367f, 0.03f, 0.01f, 0.10f},
        {0.400f, 0.18f, 0.90f, 0.95f},
    };
    for (const Layer& l : layers) {
        float in = l.inset * s;
        glColor3f(l.r, l.g, l.b);
        drawRect(x + in, y + in, s - 2 * in, s - 2 * in);
    }
}

// A block tile: dark fill, grid in the theme colour, glowing white outline
void drawBlock(float x, float y, const Theme& th) {
    glColor4f(0.85f, 0.78f, 1.0f, 0.08f);          // glow: wide, faint outlines
    for (int i = 4; i >= 1; i--) {
        glLineWidth(i * 3.0f);
        drawRectOutline(x, y, TILE, TILE);
    }
    glBegin(GL_QUADS);                               // vertical gradient fill
    setColor(mix(th.groundBottom, {0, 0, 0}, 0.2f)); glVertex2f(x, y); glVertex2f(x + TILE, y);
    glColor3f(0.02f, 0.01f, 0.06f); glVertex2f(x + TILE, y + TILE); glVertex2f(x, y + TILE);
    glEnd();
    glLineWidth(1.5f);
    setColor(th.grid);
    glBegin(GL_LINES);
    for (int i = 1; i < 3; i++) {
        glVertex2f(x + i * TILE / 3, y + 2); glVertex2f(x + i * TILE / 3, y + TILE - 2);
        glVertex2f(x + 2, y + i * TILE / 3); glVertex2f(x + TILE - 2, y + i * TILE / 3);
    }
    glEnd();
    glLineWidth(2.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawRectOutline(x, y, TILE, TILE);
}

// A spike: triangle with a colour gradient (white tip, transparent base).
// It pulses by scaling about the centre of its base (translate-scale-translate).
void drawSpike(float x, float y) {
    float pulse = 1.0f + 0.06f * sinf(elapsed * 6.0f);
    float bx = x + TILE / 2, by = y;
    glPushMatrix();
    glTranslatef(bx, by, 0);
    glScalef(1.0f, pulse, 1.0f);
    glTranslatef(-bx, -by, 0);
    glBegin(GL_TRIANGLES);
    glColor4f(0.72f, 0.95f, 0.84f, 0.30f); glVertex2f(x, y);
    glColor4f(0.72f, 0.95f, 0.84f, 0.30f); glVertex2f(x + TILE, y);
    glColor4f(1.00f, 1.00f, 1.00f, 1.00f); glVertex2f(x + TILE / 2, y + TILE);
    glEnd();
    glLineWidth(1.5f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + TILE, y); glVertex2f(x + TILE / 2, y + TILE);
    glEnd();
    glPopMatrix();
}

// A hanging needle is a spike reflected about the horizontal line through the
// tile's centre: glScalef(1, -1) is a reflection transformation.
void drawHangingNeedle(float x, float y) {
    float cy = y + TILE / 2;
    glPushMatrix();
    glTranslatef(0, cy, 0);
    glScalef(1.0f, -1.0f, 1.0f);
    glTranslatef(0, -cy, 0);
    drawSpike(x, y);
    glPopMatrix();
}

// Falling needle: a long thin triangle pointing down, red so it reads as danger
void drawFallingNeedle(float x, float y) {
    glBegin(GL_TRIANGLES);
    glColor4f(1.0f, 0.35f, 0.35f, 0.5f); glVertex2f(x + 10, y + 46); glVertex2f(x + 30, y + 46);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);   glVertex2f(x + 20, y + 4);
    glEnd();
    glLineWidth(1.5f);
    glColor3f(1.0f, 0.9f, 0.9f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x + 10, y + 46); glVertex2f(x + 30, y + 46); glVertex2f(x + 20, y + 4);
    glEnd();
}

// Jump pad: a yellow half-disc that glows
void drawPad(float x, float y) {
    float cx = x + TILE / 2;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 0.95f, 0.5f, 1.0f); glVertex2f(cx, y);
    glColor4f(1.0f, 0.75f, 0.1f, 1.0f);
    for (int i = 0; i <= 20; i++) {
        float a = 3.1416f * i / 20;
        glVertex2f(cx + 17 * cosf(a), y + 9 * sinf(a));
    }
    glEnd();
    glColor4f(1.0f, 0.85f, 0.2f, 0.25f + 0.15f * sinf(elapsed * 8));
    drawRect(x + 6, y, TILE - 12, 26);
}

// Jump orb: a filled disc and a Midpoint-circle ring that pulses
void drawOrb(float x, float y) {
    float cx = x + TILE / 2, cy = y + TILE / 2;
    float pulse = 1.0f + 0.12f * sinf(elapsed * 7);
    glColor4f(1.0f, 0.85f, 0.15f, 0.9f);
    drawDisc(cx, cy, 11);
    setRasterPointSize();
    glColor4f(1.0f, 0.95f, 0.6f, 0.9f);
    midpointCircle((int)cx, (int)cy, (int)(17 * pulse));
    glColor4f(1.0f, 0.95f, 0.6f, 0.35f);
    midpointCircle((int)cx, (int)cy, (int)(24 * pulse));
}

void drawText(float x, float y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (char c : s) glutBitmapCharacter(font, c);
}

// Stroke (vector) text can be scaled and rotated like any other geometry.
void drawStrokeText(float x, float y, float scale, const std::string& s, bool center, float width = 3.0f) {
    if (center) x -= glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)s.c_str()) * scale / 2;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1);
    glLineWidth(width);
    for (char c : s) glutStrokeCharacter(GLUT_STROKE_ROMAN, c);
    glPopMatrix();
}

// ============================================================================
// STEP 7b: particles
// ============================================================================
void spawnTrail() {
    Particle p;
    p.x = player.x + frand(-2, 6); p.y = player.y + frand(2, 12);
    p.vx = frand(-30, 0); p.vy = frand(10, 50);
    p.life = p.maxLife = frand(0.25f, 0.45f);
    p.size = frand(3, 7);
    p.c = {0.85f, 0.82f, 0.97f};
    p.gravity = false;
    particles.push_back(p);
}

void spawnBurst(float cx, float cy, int n, float minSp, float maxSp, Color a, Color b, bool gravity) {
    for (int i = 0; i < n; i++) {
        float ang = frand(0, 6.2832f), sp = frand(minSp, maxSp);
        Particle p;
        p.x = cx; p.y = cy;
        p.vx = cosf(ang) * sp; p.vy = sinf(ang) * sp;
        p.life = p.maxLife = frand(0.4f, 0.9f);
        p.size = frand(4, 10);
        p.c = i % 2 ? a : b;
        p.gravity = gravity;
        particles.push_back(p);
    }
}

void updateParticles(float dt) {
    for (Particle& p : particles) {
        if (p.gravity) p.vy -= 900.0f * dt;
        p.x += p.vx * dt; p.y += p.vy * dt;
        p.life -= dt;
    }
    for (size_t i = 0; i < particles.size();) {
        if (particles[i].life <= 0) { particles[i] = particles.back(); particles.pop_back(); }
        else i++;
    }
}

void drawParticles() {
    for (const Particle& p : particles) {
        float t = p.life / p.maxLife;                // 1 when born, 0 when gone
        float s = p.size * (0.4f + 0.6f * t);        // shrink as it fades
        setColor(p.c, t);
        drawRect(p.x - s / 2, p.y - s / 2, s, s);
    }
}

// ============================================================================
// STEP 6: collision detection
// ============================================================================
bool boxOverlap(const Box& a, const Box& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

float cross(Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool pointInTriangle(Vec2 p, const Vec2 t[3]) {
    float d1 = cross(t[0], t[1], p), d2 = cross(t[1], t[2], p), d3 = cross(t[2], t[0], p);
    bool hasNeg = d1 < 0 || d2 < 0 || d3 < 0;
    bool hasPos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(hasNeg && hasPos);
}

bool segmentsCross(Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p4) {
    float d1 = cross(p3, p4, p1), d2 = cross(p3, p4, p2);
    float d3 = cross(p1, p2, p3), d4 = cross(p1, p2, p4);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

// Box and triangle overlap if a triangle vertex is inside the box,
// a box corner is inside the triangle, or any two edges cross.
bool boxHitsTriangle(const Box& b, const Vec2 t[3]) {
    for (int i = 0; i < 3; i++)
        if (t[i].x >= b.x && t[i].x <= b.x + b.w && t[i].y >= b.y && t[i].y <= b.y + b.h) return true;
    Vec2 c[4] = {{b.x, b.y}, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.h}, {b.x, b.y + b.h}};
    for (int i = 0; i < 4; i++)
        if (pointInTriangle(c[i], t)) return true;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 3; j++)
            if (segmentsCross(c[i], c[(i + 1) % 4], t[j], t[(j + 1) % 3])) return true;
    return false;
}

// ============================================================================
// STEP 3: game logic on a fixed time step
// ============================================================================
void showBanner(int stage) {
    char buf[64];
    snprintf(buf, sizeof buf, "STAGE %d", stage + 1);
    bannerTitle = buf;
    if (stage == 0) bannerText = "Jump over the spikes!";
    else if (stage == 1) bannerText = "NEW: JUMP PADS - yellow pads launch you high";
    else if (stage == DOUBLE_JUMP_STAGE) bannerText = "NEW: DOUBLE JUMP - press jump again in the air";
    else if (stage == 3) bannerText = "NEW: JUMP ORBS - press jump while touching a ring";
    else if (stage == SLAM_STAGE) bannerText = "NEW: SLAM - low ceilings: jump, then press DOWN";
    else if (stage == 5) bannerText = "NEW: FALLING NEEDLES - watch for the red !";
    else {
        snprintf(buf, sizeof buf, "%s  -  speed x%.1f", THEMES[stage].name, stageSpeed(stage) / stageSpeed(0));
        bannerText = buf;
    }
    bannerTimer = 2.5f;
}

void spawnAtStage(int stage) {
    currentStage = stage;
    player.x = (stageStartCol[stage] + 1) * TILE;
    player.y = GROUND_Y;
    player.vy = 0;
    player.angle = 0;
    player.onGround = true;
    player.usedDouble = false;
    jumpPressed = slamPressed = false;
    state = PLAYING;
    paused = false;
    showBanner(stage);
}

void startRun() {
    attempts = 1;
    particles.clear();
    spawnAtStage(0);
}

void goHome() {
    state = MENU;
    paused = false;
    jumpHeld = false;
    particles.clear();
}

void killPlayer() {
    spawnBurst(player.x + PLAYER_SIZE / 2, player.y + PLAYER_SIZE / 2, 40, 100, 500,
               {0.55f, 0.89f, 0.17f}, {0.18f, 0.90f, 0.95f}, true);
    state = DEAD;
    deathTimer = 1.0f;
}

// A falling needle's height depends only on how close the player is, so it needs
// no stored state and resets by itself when the player respawns.
float needleY(int col, int h) {
    float speed = stageSpeed(currentStage);
    float trigger = tileX(col) - NEEDLE_LEAD * speed;
    if (player.x < trigger) return NEEDLE_START;
    float t = (player.x - trigger) / speed;
    return fmaxf(tileY(h), NEEDLE_START - 0.5f * NEEDLE_G * t * t);
}

bool touchingOrb() {
    int c0 = (int)floorf(player.x / TILE) - 1;
    for (int c = c0; c < c0 + 4; c++)
        for (int h = 0; h < LEVEL_ROWS; h++) {
            if (tileAt(c, h) != 'o') continue;
            float dx = player.x + PLAYER_SIZE / 2 - (tileX(c) + TILE / 2);
            float dy = player.y + PLAYER_SIZE / 2 - (tileY(h) + TILE / 2);
            if (dx * dx + dy * dy <= ORB_R * ORB_R) return true;
        }
    return false;
}

void physicsStep() {
    float speed = stageSpeed(currentStage);
    float prevY = player.y;
    player.x += speed * STEP;

    if (jumpPressed) {                           // a new press: jump, orb or double jump
        if (player.onGround) {
            player.vy = JUMP_V; player.onGround = false;
        } else if (touchingOrb()) {
            player.vy = ORB_V;
            spawnBurst(player.x + PLAYER_SIZE / 2, player.y + PLAYER_SIZE / 2, 12, 80, 200,
                       {1.0f, 0.88f, 0.2f}, {1, 1, 1}, false);
        } else if (doubleJumpUnlocked() && !player.usedDouble) {
            player.vy = DOUBLE_JUMP_V; player.usedDouble = true;
            spawnBurst(player.x + PLAYER_SIZE / 2, player.y, 14, 80, 220,
                       {0.18f, 0.90f, 0.95f}, {1, 1, 1}, false);
        }
    }
    if (jumpHeld && player.onGround) {           // holding jump re-jumps on landing
        player.vy = JUMP_V; player.onGround = false;
    }
    if (slamPressed && slamUnlocked() && !player.onGround && player.vy > SLAM_V) {
        player.vy = SLAM_V;
        spawnBurst(player.x + PLAYER_SIZE / 2, player.y + PLAYER_SIZE, 10, 60, 160,
                   {1.0f, 0.85f, 0.3f}, {1, 1, 1}, false);
    }
    jumpPressed = slamPressed = false;

    player.vy -= GRAVITY * STEP;                 // semi-implicit Euler
    player.y += player.vy * STEP;
    player.onGround = false;

    if (player.y <= GROUND_Y) {
        player.y = GROUND_Y; player.vy = 0; player.onGround = true;
    }

    // only test the few columns around the player
    int c0 = (int)floorf(player.x / TILE) - 1;
    Box hit = {player.x + PLAYER_INSET, player.y + PLAYER_INSET,
               PLAYER_SIZE - 2 * PLAYER_INSET, PLAYER_SIZE - 2 * PLAYER_INSET};
    bool onPad = false;
    for (int c = c0; c < c0 + 4; c++) {
        for (int h = 0; h < LEVEL_ROWS; h++) {
            char t = tileAt(c, h);
            float bx = tileX(c), by = tileY(h);
            if (t == 'v') {                                         // hanging needle, tip down
                Vec2 tri[3] = {{bx + SPIKE_INSET, by + TILE}, {bx + TILE - SPIKE_INSET, by + TILE},
                               {bx + TILE / 2, by + SPIKE_INSET * 1.5f}};
                if (boxHitsTriangle(hit, tri)) { killPlayer(); return; }
            } else if (t == '!') {                                  // falling needle
                float ny = needleY(c, h);
                Vec2 tri[3] = {{bx + 12, ny + 40}, {bx + 28, ny + 40}, {bx + 20, ny + 8}};
                if (boxHitsTriangle(hit, tri)) { killPlayer(); return; }
            } else if (t == 'p') {                                  // jump pad
                Box pad = {bx + 4, by, TILE - 8, 10};
                if (boxOverlap({player.x, player.y, PLAYER_SIZE, PLAYER_SIZE}, pad)) onPad = true;
            } else if (t == '#') {
                Box pb = {player.x, player.y, PLAYER_SIZE, PLAYER_SIZE};
                Box bb = {tileX(c), tileY(h), TILE, TILE};
                if (!boxOverlap(pb, bb)) continue;
                if (prevY >= bb.y + TILE - 1 && player.vy <= 0) {   // came from above: land
                    player.y = bb.y + TILE; player.vy = 0; player.onGround = true;
                } else {                                            // side or underside
                    killPlayer(); return;
                }
            } else if (t == '^') {
                Vec2 tri[3] = {{bx + SPIKE_INSET, by}, {bx + TILE - SPIKE_INSET, by},
                               {bx + TILE / 2, by + TILE - SPIKE_INSET * 1.5f}};
                if (boxHitsTriangle(hit, tri)) { killPlayer(); return; }
            }
        }
    }
    if (onPad) {
        player.vy = PAD_V; player.onGround = false;
        spawnBurst(player.x + PLAYER_SIZE / 2, player.y, 10, 60, 180, {1.0f, 0.88f, 0.2f}, {1, 0.6f, 0.9f}, false);
    }
    if (player.onGround) player.usedDouble = false;

    // STEP 4: spin in the air, snap flat on landing
    if (player.onGround) player.angle = roundf(player.angle / 90.0f) * 90.0f;
    else player.angle -= SPIN * STEP;

    if (player.onGround && ++trailCounter % 3 == 0) spawnTrail();

    // entering a new stage: faster, new theme, maybe a new ability
    int s = stageAtX(player.x);
    if (s != currentStage) {
        currentStage = s;
        if (currentStage > bestStage) bestStage = currentStage;
        showBanner(currentStage);
    }
    if (player.x >= levelCols * TILE) {
        state = WIN;
        bestStage = STAGES;
        spawnBurst(PLAYER_SCREEN_X + cameraX, 300, 80, 150, 600, {1, 0.85f, 0.2f}, {0.55f, 0.89f, 0.17f}, true);
    }
}

void update(float dt) {
    elapsed += dt;
    if (bannerTimer > 0) bannerTimer -= dt;
    if (state == PLAYING && !paused) physicsStep();
    if (state == DEAD) {
        deathTimer -= dt;
        if (deathTimer <= 0) { attempts++; spawnAtStage(currentStage); }
    }
    if (!paused) updateParticles(dt);
}

void timer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastTime) / 1000.0f;
    lastTime = now;
    accumulator += fminf(dt, 0.05f);             // avoid a huge catch-up after a stall
    while (accumulator >= STEP) {
        update(STEP);
        accumulator -= STEP;
    }
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

// ============================================================================
// Drawing the scene
// ============================================================================

// The theme fades from the previous stage's colours over the first 1.5 s of a stage.
Theme currentTheme() {
    if (state == MENU) {                         // the home page cycles through themes
        float t = elapsed / 3.0f;
        int k = (int)t % STAGES;
        return mixTheme(THEMES[k], THEMES[(k + 1) % STAGES], fminf(1.0f, (t - floorf(t)) * 3.0f));
    }
    int k = currentStage;
    if (k == 0) return THEMES[0];
    float into = (player.x - stageStartCol[k] * TILE) / (stageSpeed(k) * 1.5f);
    return mixTheme(THEMES[k - 1], THEMES[k], fminf(1.0f, fmaxf(0.0f, into)));
}

// STEP 7c: parallax background. Panels move at a quarter of the camera speed.
// Even stages use rectangles; odd stages rotate each panel 45 degrees about its
// centre to form diamonds, so the scene visibly changes.
void drawBackground(const Theme& th, bool diamonds) {
    static const float panels[][4] = {
        {-10, 375, 80, 165}, {90, 490, 305, 50}, {90, 247, 225, 225}, {334, 337, 62, 65},
        {334, 255, 62, 62}, {414, 337, 237, 203}, {336, 157, 315, 165}, {669, 370, 165, 165},
        {857, 375, 115, 165}, {-10, 104, 77, 165}, {89, 75, 225, 75}, {669, 190, 97, 97},
        {797, 190, 100, 97}, {669, 75, 235, 82}, {921, 104, 50, 165},
    };
    float offset = -fmodf(cameraX * 0.25f, (float)WORLD_W);
    setColor(th.panel);
    for (int k = 0; k < 2; k++) {
        glPushMatrix();
        glTranslatef(offset + k * WORLD_W, 0, 0);
        for (const auto& p : panels) {
            if (!diamonds) { drawRect(p[0], p[1], p[2], p[3]); continue; }
            float cx = p[0] + p[2] / 2, cy = p[1] + p[3] / 2, s = fminf(p[2], p[3]) * 0.75f;
            glPushMatrix();
            glTranslatef(cx, cy, 0);             // rotate each panel about its own centre
            glRotatef(45.0f, 0, 0, 1);
            drawRect(-s / 2, -s / 2, s, s);
            glPopMatrix();
        }
        glPopMatrix();
    }
}

void drawGround(const Theme& th) {
    glBegin(GL_QUADS);                               // vertical gradient
    setColor(th.groundBottom); glVertex2f(0, 0); glVertex2f(WORLD_W, 0);
    setColor(th.groundTop); glVertex2f(WORLD_W, GROUND_Y); glVertex2f(0, GROUND_Y);
    glEnd();

    // floor seams scroll with the world, drawn with Bresenham
    setRasterPointSize();
    setColor(mix(th.groundBottom, {0, 0, 0}, 0.4f), 0.8f);
    float seam = 216.0f;
    float start = -fmodf(cameraX, seam);
    for (float x = start; x < WORLD_W; x += seam)
        bresenhamLine((int)x, 0, (int)x, (int)GROUND_Y - 8);

    // glowing edge: translucent bands under a bright Bresenham line
    for (int i = 6; i >= 1; i--) {
        glColor4f(0.95f, 0.92f, 1.0f, 0.05f);
        drawRect(0, GROUND_Y - i * 1.5f, WORLD_W, i * 3.0f);
    }
    glColor3f(0.95f, 0.93f, 1.0f);
    bresenhamLine(0, (int)GROUND_Y, WORLD_W, (int)GROUND_Y);
}

// STEP 5: draw only the columns that are on screen
void drawLevel(const Theme& th) {
    int first = (int)floorf(cameraX / TILE) - 1;
    int last = first + WORLD_W / (int)TILE + 3;
    for (int c = first; c <= last; c++)
        for (int h = 0; h < LEVEL_ROWS; h++) {
            char t = tileAt(c, h);
            float x = tileX(c), y = tileY(h);
            if (t == '#') drawBlock(x, y, th);
            else if (t == '^') drawSpike(x, y);
            else if (t == 'v') drawHangingNeedle(x, y);
            else if (t == 'p') drawPad(x, y);
            else if (t == 'o') drawOrb(x, y);
            else if (t == '!') {
                float ny = needleY(c, h);
                drawFallingNeedle(x, ny);
                // warning sign at the top of the screen just before it drops
                float trigger = x - NEEDLE_LEAD * stageSpeed(currentStage);
                if (ny >= NEEDLE_START && player.x > trigger - 300) {
                    glColor3f(1.0f, 0.25f, 0.25f);
                    drawStrokeText(x + TILE / 2, 420, 0.35f, "!", true, 5.0f);
                }
            }
        }
    // thin markers at each stage start (checkpoints) and the finish line
    for (int k = 1; k <= STAGES; k++) {
        float fx = stageStartCol[k] * TILE;
        if (fx < cameraX - 10 || fx > cameraX + WORLD_W + 10) continue;
        glLineWidth(k == STAGES ? 5.0f : 2.0f);
        glColor4f(1, 1, 1, k == STAGES ? 0.9f : 0.35f);
        glBegin(GL_LINES); glVertex2f(fx, GROUND_Y); glVertex2f(fx, WORLD_H); glEnd();
    }
}

// STEP 4: rotation about the cube's own centre, M = T(c) R(angle) T(-c)
void drawPlayerAt(float x, float y, float angle) {
    float cx = x + PLAYER_SIZE / 2.0f;
    float cy = y + PLAYER_SIZE / 2.0f;
    glPushMatrix();
    glTranslatef(cx, cy, 0.0f);             // T(cx, cy)
    glRotatef(angle, 0, 0, 1);              // R(angle)
    glTranslatef(-cx, -cy, 0.0f);           // T(-cx, -cy)
    drawCube(x, y, PLAYER_SIZE);
    glPopMatrix();
}

// ----------------------------------------------------------------------------
// STEP 8: buttons and HUD (screen coordinates, not affected by the camera)
// ----------------------------------------------------------------------------
const float HOME_X = 18, HOME_Y = 488, HOME_S = 36;      // home button square
const float PAUSE_X = 925, PAUSE_Y = 505, PAUSE_R = 20;  // pause button circle
const float PLAY_X = 480, PLAY_Y = 235, PLAY_R = 62;     // play button on the home page

void drawHomeButton() {
    glColor4f(0, 0, 0, 0.25f);
    drawRect(HOME_X, HOME_Y, HOME_S, HOME_S);
    glLineWidth(2.0f);
    glColor4f(0.95f, 0.92f, 1.0f, 0.8f);
    drawRectOutline(HOME_X, HOME_Y, HOME_S, HOME_S);
    // a house: a triangle roof on a square base
    float cx = HOME_X + HOME_S / 2;
    glBegin(GL_TRIANGLES);
    glVertex2f(cx - 12, HOME_Y + 19); glVertex2f(cx + 12, HOME_Y + 19); glVertex2f(cx, HOME_Y + 30);
    glEnd();
    drawRect(cx - 8, HOME_Y + 7, 16, 12);
    glColor4f(0.1f, 0.05f, 0.25f, 0.9f);
    drawRect(cx - 3, HOME_Y + 7, 6, 7);                  // door
}

void drawPauseButton() {
    setRasterPointSize();
    glColor4f(0.95f, 0.92f, 1.0f, 0.75f);
    midpointCircle((int)PAUSE_X, (int)PAUSE_Y, (int)PAUSE_R);
    drawRect(PAUSE_X - 8, PAUSE_Y - 9, 5, 18);
    drawRect(PAUSE_X + 3, PAUSE_Y - 9, 5, 18);
}

void drawAbility(float x, float y, const char* name, bool unlocked, const char* hint) {
    glColor4f(0, 0, 0, 0.25f);
    drawRect(x, y, 150, 22);
    if (unlocked) glColor3f(0.55f, 0.89f, 0.17f); else glColor4f(1, 1, 1, 0.45f);
    std::string s = unlocked ? std::string(name) : std::string(name) + " " + hint;
    drawText(x + 6, y + 6, s, GLUT_BITMAP_HELVETICA_12);
}

void drawHUD() {
    char buf[96];
    float startX = stageStartCol[0] * TILE, endX = levelCols * TILE;
    float progress = fminf(1.0f, fmaxf(0.0f, (player.x - startX) / (endX - startX)));
    float bx = 330, by = 508, bw = 300, bh = 10;
    glColor4f(0, 0, 0, 0.35f);  drawRect(bx, by, bw, bh);
    glColor3f(0.55f, 0.89f, 0.17f); drawRect(bx + 1.5f, by + 1.5f, (bw - 3) * progress, bh - 3);
    glLineWidth(1.5f);
    glColor4f(1, 1, 1, 0.8f); drawRectOutline(bx, by, bw, bh);
    // stage ticks on the progress bar
    for (int k = 1; k < STAGES; k++) {
        float tx = bx + bw * (stageStartCol[k] - stageStartCol[0]) / (float)(levelCols - stageStartCol[0]);
        glBegin(GL_LINES); glVertex2f(tx, by); glVertex2f(tx, by + bh); glEnd();
    }
    snprintf(buf, sizeof buf, "%d%%", (int)(progress * 100));
    glColor3f(1, 1, 1); drawText(bx + bw + 12, by - 2, buf);

    // stage and time (each stage lasts 15 s at its own speed)
    float inStage = (player.x - stageStartCol[currentStage] * TILE) / stageSpeed(currentStage);
    int secs = (int)(currentStage * STAGE_SECONDS + fminf(inStage, STAGE_SECONDS));
    snprintf(buf, sizeof buf, "STAGE %d/%d   %d:%02d / 2:00", currentStage + 1, STAGES, secs / 60, secs % 60);
    drawText(bx, by - 26, buf, GLUT_BITMAP_HELVETICA_12);

    drawAbility(64, 500, "DOUBLE JUMP", doubleJumpUnlocked(), "(stage 3)");
    drawAbility(64, 472, "DOWN SLAM", slamUnlocked(), "(stage 5)");
    drawHomeButton();
    drawPauseButton();

    // stage banner: vector text that fades out
    if (bannerTimer > 0 && state != WIN) {
        float a = fminf(1.0f, bannerTimer / 0.6f);
        glColor4f(1, 1, 1, a);
        drawStrokeText(WORLD_W / 2.0f, 360, 0.45f, bannerTitle, true, 4.0f);
        glColor4f(1, 1, 0.75f, a);
        drawText(WORLD_W / 2.0f - bannerText.size() * 4.6f, 325, bannerText);
    }
    if (paused) {
        glColor4f(0, 0, 0, 0.45f); drawRect(0, 0, WORLD_W, WORLD_H);
        glColor3f(1, 1, 1); drawStrokeText(WORLD_W / 2.0f, 280, 0.4f, "PAUSED", true);
        drawText(395, 245, "Press P to continue");
    }
    if (state == WIN) {
        glColor4f(0, 0, 0, 0.5f); drawRect(0, 0, WORLD_W, WORLD_H);
        glColor3f(1.0f, 0.85f, 0.2f); drawStrokeText(WORLD_W / 2.0f, 310, 0.42f, "YOU BEAT ALL 8 STAGES!", true, 4.0f);
        snprintf(buf, sizeof buf, "Attempts: %d", attempts);
        glColor3f(1, 1, 1); drawText(425, 265, buf);
        drawText(330, 235, "Click or press Space to go home");
    }
}

// The home page: animated title, a bouncing cube and a big play button
void drawMenu(const Theme& th) {
    float pulse = 1.0f + 0.04f * sinf(elapsed * 4.0f);

    // title: stroke font, scaled and gently tilted (rotation about its centre)
    glPushMatrix();
    glTranslatef(WORLD_W / 2.0f, 430, 0);
    glRotatef(2.0f * sinf(elapsed * 1.5f), 0, 0, 1);
    glTranslatef(-WORLD_W / 2.0f, -430, 0);
    glColor4f(0, 0, 0, 0.35f);
    drawStrokeText(WORLD_W / 2.0f + 4, 416, 0.52f, "GEOMETRY DASH", true, 7.0f);
    glColor3f(1, 1, 1);
    drawStrokeText(WORLD_W / 2.0f, 420, 0.52f, "GEOMETRY DASH", true, 6.0f);
    glPopMatrix();
    setColor(mix(th.grid, {1, 1, 1}, 0.5f));
    drawStrokeText(WORLD_W / 2.0f, 372, 0.2f, "CLONE  -  C++ / OpenGL", true, 2.5f);

    // play button: filled disc, Midpoint circle outline and a play triangle,
    // scaled about its centre to pulse
    glPushMatrix();
    glTranslatef(PLAY_X, PLAY_Y, 0);
    glScalef(pulse, pulse, 1);
    glTranslatef(-PLAY_X, -PLAY_Y, 0);
    glColor3f(0.03f, 0.01f, 0.10f); drawDisc(PLAY_X, PLAY_Y, PLAY_R + 6);
    glColor3f(0.55f, 0.89f, 0.17f); drawDisc(PLAY_X, PLAY_Y, PLAY_R);
    glColor3f(0.03f, 0.01f, 0.10f);
    glBegin(GL_TRIANGLES);
    glVertex2f(PLAY_X - 18, PLAY_Y - 28); glVertex2f(PLAY_X - 18, PLAY_Y + 28); glVertex2f(PLAY_X + 30, PLAY_Y);
    glEnd();
    glPopMatrix();
    setRasterPointSize();
    glColor4f(1, 1, 1, 0.8f);
    midpointCircle((int)PLAY_X, (int)PLAY_Y, (int)(PLAY_R * pulse + 14));

    glColor3f(1, 1, 1);
    drawText(WORLD_W / 2.0f - 118, 140, "Click PLAY or press SPACE to start");
    glColor4f(1, 1, 1, 0.8f);
    drawText(40, 70, "Jump: Space / Up / Click     Double jump: jump again in the air (stage 3+)", GLUT_BITMAP_HELVETICA_12);
    drawText(40, 50, "Slam: Down / S (stage 5+)     Pause: P     Restart stage: R     Home: H", GLUT_BITMAP_HELVETICA_12);
    drawText(40, 30, "Orbs: press jump while touching a ring     Practice: press 1-8 to start at that stage", GLUT_BITMAP_HELVETICA_12);
    char buf[64];
    if (bestStage >= STAGES) snprintf(buf, sizeof buf, "Best: all 8 stages cleared!");
    else snprintf(buf, sizeof buf, "Best: stage %d of 8", bestStage + 1);
    drawText(40, 90, buf, GLUT_BITMAP_HELVETICA_12);

    // a cube hopping beside the button, spinning about its centre
    float t = fmodf(elapsed, 0.8f) / 0.8f;
    float hop = 4 * 90 * t * (1 - t);
    drawPlayerAt(700, GROUND_Y + hop, -180.0f * t);
}

void display() {
    Theme th = currentTheme();
    glClearColor(th.bg.r, th.bg.g, th.bg.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if (state == MENU) cameraX = elapsed * 120.0f;                 // slow drift
    else if (state != DEAD) cameraX = player.x - PLAYER_SCREEN_X;

    int k = state == MENU ? (int)(elapsed / 3.0f) % STAGES : currentStage;
    drawBackground(th, k % 2 == 1);                // screen space, own parallax offset
    drawGround(th);

    if (state == MENU) {
        drawMenu(th);
        glutSwapBuffers();
        return;
    }

    glPushMatrix();
    glTranslatef(-cameraX, 0, 0);                  // STEP 5: the camera is a translation
    drawLevel(th);
    char buf[32];
    snprintf(buf, sizeof buf, "Attempt %d", attempts);
    glColor4f(1, 1, 1, 0.75f);
    drawText((stageStartCol[currentStage] + 3) * TILE, 430, buf, GLUT_BITMAP_TIMES_ROMAN_24);
    drawParticles();
    if (state != DEAD) drawPlayerAt(player.x, player.y, player.angle);
    glPopMatrix();

    drawHUD();
    glutSwapBuffers();
}

// ============================================================================
// STEP 1: window, viewport and projection
// ============================================================================
void reshape(int w, int h) {
    // letterbox to keep 16:9 so the scene is never stretched
    winW = w; winH = h;
    vpW = w; vpH = w * WORLD_H / WORLD_W;
    if (vpH > h) { vpH = h; vpW = h * WORLD_W / WORLD_H; }
    vpX = (w - vpW) / 2; vpY = (h - vpH) / 2;
    glViewport(vpX, vpY, vpW, vpH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WORLD_W, 0, WORLD_H);      // origin bottom-left, y up
    glMatrixMode(GL_MODELVIEW);
}

// Window pixel (y down) -> world coordinates (y up): the inverse of the viewport mapping
Vec2 toWorld(int mx, int my) {
    return {(mx - vpX) * (float)WORLD_W / vpW, (winH - my - vpY) * (float)WORLD_H / vpH};
}

// ----------------------------------------------------------------------------
// Input
// ----------------------------------------------------------------------------
void pressJump() {
    if (state == MENU) { startRun(); return; }
    if (state == WIN) { goHome(); return; }
    jumpHeld = true;
    jumpPressed = true;
}

void keyboard(unsigned char key, int, int) {
    if (key == ' ') pressJump();
    if (key == 13 && state == MENU) startRun();
    if (key >= '1' && key <= '8' && state == MENU) {    // practice a single stage
        startRun();
        spawnAtStage(key - '1');
    }
    if ((key == 's' || key == 'S') && state == PLAYING) slamPressed = true;
    if ((key == 'p' || key == 'P') && state == PLAYING) paused = !paused;
    if ((key == 'r' || key == 'R') && (state == PLAYING || state == DEAD)) { attempts++; spawnAtStage(currentStage); }
    if (key == 'h' || key == 'H') goHome();
    if (key == 27) { if (state == MENU) exit(0); else goHome(); }
}
void keyboardUp(unsigned char key, int, int) { if (key == ' ') jumpHeld = false; }
void special(int key, int, int) {
    if (key == GLUT_KEY_UP) pressJump();
    if (key == GLUT_KEY_DOWN && state == PLAYING) slamPressed = true;
}
void specialUp(int key, int, int) { if (key == GLUT_KEY_UP) jumpHeld = false; }

void mouse(int button, int st, int mx, int my) {
    if (button != GLUT_LEFT_BUTTON) return;
    if (st == GLUT_UP) { jumpHeld = false; return; }
    Vec2 p = toWorld(mx, my);
    if (state == MENU) {
        float dx = p.x - PLAY_X, dy = p.y - PLAY_Y;
        if (dx * dx + dy * dy <= (PLAY_R + 14) * (PLAY_R + 14)) startRun();
        return;
    }
    if (p.x >= HOME_X && p.x <= HOME_X + HOME_S && p.y >= HOME_Y && p.y <= HOME_Y + HOME_S) { goHome(); return; }
    float dx = p.x - PAUSE_X, dy = p.y - PAUSE_Y;
    if (dx * dx + dy * dy <= PAUSE_R * PAUSE_R * 1.5f) { if (state == PLAYING) paused = !paused; return; }
    pressJump();
}

int main(int argc, char** argv) {
    srand((unsigned)time(nullptr));
    buildLevel();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WORLD_W, WORLD_H);
    glutCreateWindow("Geometry Dash Clone - CG Mini Project");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(special);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouse);
    glutIgnoreKeyRepeat(1);

    player = {0, GROUND_Y, 0, 0, true, false};
    lastTime = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);
    glutMainLoop();
    return 0;
}
