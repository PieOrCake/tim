# Mission Finder — Fix: Remove WndProc Registration

## Context

Mission Finder is a GW2 Nexus addon. A debugging session on a related addon (Tyrian Instant Messaging) identified that enabling Mission Finder causes permanent text input corruption in other addons for the remainder of the GW2 session. Disabling Mission Finder does not fix it — only a game restart does. The same symptom occurs with at least one other addon (Hoard & Seek), pointing to a Nexus bug triggered by the presence of a second addon. However, Mission Finder has an additional specific issue that should be fixed regardless.

## The Problem

`src/dllmain.cpp` lines 25–29 and 49 and 84:

```cpp
UINT WndProcCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_gameHandle)
        g_gameHandle = hWnd;
    return uMsg;
}
// ...
APIDefs->WndProc_Register(WndProcCallback);   // AddonLoad
// ...
APIDefs->WndProc_Deregister(WndProcCallback); // AddonUnload
```

The WndProc callback is registered **solely to capture `g_gameHandle`** (the GW2 window handle). It does nothing else — it captures the HWND on the first message and returns `uMsg` (pass-through) for everything else.

Registering with Nexus's WndProc system means Nexus inserts this callback into its internal message-dispatch list and calls it on every WM_KEYDOWN, WM_KEYUP, WM_CHAR, and mouse message — i.e. every input event, every frame. When the addon is hot-unloaded and the callback is deregistered, Nexus removes it from the list. There is a known interaction where this add/remove cycle leaves Nexus's dispatch list in a state that permanently breaks input processing for the rest of the session.

This is an unnecessary use of the WndProc API. The game HWND can be obtained directly via `FindWindowA` without touching Nexus's input chain.

## The Fix

Remove the WndProc entirely. Populate `g_gameHandle` lazily via `FindWindowA` inside `PasteAndSendFromThread` (which is the only place it's used).

### Changes required

**`src/dllmain.cpp`:**

1. Delete the `WndProcCallback` function (lines 25–29).
2. Remove `APIDefs->WndProc_Register(WndProcCallback);` from `AddonLoad`.
3. Remove `APIDefs->WndProc_Deregister(WndProcCallback);` from `AddonUnload`.

**`src/ChatMessage.cpp`:**

In `PasteAndSendFromThread`, replace the existing `g_gameHandle` usage at the top with a lazy `FindWindowA` lookup:

```cpp
static void PasteAndSendFromThread(std::string message) {
    HWND game = g_gameHandle;
    if (!game) {
        game = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
        if (!game) game = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
        if (!game) return;
        g_gameHandle = game;
    }
    // ... rest unchanged
}
```

The `g_gameHandle` global in `Shared.h` and `Shared.cpp` can remain as-is — it just gets populated by FindWindowA on first send instead of by WndProc.

## What NOT to change

- The `return uMsg` pattern elsewhere in the codebase is correct Nexus convention (return `uMsg` = pass through, return `0` = block).
- The `SendInput` + `SendMessage` approach in `PasteAndSendFromThread` is intentional (Ctrl modifier must go via `SendInput` to register at the hardware level; the V key goes via `SendMessage` to bypass ImGui).
- No other files need changes.

## Verification

After the fix, build the DLL and confirm:
1. The DLL compiles with no errors.
2. The words `WndProc_Register` and `WndProc_Deregister` no longer appear anywhere in `src/dllmain.cpp`.
3. `FindWindowA` is present in `src/ChatMessage.cpp`.
