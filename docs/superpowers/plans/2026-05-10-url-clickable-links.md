# URL Clickable Links Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect `http://` and `https://` URLs in chat messages and render them as clickable inline links showing only the domain name; clicking opens the URL in the system browser.

**Architecture:** URLs are parsed in `ParseSegments` (ChatLinks.cpp) into `Segment` structs with the new `GW2LinkType::Url` type — immediately `Resolved`, no async fetch. The existing per-segment invisible button loop in dllmain already handles hover/click; a new branch there calls `ShellExecuteA` instead of copying to clipboard.

**Tech Stack:** C++ / Win32 / Dear ImGui — cross-compiled with MinGW. Build: `cd build && make`. No test suite; verification is a clean build.

---

### Task 1: Add `Url` to `GW2LinkType` enum

**Files:**
- Modify: `src/ChatLinks.h:10-21`

- [ ] **Step 1: Add `Url` to the enum**

In `src/ChatLinks.h`, add `Url` before `Unknown`:

```cpp
enum class GW2LinkType : uint8_t {
    Item,           // wire byte 0x02 — item with optional quantity/upgrades
    MapLink,        // wire byte 0x04 — waypoint / POI / vista
    Skill,          // wire byte 0x06
    Trait,          // wire byte 0x07
    Recipe,         // wire byte 0x09
    Skin,           // wire byte 0x0A
    Outfit,         // wire byte 0x0B
    BuildTemplate,  // wire byte 0x0F — native GW2 build template
    AE2Build,       // AE2:... Alter Ego build code
    Url,            // http:// or https:// web link
    Unknown
};
```

- [ ] **Step 2: Verify it builds**

```bash
cd build && make 2>&1 | tail -5
```

Expected: clean build (zero errors).

- [ ] **Step 3: Commit**

```bash
git add src/ChatLinks.h
git commit -m "feat: add GW2LinkType::Url for web URLs"
```

---

### Task 2: Detect URLs in `ParseSegments`

**Files:**
- Modify: `src/ChatLinks.cpp:289-333` (the `while` loop inside `ParseSegments`)

- [ ] **Step 1: Add URL detection block at the top of the while loop**

In `src/ChatLinks.cpp`, inside `ParseSegments`, add this block **before** the `// --- AE2 build code ---` comment (i.e., as the very first check inside `while (i < text.size())`):

```cpp
    while (i < text.size()) {
        // --- HTTP/HTTPS URL ---
        bool is_http  = i + 7 <= text.size() && text.compare(i, 7, "http://")  == 0;
        bool is_https = i + 8 <= text.size() && text.compare(i, 8, "https://") == 0;
        if (is_http || is_https) {
            size_t j = i;
            while (j < text.size() && !std::isspace((unsigned char)text[j])) j++;
            std::string url = text.substr(i, j - i);
            size_t proto_end = url.find("://");
            std::string domain;
            if (proto_end != std::string::npos) {
                size_t dom_start = proto_end + 3;
                size_t dom_end   = url.find('/', dom_start);
                domain = url.substr(dom_start,
                    dom_end == std::string::npos ? std::string::npos : dom_end - dom_start);
            } else {
                domain = url;
            }
            LinkSegment seg;
            seg.type         = GW2LinkType::Url;
            seg.raw          = url;
            seg.display      = domain;
            seg.tooltip_text = url;
            seg.state        = LinkState::Resolved;
            seg.colour       = IM_COL32(100, 160, 255, 255);
            pushLink(std::move(seg));
            i = j;
            continue;
        }

        // --- AE2 build code ---
        // ... rest of existing loop unchanged
```

- [ ] **Step 2: Verify it builds**

```bash
cd build && make 2>&1 | tail -5
```

Expected: clean build (zero errors).

- [ ] **Step 3: Commit**

```bash
git add src/ChatLinks.cpp
git commit -m "feat: parse http/https URLs into Url link segments"
```

---

### Task 3: Open URL in browser on click

**Files:**
- Modify: `src/dllmain.cpp:3575-3588` (the `if (lClicked)` block in the per-link button loop)

- [ ] **Step 1: Replace the `lClicked` block with a URL-aware branch**

The current block (lines 3575–3588) is:

```cpp
                    if (lClicked) {
                        std::string ctxt =
                            (seg.link.type == TyrianIM::GW2LinkType::AE2Build &&
                             !seg.link.ae2_chat_link.empty())
                            ? seg.link.ae2_chat_link : NormalizeGW2Text(seg.link.raw);
                        ImGui::SetClipboardText(ctxt.c_str());
                        if (seg.link.type == TyrianIM::GW2LinkType::MapLink)
                            g_ClipboardMsg = "Waypoint link copied \xe2\x80\x94 paste in chat to use";
                        else if (seg.link.type == TyrianIM::GW2LinkType::AE2Build)
                            g_ClipboardMsg = "Build link copied";
                        else
                            g_ClipboardMsg = "Link copied";
                        g_ClipboardMsgExpiry = ImGui::GetTime() + 2.5;
                    }
```

Replace it with:

```cpp
                    if (lClicked) {
                        if (seg.link.type == TyrianIM::GW2LinkType::Url) {
                            ShellExecuteA(NULL, "open", seg.link.raw.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        } else {
                            std::string ctxt =
                                (seg.link.type == TyrianIM::GW2LinkType::AE2Build &&
                                 !seg.link.ae2_chat_link.empty())
                                ? seg.link.ae2_chat_link : NormalizeGW2Text(seg.link.raw);
                            ImGui::SetClipboardText(ctxt.c_str());
                            if (seg.link.type == TyrianIM::GW2LinkType::MapLink)
                                g_ClipboardMsg = "Waypoint link copied \xe2\x80\x94 paste in chat to use";
                            else if (seg.link.type == TyrianIM::GW2LinkType::AE2Build)
                                g_ClipboardMsg = "Build link copied";
                            else
                                g_ClipboardMsg = "Link copied";
                            g_ClipboardMsgExpiry = ImGui::GetTime() + 2.5;
                        }
                    }
```

- [ ] **Step 2: Verify it builds**

```bash
cd build && make 2>&1 | tail -5
```

Expected: clean build (zero errors).

- [ ] **Step 3: Commit**

```bash
git add src/dllmain.cpp
git commit -m "feat: open URLs in system browser on click"
```

---

### Task 4: Final build

- [ ] **Step 1: Clean build**

```bash
cd build && make 2>&1 | tail -10
```

Expected: `build/TyrianIM.dll` produced with zero errors or warnings.
