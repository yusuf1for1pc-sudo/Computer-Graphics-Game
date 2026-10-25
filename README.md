# Geometry Dash Clone

A Geometry Dash-style rhythm runner built for our Computer Graphics mini project, written in C++ with OpenGL (fixed-function pipeline) and FreeGLUT. Every object on screen is drawn from OpenGL primitives and our own scan-conversion routines; there are no image assets.

**Play in the browser:** https://yusuf1for1pc-sudo.github.io/Computer-Graphics-Game/

**Play on Windows:** download [`release/GeometryDash-Windows.zip`](release/GeometryDash-Windows.zip), unzip it and run `GeometryDash.exe`.

## The game

The run lasts two minutes: 8 stages of 15 seconds. Each stage is faster than the last (400 to 680 px/s), shortens the gaps between obstacles, changes the colour theme and introduces something new.

| Stage | Theme | New in this stage |
|---|---|---|
| 1 | Neon Purple | Spikes and blocks |
| 2 | Deep Blue | Jump pads |
| 3 | Jungle | **Double jump** unlocks: 4-high walls and 8-spike pits |
| 4 | Ocean | Jump orbs |
| 5 | Lava | **Down slam** unlocks: low ceilings of hanging needles |
| 6 | Sunset | Falling needles |
| 7-8 | Candy, Midnight | Everything mixed |

Dying restarts the current stage. On the home page, keys 1-8 start practice at that stage.

## Controls

| Action | Keyboard | Mouse / touch |
|---|---|---|
| Jump (hold to keep jumping) | Space, Up | Click or tap |
| Double jump (stage 3+) | Jump again in the air | Click or tap again |
| Jump orb | Jump while touching the ring | Click or tap |
| Slam (stage 5+) | Down or S in the air | Swipe down |
| Pause | P | Pause button |
| Restart stage | R | |
| Home | H or Esc | Home button |

## Computer graphics concepts in the code

`main.cpp` is organised in the order the game was built:

1. **Window, viewport and projection:** `gluOrtho2D` with a letterboxed 16:9 viewport.
2. **Output primitives:** `GL_QUADS`, `GL_TRIANGLES`, `GL_LINES`, triangle fans, colour interpolation across spikes.
3. **Animation loop:** GLUT timer with a fixed 1/120 s physics step.
4. **Rotation about an arbitrary point:** the cube spins with `T(c) R(angle) T(-c)`; background panels rotate about their own centres; hanging needles use a reflection (`glScalef(1, -1)`).
5. **Camera as a translation and the level map:** the level is built from text patterns with a fixed random seed.
6. **Collision detection:** axis-aligned boxes for blocks and an exact box-against-triangle test (point-in-triangle and segment intersection with cross products) for spikes.
7. **Scan conversion and effects:** our own Bresenham line and midpoint circle algorithms, parallax scrolling, alpha-blended glow, particle systems, colour themes interpolated between stages.
8. **HUD and home page:** scaled stroke text, buttons, and window-to-world mapping for mouse clicks.

A simulation of the same physics was used to check that every obstacle pattern can be passed at every speed it appears at, and that the double jump and slam obstacles cannot be passed without those abilities.

## Building

With MSYS2 (UCRT64) on Windows:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-freeglut
g++ main.cpp -o GeometryDash.exe -lfreeglut -lopengl32 -lglu32 -static-libgcc -static-libstdc++
```

Copy `libfreeglut.dll` and `libwinpthread-1.dll` from `C:\msys64\ucrt64\bin` next to the executable. On Linux: `g++ main.cpp -o geometrydash -lglut -lGL -lGLU`.

`web/index.html` is a JavaScript and Canvas port of the same game, used for the browser version. It generates exactly the same level as the C++ code.

## Team

Mohammed Yusuf Sayed (24101B0001), Ritesh Zagade (24101B0008), Swayam Sonavne (24101B0019).
Project guide: Dr. Santosh Tamboli. Department of Information Technology, Vidyalankar Institute of Technology.

Inspired by Geometry Dash by RobTop Games. This is a student project for learning and is not affiliated with RobTop Games.
