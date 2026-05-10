# Nyan Cat Theme — Design Spec

**Date:** 2026-05-10  
**Status:** Approved

## Summary

A compiled-in TyrianIM theme faithful to the Nyan Cat meme: dark navy space background, pixel-art twinkling stars, an animated rainbow trail waving right-to-left, and the Nyan Cat GIF playing in the centre of the chat pane. All assets are embedded in the DLL at build time — no runtime files required.

---

## Section 1 — Asset Pipeline

**Source:** `nyan-cat.gif` (project root) — 16 frames, 498×280px, 50ms per frame (20fps, 800ms full cycle).

**Build-time extraction (CMake custom command):**
1. `convert nyan-cat.gif -coalesce +adjoin src/nyan_frame_%02d.png` — splits GIF into 16 PNGs, coalesced to resolve GIF disposal/delta frames.
2. `xxd -i src/nyan_frame_NN.png` run for each frame — produces `src/nyancat_frames.h` containing 16 `unsigned char[]` arrays and their lengths.

The custom command declares the GIF as input and the header as output, so it only re-runs when the GIF changes.

**Runtime loading (`AddonLoad`):**
```
for i in 0..15:
    APIDefs->Textures_GetOrCreateFromMemory(
        "TYRIAN_NYAN_NN", nyan_frame_NN_data, nyan_frame_NN_len)
```
Resulting `Texture_t*` pointers stored in `static Texture_t* s_NyanFrames[16]`.

The original `nyancat.png` is superseded and no longer used.

---

## Section 2 — Cat Sprite Rendering

**Position:** Horizontally centred in the chat pane (left edge of sprite at `mn.x + (mx.x - mn.x) * 0.5`). Vertically at 48% from the top of the pane.

**Size:** Rendered at a height of `(mx.y - mn.y) * 0.38` (38% of pane height), width scaled to maintain the 498:280 aspect ratio (~1.78:1).

**UV crop:** Each GIF frame includes the rainbow stub on its left ~25% (approximately x=0 to x=124 of 498px). The sprite is drawn with UV `(0.25, 0.0)` to `(1.0, 1.0)` so only the cat body is rendered. Our animated trail connects at the sprite's left edge.

**Frame cycling:**
```cpp
int frame = (int)(ImGui::GetTime() / 0.05f) % 16;
Texture_t* tex = s_NyanFrames[frame];
if (tex && tex->Resource)
    dl->AddImage((ImTextureID)tex->Resource, cat_min, cat_max,
                 ImVec2(0.25f, 0.0f), ImVec2(1.0f, 1.0f));
```

**Animation source:** Bob, paw cycle, head jiggle, and tail wag all come from the GIF frames — no separate transforms are applied to the sprite.

---

## Section 3 — Rainbow Trail

Drawn in `draw_chat_bg` from the left edge of the pane to the sprite's left edge (i.e. `mn.x` to `cat_left_x`).

**Bands:** 6 horizontal bands, 8px each (48px total):

| Band | Colour |
|------|--------|
| Red    | `IM_COL32(255, 30,   0,  α)` |
| Orange | `IM_COL32(255, 153,  0,  α)` |
| Yellow | `IM_COL32(255, 240,  0,  α)` |
| Green  | `IM_COL32( 51, 210,  0,  α)` |
| Blue   | `IM_COL32( 51, 102, 255, α)` |
| Purple | `IM_COL32(160,   0, 255, α)` |

**Vertical centre:** `cat_center_y = sprite_top + sprite_height * 0.45` (approximate centre of the rainbow within the sprite).

**Wave distortion:** Drawn as 3px-wide vertical strips. For strip at x:
```
y_offset = 6.0f * sinf(2π * x / 180.0f + 2π * t / 0.8f)
band_top  = cat_center_y - 24.0f + y_offset + band_index * 8.0f
```
Wave travels right-to-left (phase increases with x), period matches the 800ms GIF cycle.

**Alpha fade:** `α = 0.30 + 0.65 * (x - mn.x) / (cat_left_x - mn.x)` — fades from 0.30 at the left edge to 0.95 at the cat.

**Alignment note:** Because the cat's vertical bob is baked into the GIF frames (not a separate transform), the rainbow Y connection is driven by a sine approximation. Post-build tuning of `y_offset` amplitude and phase is expected.

---

## Section 4 — Stars

Drawn in both `draw_chat_bg` (20 stars) and `draw_contacts_bg` (8 stars).

**Pool:** Fixed-size array of star descriptors, each with a seeded position (`phi * width`, `psi * height` using golden-ratio spacing) and a staggered phase offset so they're never all in the same stage.

**Cycle duration:** 2.5 seconds per star. Stage boundaries within the cycle:

| Stage | Time window | Appearance |
|-------|-------------|------------|
| Dot         | 0–15%  | Single 2×2px white square |
| Small cross | 15–35% | 3×3px centre + 1px arms (7px span) |
| Large cross | 35–60% | 3×3px centre + 4px arms (11px span) |
| Ring        | 60–80% | 8 single-pixel dots at cardinal + diagonal positions on a 4px radius |
| Gone        | 80–100%| Invisible; position reseeded at end of cycle |

All drawn with `AddRectFilled` (1×1 or 2×2px squares). Colour: `IM_COL32(255, 255, 255, 220)`.

**Position reseeding:** At the end of each cycle the star's seed index is incremented, producing a new deterministic position via the golden-ratio sequence — no `rand()`, no heap allocation.

---

## Section 5 — Colour Palette & ImGui Style

**Background:** `#1a3060` — navy matching the meme.

| Element | Value |
|---------|-------|
| `WindowBg` | `ImVec4(0.10, 0.19, 0.38, 0.97)` |
| `ChildBg` | `ImVec4(0.08, 0.15, 0.30, 0.85)` |
| `bubble_self` (gradient) | `IM_COL32(210, 80, 180, 215)` → `IM_COL32(130, 35, 175, 225)` |
| `bubble_other` (gradient) | `IM_COL32(28, 52, 120, 200)` → `IM_COL32(14, 28, 70, 215)` |
| `sender_self` | `ImVec4(1.0, 0.55, 0.85, 1.0)` — pink |
| `sender_other` | `ImVec4(0.45, 0.80, 1.0, 1.0)` — sky blue |
| `timestamp` | `ImVec4(0.50, 0.60, 0.80, 0.65)` |
| `active_bg` | `IM_COL32(190, 55, 170, 180)` |
| `header_bg` | `IM_COL32(10, 18, 48, 245)` |
| `input_bg` | `IM_COL32(12, 22, 58, 220)` |
| `input_border` | `IM_COL32(190, 70, 210, 160)` |
| `avatar_bg` | `IM_COL32(170, 50, 200, 255)` |
| `pin_accent` / `unread_dot` | `IM_COL32(255, 80, 80, 255)` |

**ImGui rounding:** 6px on all elements (window, frame, popup, scrollbar, grab, tab).

---

## Section 6 — Files Changed

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add custom command: GIF → PNGs → `src/nyancat_frames.h` |
| `src/nyancat_frames.h` | Generated — 16 byte arrays |
| `src/dllmain.cpp` | `static Texture_t* s_NyanFrames[16]`; `NyanCatDrawChatBg`; `NyanCatDrawContactsBg`; `BuildNyanCatTheme()`; register in theme list at `AddonLoad` |

No changes to `TyrianTheme` struct, `ChatManager`, `WhisperHook`, `GW2API`, or `ChatLinks`.

---

## Out of Scope

- Sound (nyan cat music)
- Animated notification icon (floating speech bubble remains default, themed pink)
- Per-contact rainbow colours
