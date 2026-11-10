# PDX 0.8.0 API reference

PDX is an external Windows/Direct3D 9 game framework module for PIPI. Its exports are defined in `lib/pdx.pmod`. The PIPI compiler and VM are not modified.

## Lifecycle

| Function | Arguments | Return | Meaning |
|---|---|---|---|
| `open` | `title: string, width: number, height: number` | boolean | Creates a Win32 window and Direct3D 9 device. Dimensions must be 64–8192. |
| `pollEvents` | none | boolean | Pumps the Windows message queue, updates keyboard/mouse state and frame timing. False means the window was closed. Call first in every loop iteration. |
| `beginFrame` | `r, g, b` | boolean | Clears the backbuffer and opens a render scene. Returns false while the device is temporarily unavailable. |
| `endFrame` | none | boolean | Ends the scene and presents the backbuffer. Optional FPS pacing happens here. |
| `setTargetFPS` | `fps` | boolean | Limits frame rate. `0` disables pacing; accepted values are clamped to 0–1000. |
| `deltaTime` | none | number | Seconds since the previous event poll, clamped to 0–0.25. For movement, additionally clamp large pause/resume deltas to 0.05. |
| `time` | none | number | Elapsed seconds since PDX opened. |
| `setTitle` | `title: string` | boolean | Changes the native window title. |
| `close` | none | null | Destroys the window and releases textures/device/audio. Safe to call after the loop exits. |

## 2D drawing

All shapes and sprites must be drawn between a successful `beginFrame` and `endFrame`. RGB values are clamped to 0–255. Positive width/height/radius are required.

| Function | Arguments | Meaning |
|---|---|---|
| `fillRect` | `x, y, width, height, r, g, b` | Filled rectangle. |
| `drawRect` | `x, y, width, height, r, g, b` | One-pixel outline rectangle. |
| `drawLine` | `x1, y1, x2, y2, r, g, b` | Line between two points. |
| `fillCircle` | `x, y, radius, r, g, b` | Filled circle (32-segment approximation). |
| `drawCircle` | `x, y, radius, r, g, b` | Circle outline. |
| `drawText` | `text, x, y, r, g, b, size` | Single-line GDI-rasterized text. Text is cached as a Direct3D texture; ANSI/ASCII, max 255 bytes; size 8–96. |
| `setCamera` | `x, y, zoom` | Sets world camera origin and zoom. Zoom must be greater than 0 and at most 16. |
| `resetCamera` | none | Restores origin `(0, 0)` and zoom `1`. Use before screen-space HUD. |

Camera transform is `(worldX - cameraX) * zoom` and `(worldY - cameraY) * zoom`. Shapes, sprite coordinates/sizes and text use this transform. For UI, call `resetCamera()` before drawing HUD text.

## Textures and sprite sheets

| Function | Arguments | Return / meaning |
|---|---|---|
| `loadTexture` | `path: string` | Returns an integer handle starting at 1. Loads a BMP readable from the process working directory. |
| `drawTexture` | `id, x, y, width, height` | Draws the whole texture, scaled to the given rectangle. |
| `drawTextureRegion` | `id, x, y, width, height, u, v, uvWidth, uvHeight` | Draws a normalized UV region. UV coordinates must remain in `[0,1]`. |
| `drawTilemap` | `id, map, cols, rows, tilePixels, x, y, scale` | Draws a row-major flat string as a tilemap. Each cell is `0`–`9` or `A`–`F` to select an atlas tile; `.` or space is empty. Tile size must divide atlas width/height; max 16,384 cells. Returns drawn-cell count. |
| `textureWidth` | `id` | Original pixel width. |
| `textureHeight` | `id` | Original pixel height. |
| `freeTexture` | `id` | Releases a texture handle. |

Limits: 64 simultaneously loaded textures, each up to 4096×4096. PDX reads Windows BMP through GDI and turns magenta RGB `(255, 0, 255)` into transparent pixels. PNG/JPEG are not supported in 0.8.0. Keep asset paths relative to the working directory; launching from the project root is the simplest path convention.

For a 4-sprite horizontal atlas, each sprite has UV width `0.25`: `(0.0,0.0,0.25,1.0)`, `(0.25,0.0,0.25,1.0)`, `(0.50,0.0,0.25,1.0)`, `(0.75,0.0,0.25,1.0)`.

## Input

| Function | Arguments | Meaning |
|---|---|---|
| `keyDown` | `name: string` | True while key is down. |
| `keyPressed` | `name: string` | True only on the transition from up to down since the previous `pollEvents`. |
| `keyReleased` | `name: string` | True only on the transition from down to up since the previous `pollEvents`. |
| `mouseX` / `mouseY` | none | Mouse coordinates relative to the client area. |
| `mouseDown` | `"LEFT"`, `"RIGHT"`, `"MIDDLE"` | True while mouse button is held. |
| `mousePressed` | same names | True on button-down transition since previous poll. |

Key names: letters A–Z, digits 0–9, arrows, `SPACE`, `ESCAPE` / `ESC`, `ENTER` / `RETURN`, `SHIFT`, `CTRL` / `CONTROL`, `ALT`, `TAB`, `BACKSPACE`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `INSERT`, and `F1`–`F12`.

## Audio and utilities

| Function | Arguments | Meaning |
|---|---|---|
| `playSound` | `path: string, loop: boolean` | Starts an asynchronous WAV file using WinMM. Returns whether Windows accepted the request. |
| `stopSound` | none | Stops the current WinMM sound. |
| `rectsOverlap` | `ax, ay, aw, ah, bx, by, bw, bh` | Axis-aligned rectangle overlap test. |
| `clamp` | `value, min, max` | Clamps the value; swaps inverted bounds. |
| `randomInt` | `min, max` | Returns a pseudo-random integer in the inclusive range. |
| `distance` | `x1, y1, x2, y2` | Euclidean distance. |

## Loop template

```ts
import { open as pdxOpen, pollEvents, beginFrame, endFrame,
  fillRect, drawText, keyDown, deltaTime, clamp,
  close as pdxClose } from "pdx";

let x = 120;
if (pdxOpen("My PIPI Game", 800, 600)) {
  while (pollEvents()) {
    let dt = clamp(deltaTime(), 0.001, 0.05);
    if (keyDown("LEFT")) { x = x - 220 * dt; }
    if (keyDown("RIGHT")) { x = x + 220 * dt; }
    if (beginFrame(16, 20, 34)) {
      fillRect(x, 240, 32, 32, 95, 220, 255);
      resetCamera();
      drawText("PIPI + PDX", 18, 16, 230, 240, 255, 18);
      endFrame();
    }
  }
  pdxClose();
}
```

Tilemap example: a map of `12` columns and `5` rows uses 60 characters in row-major order. The first 12 characters are row 0, the next 12 are row 1, and so on. A tile ID must exist within the loaded atlas grid.

More examples:

- `frameworks/PDX/examples/pdx_demo.pipi` — Neon Arena, movement, frame delta, collision, score and drawing.
- `frameworks/PDX/examples/texture_demo.pipi` — BMP texture loading, string-grid tilemap, sprite atlas UVs, texture dimensions and keyboard movement.

## v0.8.0 limitations

PDX uses Win32 + Direct3D 9 and therefore runs on Windows only in this release. Audio is WAV-only and uses the simple Windows multimedia playback API, not a spatial mixer. Text is single-line and uses an ANSI/GDI path. There is no bundled physics solver, entity/component system, scene graph, animation editor, tilemap editor or asset packager. The PIPI language subset also has no first-class arrays/objects, so large entity collections are better implemented in an external native module until the language gains richer data types.
