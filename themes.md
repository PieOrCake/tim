# TyrianIM Theme Guide

Themes are `.toml` files placed in `addons/TyrianIM/themes/` inside your GW2 folder. They appear in the theme dropdown in the TyrianIM settings panel automatically — no restart required.

## Built-in themes

The following themes ship with TyrianIM and can be selected directly from the dropdown. They serve as good references for color direction when building a custom theme.

| Theme | Style |
|---|---|
| GW2 Dark | Default dark palette matching the GW2 UI |
| Nexus Passthrough | Inherits the host Nexus window style |
| Charr Steel | Industrial dark red |
| Asuran Lab | Electric cyan on near-black |
| Sylvari Grove | Green and gold with animated Bezier vines |
| Divinity's Reach | Warm stone and parchment |
| Hoelbrak | Nordic blue with aurora shimmer |
| Nyan Cat | Animated rainbow and star field |
| Commodore 64 | Classic C64 blue phosphor |
| Underwater | Deep ocean teal with rising bubbles |
| Candlelight | Warm amber with ember particles |
| Barbie | Hot pink with wandering hearts and sparkles |
| Fishtank | Deep navy with swimming fish, seaweed and a visiting crab |

> **Note:** Animated backgrounds (particle systems, draw hooks) are exclusive to compiled-in themes and cannot be replicated via TOML. TOML themes support a static PNG background via `bg_texture` (see below).

---

---

## Quick start

Create `addons/TyrianIM/themes/mytheme.toml`:

```toml
name        = "My Theme"
author      = "YourName.1234"
description = "A brief description"

[colors]
WindowBg = [0.08, 0.08, 0.10, 0.97]
Text     = [0.90, 0.90, 0.95, 1.00]

[chat]
bubble_self  = [40, 60, 120, 210]
bubble_other = [50, 50, 60,  210]
pin_accent   = [100, 180, 255, 255]
```

All sections are optional. Any field you omit inherits from the GW2 Dark built-in theme.

---

## File format

### Metadata (root level)

| Key           | Type   | Description                        |
|---------------|--------|------------------------------------|
| `name`        | string | Display name in the dropdown       |
| `author`      | string | Your account name or handle        |
| `description` | string | Short description shown in UI      |

---

### `[style]` — ImGui metrics

Controls rounding, padding, and border sizes. All values are floats.

| Key                 | Description                                 |
|---------------------|---------------------------------------------|
| `windowRounding`    | Window corner radius (px)                   |
| `childRounding`     | Child window corner radius                  |
| `frameRounding`     | Input/button corner radius                  |
| `popupRounding`     | Popup corner radius                         |
| `scrollbarRounding` | Scrollbar nub corner radius                 |
| `grabRounding`      | Slider grab corner radius                   |
| `tabRounding`       | Tab corner radius                           |
| `scrollbarSize`     | Scrollbar width (px)                        |
| `grabMinSize`       | Minimum slider grab length (px)             |
| `windowBorderSize`  | Window border thickness (0 = none)          |
| `frameBorderSize`   | Input/button border thickness (0 = none)    |
| `windowPadding`     | `[x, y]` — padding inside windows          |
| `framePadding`      | `[x, y]` — padding inside inputs/buttons   |
| `itemSpacing`       | `[x, y]` — space between items             |
| `itemInnerSpacing`  | `[x, y]` — space inside compound widgets   |

```toml
[style]
windowRounding = 8.0
frameRounding  = 4.0
windowPadding  = [10.0, 8.0]
```

---

### `[colors]` — ImGui color slots

Colors are RGBA floats in the range 0–1:

```toml
[colors]
WindowBg      = [0.08, 0.06, 0.05, 0.97]
ChildBg       = [0.07, 0.05, 0.04, 0.85]
Text          = [0.92, 0.88, 0.80, 1.00]
TextDisabled  = [0.50, 0.44, 0.36, 1.00]
Border        = [0.40, 0.30, 0.12, 0.50]
```

Supported slot names:

`Text`, `TextDisabled`, `WindowBg`, `ChildBg`, `PopupBg`, `Border`, `BorderShadow`,
`FrameBg`, `FrameBgHovered`, `FrameBgActive`,
`TitleBg`, `TitleBgActive`, `TitleBgCollapsed`, `MenuBarBg`,
`ScrollbarBg`, `ScrollbarGrab`, `ScrollbarGrabHovered`, `ScrollbarGrabActive`,
`CheckMark`, `SliderGrab`, `SliderGrabActive`,
`Button`, `ButtonHovered`, `ButtonActive`,
`Header`, `HeaderHovered`, `HeaderActive`,
`Separator`, `SeparatorHovered`, `SeparatorActive`,
`ResizeGrip`, `ResizeGripHovered`, `ResizeGripActive`,
`Tab`, `TabHovered`, `TabActive`, `TabUnfocused`, `TabUnfocusedActive`,
`PlotHistogram`, `PlotHistogramHovered`,
`TableHeaderBg`, `TableBorderStrong`, `TableBorderLight`, `TableRowBg`, `TableRowBgAlt`,
`NavHighlight`, `ModalWindowDimBg`

> **ImThemes compatibility** — theme files exported from the [ImThemes](https://github.com/Patitotective/ImThemes) editor work as-is. TyrianIM also accepts the `"rgba(R, G, B, A)"` string format those files use, where R/G/B are 0–255 and A is 0–1.

---

### `[chat]` — TyrianIM-specific colors and settings

If this section is absent, TyrianIM derives reasonable chat colors from your `[colors]` slots automatically.

#### Message bubbles

Colors here are **RGBA integers 0–255** (unlike the `[colors]` section which uses floats).

| Key                | Description                                      |
|--------------------|--------------------------------------------------|
| `bubble_self`      | Your message bubble fill                         |
| `bubble_other`     | Their message bubble fill                        |
| `bubble_self_top`  | Gradient top color for your bubble               |
| `bubble_self_bot`  | Gradient bottom color for your bubble            |
| `bubble_other_top` | Gradient top color for their bubble              |
| `bubble_other_bot` | Gradient bottom color for their bubble           |
| `bubble_rounding`  | Bubble corner radius in px (float, default 10)   |

When `bubble_self_top` equals `bubble_self_bot` the bubble is flat with rounded corners. When they differ, a vertical gradient is drawn instead (rounding is ignored for gradients).

```toml
[chat]
# Flat royal-blue self bubble
bubble_self = [30, 55, 110, 215]

# Or a gradient (top ≠ bottom):
bubble_self_top = [45, 75, 140, 225]
bubble_self_bot = [15, 35, 80,  235]
```

#### Chrome colors

| Key            | Description                                         |
|----------------|-----------------------------------------------------|
| `header_bg`    | Contact list header row background                  |
| `active_bg`    | Selected/active contact highlight                   |
| `input_bg`     | Text entry field background                         |
| `input_border` | Text entry field border (0 alpha = no border)       |
| `avatar_bg`    | Base hue for avatar circles (hue shifts per contact)|
| `unread_dot`   | Unread indicator dot                                |
| `pin_accent`   | Pinned contact bar; also tints the notification flash|

#### Text colors

These are **RGBA floats 0–1**.

| Key            | Description                              |
|----------------|------------------------------------------|
| `sender_self`  | Your name in message headers             |
| `sender_other` | Their name in message headers            |
| `timestamp`    | Message timestamp text                   |
| `unread_label` | "Unread" label text                      |
| `status_ok`    | Hook status: connected                   |
| `status_warn`  | Hook status: warning                     |
| `status_err`   | Hook status: error                       |

#### Background overlay

A full-panel gradient drawn over the chat area before messages are rendered. Use low alpha values — this sits on top of `WindowBg`.

| Key           | Description                   |
|---------------|-------------------------------|
| `chat_bg_top` | Gradient color at top of chat |
| `chat_bg_bot` | Gradient color at bottom      |

```toml
[chat]
chat_bg_top = [0,  0,  0,  0]   # transparent
chat_bg_bot = [10, 5,  2, 35]   # faint warm tint at bottom
```

#### Animation

| Key              | Description                                          | Default  |
|------------------|------------------------------------------------------|----------|
| `bubble_rounding`| Bubble corner radius (px)                            | `10.0`   |
| `bob_amplitude`  | Floating icon vertical bob range (px)                | `3.0`    |
| `bob_period_ms`  | Bob cycle duration (ms)                              | `2000.0` |
| `flash_period_ms`| Notification icon flash cycle (ms)                   | `1000.0` |
| `fade_ms`        | New message fade-in duration (ms)                    | `150.0`  |

#### Texture files

Place PNG files in the same `themes/` directory as your `.toml`.

| Key              | Description                                                   |
|------------------|---------------------------------------------------------------|
| `bg_texture`     | PNG drawn behind chat messages (filename only, e.g. `bg.png`)|
| `icon_texture`   | PNG to replace the floating notification icon (64×64 recommended, transparent background) |

```toml
[chat]
bg_texture   = "krytan_stone.png"
icon_texture = "my_icon.png"
```

`bg_texture` renders as a static image. For animated backgrounds (particle systems, draw hooks), see the built-in themes — these are compiled-in and not available via TOML.

`icon_texture` replaces the floating speech-bubble icon that bobs in the corner and flashes on incoming whispers. 64×64 PNG with a transparent background works best. The flash tint is driven by `pin_accent`.

---

## Full example

```toml
name        = "Krytan Midnight"
author      = "Exemplar.1234"
description = "Deep blue with gold accents, human-inspired"

[style]
windowRounding = 6.0
frameRounding  = 4.0
frameBorderSize = 0.0

[colors]
WindowBg         = [0.06, 0.05, 0.10, 0.97]
ChildBg          = [0.05, 0.04, 0.09, 0.85]
Text             = [0.92, 0.90, 0.80, 1.00]
TextDisabled     = [0.48, 0.44, 0.34, 1.00]
Border           = [0.45, 0.35, 0.12, 0.50]
FrameBg          = [0.12, 0.09, 0.18, 0.75]
FrameBgHovered   = [0.18, 0.13, 0.28, 0.85]
Button           = [0.18, 0.13, 0.28, 0.80]
ButtonHovered    = [0.28, 0.20, 0.44, 0.90]
Header           = [0.16, 0.11, 0.26, 0.70]
HeaderHovered    = [0.26, 0.18, 0.40, 0.80]
CheckMark        = [0.88, 0.70, 0.20, 1.00]
ScrollbarGrab    = [0.45, 0.34, 0.10, 0.75]

[chat]
bubble_self_top  = [40,  60, 130, 225]
bubble_self_bot  = [18,  28,  70, 235]
bubble_other_top = [45,  38,  22, 220]
bubble_other_bot = [22,  18,  10, 235]
bubble_rounding  = 10.0

header_bg    = [14, 10,  22, 245]
active_bg    = [35, 26,  75, 185]
input_bg     = [18, 14,  30, 235]
input_border = [180, 140, 50, 130]
avatar_bg    = [160, 120, 35, 255]
pin_accent   = [210, 168, 50, 255]

sender_self  = [0.88, 0.70, 0.22, 1.0]
sender_other = [0.90, 0.88, 0.80, 1.0]
timestamp    = [0.48, 0.42, 0.30, 1.0]

chat_bg_top  = [0,  0,  0,  0]
chat_bg_bot  = [30, 15, 50, 28]

bob_amplitude   = 3.0
bob_period_ms   = 2400.0
flash_period_ms = 1100.0
fade_ms         = 200.0
```

---

## Tips

- **Start from a built-in theme** — copy its visual direction and tweak values from there.
- **Keep alpha in mind** — `WindowBg` at full opacity (α = 1.0) will hide game graphics behind the window. Values around 0.93–0.97 are typical.
- **`pin_accent` doubles as the notification flash tint** — pick something that stands out against your background.
- **`input_border` at zero alpha disables the border** entirely, which suits lighter themes that don't need the extra definition.
- **Gradient vs flat bubbles** — set `bubble_self_top` and `bubble_self_bot` to the same value for flat rounded corners, or different values for a top-to-bottom gradient (rounding is disabled for gradients).
- **Textures are optional** — omit `bg_texture` and `icon_texture` if you don't need them. Large or high-contrast textures can make messages hard to read.
- **ImThemes files drop straight in** — files exported from the ImThemes editor are supported without modification. Chat colors will be auto-derived from your ImGui slots if you don't add a `[chat]` section.
