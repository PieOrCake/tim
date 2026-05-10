# Hoard & Seek — Fix: Audit PushGW2Theme / PopGW2Theme Balance

## Context

Hoard & Seek is a GW2 Nexus addon. A debugging session on a related addon (Tyrian Instant Messaging) found that enabling Hoard & Seek causes permanent text input corruption in other addons for the remainder of the GW2 session. Disabling H&S does not fix it — only a game restart does.

The root cause is believed to be a Nexus loader bug that triggers when a second addon with an `RT_Render` callback is hot-loaded (this affects Mission Finder too, which has an additional separate bug). That Nexus bug is being reported upstream separately.

However, H&S has its own issue that should be fixed regardless, because it would cause silent visual corruption if `PopGW2Theme` is ever skipped.

## The Problem

H&S's `AddonRender` function (`src/dllmain.cpp`) wraps all rendering in a theme swap:

```cpp
void AddonRender() {
    PushGW2Theme();      // saves ImGui::GetStyle(), applies g_GW2Style
    // ...
    PopGW2Theme();       // restores saved style
}
```

`PushGW2Theme` writes directly to `ImGui::GetStyle()` — the shared, global ImGui style struct used by every addon in the process. If any code path exits `AddonRender` after `PushGW2Theme` without calling `PopGW2Theme`, the global style is permanently corrupted for the rest of the session.

There are multiple early-return paths in `AddonRender`:

```cpp
PushGW2Theme();                                  // line ~1610
if (!g_WindowVisible) { PopGW2Theme(); return; } // line ~1684 — has Pop ✓
if (!ImGui::Begin(...)) {                         // line ~1691
    PopGW2Theme(); ...; return;                  // has Pop ✓
}
// ... more render code with potential sub-returns ...
PopGW2Theme();                                   // final Pop ~line 1968
```

Each early return must have its own `PopGW2Theme()`. Missing even one would leave `ImGui::GetStyle()` permanently set to the GW2 theme for all other addons.

## The Fix

**Audit every exit path in `AddonRender` between the opening `PushGW2Theme()` and the final `PopGW2Theme()`.** Confirm that every `return` statement in that function is preceded by `PopGW2Theme()`.

The safest approach is to refactor the function so there is only **one** exit point, eliminating the need to manually add `PopGW2Theme` to each return. Example pattern:

```cpp
void AddonRender() {
    PushGW2Theme();
    RenderInternal();   // all logic moved here, returns freely
    PopGW2Theme();
}

static void RenderInternal() {
    if (!g_WindowVisible) return;
    if (!ImGui::Begin(...)) { ImGui::End(); return; }
    // ... render ...
    ImGui::End();
}
```

Alternatively, a RAII guard eliminates the risk entirely:

```cpp
struct ThemeGuard {
    ThemeGuard()  { PushGW2Theme(); }
    ~ThemeGuard() { PopGW2Theme(); }
};

void AddonRender() {
    ThemeGuard guard;
    if (!g_WindowVisible) return;
    // ... etc, PopGW2Theme called automatically on any return
}
```

## What NOT to change

- The `PushGW2Theme` / `PopGW2Theme` mechanism itself is sound — saving and restoring `ImGui::GetStyle()` is the correct way to apply a per-addon theme without affecting other addons.
- `BuildGW2Theme()` (called once in `AddonLoad`) correctly snapshots the style into `g_GW2Style` without modifying the global style — do not change this.
- All threading, API calls, and search logic are unrelated to this fix.

## Secondary note

The `g_PendingSearchResults` vector is written from a background thread and read from the render thread. The `g_SearchResultsReady` atomic flag is used as the synchronisation point. This is safe with C++11 default (`seq_cst`) memory ordering — the atomic store acts as a full barrier. No change needed here.

## Verification

After the fix:
1. The DLL compiles with no errors.
2. Every `return` statement inside `AddonRender` (after `PushGW2Theme` is called) is either preceded by `PopGW2Theme()` or the function uses the RAII guard pattern so `PopGW2Theme` is guaranteed by destruction.
