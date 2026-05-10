# Message Date Labels Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show weekday + date in message bubble timestamps for messages from previous days (e.g. `Monday, 13 Mar 26 @ 21:30`), while keeping today's messages showing just `HH:MM`.

**Architecture:** Add a `displayTimestamp` string to `BubbleLayout`, computed from `epoch_ms` at layout time via a new `FormatDisplayTime` function. The render loop draws `displayTimestamp` instead of `msg.timestamp`. No changes to the persisted data format.

**Tech Stack:** C++17, Dear ImGui, MinGW cross-compile. No test suite — verification is build + manual in-game check.

---

### Task 1: Add `FormatDisplayTime` function

**Files:**
- Modify: `src/dllmain.cpp` — add function after `FormatTime` (~line 1057)

- [ ] **Step 1: Add the function after `FormatTime`**

In `src/dllmain.cpp`, directly after the closing brace of `FormatTime` (line 1057), insert:

```cpp
// Returns "HH:MM" for today's messages, "Weekday, DD Mon YY @ HH:MM" for older ones
static std::string FormatDisplayTime(uint64_t epoch_ms) {
    time_t t = static_cast<time_t>(epoch_ms / 1000);
    struct tm msg_tm;
    localtime_s(&msg_tm, &t);

    time_t now_t = time(nullptr);
    struct tm now_tm;
    localtime_s(&now_tm, &now_t);

    bool sameDay = (msg_tm.tm_year == now_tm.tm_year &&
                    msg_tm.tm_yday == now_tm.tm_yday);
    if (sameDay) {
        char buf[8];
        strftime(buf, sizeof(buf), "%H:%M", &msg_tm);
        return std::string(buf);
    }

    char buf[40];
    strftime(buf, sizeof(buf), "%A, %d %b %y @ %H:%M", &msg_tm);
    return std::string(buf);
}
```

- [ ] **Step 2: Build to confirm it compiles**

```bash
cd /home/tony/Dev/tyrian_instant_messaging/build
make 2>&1 | tail -20
```

Expected: build succeeds (no errors, warnings about unused function are fine at this point).

---

### Task 2: Add `displayTimestamp` to `BubbleLayout` and populate it

**Files:**
- Modify: `src/dllmain.cpp` — `BubbleLayout` struct (~line 741) and `ComputeBubbleLayout` (~line 794)

- [ ] **Step 1: Add `displayTimestamp` field to `BubbleLayout`**

In the `BubbleLayout` struct (around line 741), add the field after `senderLabel`:

```cpp
struct BubbleLayout {
    float bubbleW, bubbleH;
    ImVec2 nameSize, timeSize, msgSize;
    float textWrapWidth;
    std::string senderLabel;
    std::string displayTimestamp;   // <-- add this line
    bool isSystem;
    // For system messages
    float sysW, sysH;
};
```

- [ ] **Step 2: Populate `displayTimestamp` and use it for size measurement**

In `ComputeBubbleLayout`, find these two lines (around line 794):

```cpp
    layout.nameSize = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, layout.senderLabel.c_str());
    float timeFs = font->FontSize * (fontScale * 0.85f);
    layout.timeSize = font->CalcTextSizeA(timeFs, FLT_MAX, 0.0f, msg.timestamp.c_str());
```

Replace them with:

```cpp
    layout.nameSize = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, layout.senderLabel.c_str());
    layout.displayTimestamp = FormatDisplayTime(msg.epoch_ms);
    float timeFs = font->FontSize * (fontScale * 0.85f);
    layout.timeSize = font->CalcTextSizeA(timeFs, FLT_MAX, 0.0f, layout.displayTimestamp.c_str());
```

- [ ] **Step 3: Build to confirm it compiles**

```bash
cd /home/tony/Dev/tyrian_instant_messaging/build
make 2>&1 | tail -20
```

Expected: build succeeds with no errors.

---

### Task 3: Update render loop to draw `displayTimestamp`

**Files:**
- Modify: `src/dllmain.cpp` — render loop timestamp draw (~line 1896)

- [ ] **Step 1: Replace `msg.timestamp` with `layout.displayTimestamp` in the render call**

Find this line (around line 1896):

```cpp
                ImGui::ColorConvertFloat4ToU32(tsCol), msg.timestamp.c_str());
```

Replace with:

```cpp
                ImGui::ColorConvertFloat4ToU32(tsCol), layout.displayTimestamp.c_str());
```

- [ ] **Step 2: Build final DLL**

```bash
cd /home/tony/Dev/tyrian_instant_messaging/build
make 2>&1 | tail -20
```

Expected: build succeeds, `build/TyrianIM.dll` updated.

- [ ] **Step 3: Commit**

```bash
git add src/dllmain.cpp
git commit -m "feat: show weekday and date on bubbles for messages from previous days"
```

---

## Manual verification checklist

After copying the DLL into GW2:

- [ ] Messages received today show `HH:MM` only (e.g. `14:32`)
- [ ] Messages from any previous day show `Weekday, DD Mon YY @ HH:MM` (e.g. `Monday, 13 Mar 26 @ 21:30`)
- [ ] Bubble width expands to fit the longer date string without clipping
- [ ] Both incoming and outgoing old messages show the date label
- [ ] History-loaded messages (from TSV) correctly show dates
