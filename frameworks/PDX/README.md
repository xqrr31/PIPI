# PDX 0.8.0 — 2D game framework for PIPI

PDX is a Windows game framework built as an external PIPI native module. It uses **Win32 + Direct3D 9** for its window and rendering backend; the PIPI compiler now performs static call resolution, while PIPIBC06 bytecode and the public native-module ABI remain compatible.

PDX is designed to be useful for small 2D games and prototypes: frame lifecycle, frame limiting, frame delta/time, keyboard/mouse input, rectangles/lines/circles, text, BMP sprites, sprite-sheet regions and string-grid tilemaps, camera transforms, WAV playback, collision helpers, and basic math.

## Build

From a Windows project-root CMD with MinGW-w64 `g++`, DirectX 9 headers/import library and `WinMM` system library support:

```bat
build.bat
```

The full root `build.bat` invokes the PDX build and writes `modules\pdx\native\windows-x86_64\pipi_pdx.dll`. The generic compiler source can still be built without the DirectX SDK, but the full Windows build requires it. If MinGW cannot locate `d3d9.h` or `libd3d9`, install/configure a compatible Windows SDK or DirectX 9 development environment.

Compile and run the game sample from the project root so its asset paths resolve:

```bat
bin\pipi-c.exe compile frameworks\PDX\examples\pdx_demo.pipi -o dist\PIPI_PDX_Demo.pbc
bin\pipi-vm.exe dist\PIPI_PDX_Demo.pbc
```

The module loader looks for `pipi_pdx.dll` below `modules/pdx/native/windows-x86_64/`. PDX is Windows-only in this release; the `.pbc` itself remains PIPIBC06, but it requires a platform-compatible PDX module to run.

## Public API

### Window and frame lifecycle

- `open(title, width, height)`
- `pollEvents()`
- `beginFrame(r, g, b)` / `endFrame()`
- `setTitle(title)` / `close()`
- `setTargetFPS(fps)` (`0` disables pacing)
- `deltaTime()` (seconds since previous event poll; clamped at 0.25 s) / `time()` (seconds since open)

### Drawing

- `fillRect(x, y, width, height, r, g, b)`
- `drawRect(x, y, width, height, r, g, b)`
- `drawLine(x1, y1, x2, y2, r, g, b)`
- `fillCircle(x, y, radius, r, g, b)` / `drawCircle(...)`
- `drawText(text, x, y, r, g, b, size)`
- `loadTexture(path)`, `drawTexture(id, x, y, width, height)`, `drawTextureRegion(id, x, y, width, height, u, v, uvWidth, uvHeight)`, `textureWidth(id)`, `textureHeight(id)`, `freeTexture(id)`
- `setCamera(x, y, zoom)` / `resetCamera()`

Primitive and texture coordinates are world coordinates transformed by the current camera. `drawText` uses the same transform; call `resetCamera()` before HUD drawing. Color channels are clamped to `0..255`. `drawText` uses a small GDI-backed texture cache and supports basic single-line ANSI/ASCII text (up to 255 bytes) in sizes 8–96.

### Input

- `keyDown(name)`, `keyPressed(name)`, `keyReleased(name)`
- Names: `A`–`Z`, `0`–`9`, `LEFT`, `RIGHT`, `UP`, `DOWN`, `SPACE`, `ESCAPE`, `ENTER`, `SHIFT`, `CTRL`, `ALT`, `TAB`, `BACKSPACE`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `INSERT`, `F1`–`F12`.
- `mouseX()`, `mouseY()`, `mouseDown("LEFT"|"RIGHT"|"MIDDLE")`, `mousePressed("LEFT"|"RIGHT"|"MIDDLE")`.

Call `pollEvents()` at the top of every frame. `keyPressed` and `mousePressed` are edge-triggered against the preceding poll.

### Assets, audio and helpers

- `loadTexture(path)` supports Windows BMP files. The file must be readable from the process working directory; max supported dimensions are 4096×4096; max simultaneously loaded textures is 64. **Magenta RGB (255, 0, 255) is treated as transparent.**
- `drawTextureRegion` takes normalized UV coordinates within `0..1`; useful for horizontal/vertical sprite sheets.
- `playSound(path, loop)` plays WAV files asynchronously using WinMM; `stopSound()` stops it.
- `rectsOverlap(ax, ay, aw, ah, bx, by, bw, bh)`, `clamp(value, min, max)`, `randomInt(min, max)` (inclusive), `distance(x1, y1, x2, y2)`.

## Game loop pattern

```ts
import { open as pdxOpen, pollEvents, beginFrame, endFrame,
  fillRect, drawText, keyDown, deltaTime, clamp,
  close as pdxClose } from "pdx";

let x = 120;
if (pdxOpen("my first game", 800, 600)) {
  while (pollEvents()) {
    let dt = clamp(deltaTime(), 0.001, 0.05);
    if (keyDown("LEFT")) { x = x - 220 * dt; }
    if (keyDown("RIGHT")) { x = x + 220 * dt; }
    if (beginFrame(16, 20, 34)) {
      fillRect(x, 240, 32, 32, 95, 220, 255);
      drawText("PIPI + PDX", 18, 16, 230, 240, 255, 18);
      endFrame();
    }
  }
  pdxClose();
}
```

Examples: `examples/pdx_demo.pipi` (Neon Arena game loop and collisions), `examples/texture_demo.pipi` (tilemap, sprites, atlas regions and input). Precompiled artifacts are in the root `dist/` directory.

## Known limits in 0.8.0

- Direct3D 9 / Win32 only; no Linux PDX backend.
- Sprite loading accepts BMP only; magenta is the transparency color key. PNG/JPEG decoding is not bundled.
- WAV playback uses the Windows WinMM API; it is not a positional/mixer audio engine.
- No built-in scene graph, ECS, physics solver, tilemap editor, animation system, UI editor or asset packer yet. The low-level `drawTilemap` helper draws flat row-major string grids from an atlas; it is not a map editor or a chunked streaming system. The primitives make these possible as later external PDX modules or PIPI helper libraries without changing the language core.
- The language has no arrays/objects yet, so large entity collections are awkward to express in PIPI code. Keep gameplay state in individual variables or build a dedicated native module for larger collections.
- Runtime visual testing must be done on Windows with the DirectX 9 runtime; this release was prepared in a Linux container and its Win32 DLL could not be executed here.

PDX native code runs inside the PIPI process with the same OS privileges. Only load native modules you trust.
