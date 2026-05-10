# Draft Persistence, Contact Notes, Unread Count & Timestamps Toggle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add per-contact draft persistence, private contact notes, verify the per-contact unread count badge, and add a toggle to hide message timestamps.

**Architecture:** All four features live entirely in `dllmain.cpp` and `settings.ini`. No new files. Drafts are session-only (in-memory map). Notes and timestamp preference persist to `settings.ini`. No changes to ChatManager, ChatLinks, or GW2API.

**Tech Stack:** C++17, ImGui, existing `SaveSettings`/`LoadSettings` INI pattern in `dllmain.cpp`.

---

## File map

| File | Changes |
|---|---|
| `src/dllmain.cpp` | Add `g_Drafts` map; hook contact switch and send; add `g_ContactNotes` map; extend `SaveSettings`/`LoadSettings`; add note editing modal; render note in contact row; add `g_ShowTimestamps` toggle |

---

## Task 1: Draft persistence

Unsent text in `g_InputBuf` is lost when switching contacts. Save it per-contact in a session-only map and restore it when returning.

**Files:**
- Modify: `src/dllmain.cpp`

- [ ] **Step 1: Add the drafts map near the other globals (around line 574)**

```cpp
static std::unordered_map<std::string, std::string> g_Drafts;
```

- [ ] **Step 2: Save draft and load new one on contact switch**

Find the Selectable click handler (around line 2853):
```cpp
if (ImGui::Selectable(("##contact_" + convo->contact).c_str(), false, 0, ImVec2(0, itemHeight))) {
    g_SelectedContact = convo->contact;
```

Replace with:
```cpp
if (ImGui::Selectable(("##contact_" + convo->contact).c_str(), false, 0, ImVec2(0, itemHeight))) {
    // Persist current draft before switching
    if (!g_SelectedContact.empty())
        g_Drafts[g_SelectedContact] = g_InputBuf;
    g_SelectedContact = convo->contact;
    // Restore draft for the new contact
    auto dit = g_Drafts.find(convo->contact);
    if (dit != g_Drafts.end()) {
        strncpy(g_InputBuf, dit->second.c_str(), sizeof(g_InputBuf) - 1);
        g_InputBuf[sizeof(g_InputBuf) - 1] = '\0';
    } else {
        g_InputBuf[0] = '\0';
    }
```

- [ ] **Step 3: Clear draft on send**

Find the send handler (around line 3417) where `g_InputBuf[0] = '\0';` is set after sending. Add the erase immediately after:
```cpp
g_InputBuf[0] = '\0';
g_Drafts.erase(g_SelectedContact);
```

- [ ] **Step 4: Also save draft when the floating icon click switches contact**

Find the floating icon click that sets `g_SelectedContact` (around line 2722). Apply the same save/restore pattern:
```cpp
// Before changing g_SelectedContact:
if (!g_SelectedContact.empty())
    g_Drafts[g_SelectedContact] = g_InputBuf;
g_SelectedContact = c->contact;
auto dit = g_Drafts.find(c->contact);
if (dit != g_Drafts.end()) {
    strncpy(g_InputBuf, dit->second.c_str(), sizeof(g_InputBuf) - 1);
    g_InputBuf[sizeof(g_InputBuf) - 1] = '\0';
} else {
    g_InputBuf[0] = '\0';
}
```

- [ ] **Step 5: Build and test**

```bash
cd build && make -j$(nproc) 2>&1
```

Expected: clean build. In-game: type a message, switch contacts, switch back — text is restored. Sending clears the draft.

- [ ] **Step 6: Commit**

```bash
git add src/dllmain.cpp
git commit -m "feat: persist unsent message drafts per contact"
```

---

## Task 2: Verify per-contact unread count badge

The badge is already implemented (dllmain.cpp lines 2944–2957). This task confirms it works correctly.

**Files:**
- Modify: `src/dllmain.cpp` (fix only if broken)

- [ ] **Step 1: Read the existing badge code**

Lines 2944–2957 render a red circle badge with the unread count next to the contact name when `convo->unread_count > 0`. Check that `MarkAsRead` is called when the conversation is opened (around line 3042).

- [ ] **Step 2: Confirm MarkAsRead is called on conversation open**

Line ~3042 should have:
```cpp
TyrianIM::ChatManager::MarkAsRead(g_SelectedContact);
```
This resets `unread_count` to 0, hiding the badge. If this line is absent, add it inside the block that runs when `g_SelectedContact` changes to a conversation with unread messages.

- [ ] **Step 3: Test in-game**

Receive a whisper while viewing a different conversation. Confirm the sender's row shows a red count badge. Switch to that conversation. Confirm the badge disappears.

No code changes expected unless a bug is found.

---

## Task 3: Contact notes

Private per-contact notes stored locally. Shown as a subtitle in the contact row. Editable via right-click → "Edit note…".

**Files:**
- Modify: `src/dllmain.cpp`

- [ ] **Step 1: Add the notes map and note-edit modal state near other globals**

```cpp
static std::unordered_map<std::string, std::string> g_ContactNotes;
static std::string g_NoteEditContact;
static char        g_NoteEditBuf[500] = {};
static bool        g_NoteEditOpen     = false;
```

- [ ] **Step 2: Save notes in SaveSettings**

In `SaveSettings()`, after the `pinned_contacts` block, add:
```cpp
for (const auto& [contact, note] : g_ContactNotes) {
    if (note.empty()) continue;
    // Escape newlines so the INI stays single-line per entry
    std::string escaped;
    for (char c : note)
        escaped += (c == '\n') ? "\\n" : std::string(1, c);
    f << "note_" << contact << "=" << escaped << "\n";
}
```

- [ ] **Step 3: Load notes in LoadSettings**

In the `LoadSettings` while-loop, add a new branch after the `pinned_contacts` branch:
```cpp
else if (key.size() > 5 && key.substr(0, 5) == "note_") {
    std::string contact = key.substr(5);
    // Unescape \n
    std::string note;
    for (size_t ni = 0; ni < val.size(); ni++) {
        if (val[ni] == '\\' && ni + 1 < val.size() && val[ni+1] == 'n') {
            note += '\n'; ni++;
        } else {
            note += val[ni];
        }
    }
    g_ContactNotes[contact] = note;
}
```

- [ ] **Step 4: Add "Edit note…" to the contact right-click context menu**

Find the context menu block (around line 2870) and add before `ImGui::Separator()` / "Delete conversation":
```cpp
if (ImGui::MenuItem("Edit note\xe2\x80\xa6")) {
    g_NoteEditContact = convo->contact;
    const auto& n = g_ContactNotes[convo->contact];
    strncpy(g_NoteEditBuf, n.c_str(), sizeof(g_NoteEditBuf) - 1);
    g_NoteEditBuf[sizeof(g_NoteEditBuf) - 1] = '\0';
    g_NoteEditOpen = true;
}
ImGui::Separator();
```

- [ ] **Step 5: Render the note-edit modal**

Add this block after the "Confirm Delete" modal (around line 2990), still inside the Contacts child window render:
```cpp
if (g_NoteEditOpen) {
    ImGui::OpenPopup("Edit Note##NoteEdit");
    g_NoteEditOpen = false;
}
if (ImGui::BeginPopupModal("Edit Note##NoteEdit", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(g_ActiveTheme.sender_other, "%s", g_NoteEditContact.c_str());
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputTextMultiline("##notetext", g_NoteEditBuf, sizeof(g_NoteEditBuf),
        ImVec2(320, 80));
    ImGui::Spacing();
    if (ImGui::Button("Save", ImVec2(100, 0))) {
        if (g_NoteEditBuf[0])
            g_ContactNotes[g_NoteEditContact] = g_NoteEditBuf;
        else
            g_ContactNotes.erase(g_NoteEditContact);
        SaveSettings();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0)))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
```

- [ ] **Step 6: Display note snippet in the contact row**

Find the subtitle block (around line 2960):
```cpp
// Subtitle
if (!convo->display_name.empty() && convo->display_name != convo->contact) {
    float subFs = font->FontSize * (g_FontScale * 0.85f);
    dl->AddText(font, subFs, ImVec2(textX, cursor.y + 6 + fs + 2),
        ImGui::ColorConvertFloat4ToU32(g_ActiveTheme.timestamp), convo->contact.c_str());
}
```

Replace with:
```cpp
// Subtitle: note takes priority over account name
{
    float subFs = font->FontSize * (g_FontScale * 0.85f);
    auto nit = g_ContactNotes.find(convo->contact);
    if (nit != g_ContactNotes.end() && !nit->second.empty()) {
        // First line of the note, truncated to 32 chars
        const std::string& noteText = nit->second;
        size_t nl = noteText.find('\n');
        std::string snippet = noteText.substr(0, std::min(nl == std::string::npos ? noteText.size() : nl, (size_t)32));
        if (snippet.size() < noteText.size()) snippet += "\xe2\x80\xa6"; // …
        dl->AddText(font, subFs, ImVec2(textX, cursor.y + 6 + fs + 2),
            IM_COL32(160, 200, 160, 180), snippet.c_str()); // soft green tint for notes
    } else if (!convo->display_name.empty() && convo->display_name != convo->contact) {
        dl->AddText(font, subFs, ImVec2(textX, cursor.y + 6 + fs + 2),
            ImGui::ColorConvertFloat4ToU32(g_ActiveTheme.timestamp), convo->contact.c_str());
    }
}
```

- [ ] **Step 7: Build and test**

```bash
cd build && make -j$(nproc) 2>&1
```

Expected: clean build. In-game: right-click a contact → "Edit note…" → type a note → Save. Contact row shows the note snippet in soft green. Restart GW2; note persists.

- [ ] **Step 8: Commit**

```bash
git add src/dllmain.cpp
git commit -m "feat: per-contact notes with inline row display and INI persistence"
```

---

## Task 4: Timestamps toggle

Timestamps affect bubble widths (they're included in `layout.bubbleW`). The toggle must invalidate the bubble layout cache so bubbles resize correctly.

**Files:**
- Modify: `src/dllmain.cpp`

- [ ] **Step 1: Add the global toggle near other settings globals**

```cpp
static bool g_ShowTimestamps = true;
```

- [ ] **Step 2: Gate timestamp measurement in ComputeBubbleLayout**

Find lines ~667-669 in `ComputeBubbleLayout`:
```cpp
layout.displayTimestamp = FormatDisplayTime(msg.epoch_ms);
float timeFs = font->FontSize * (fontScale * 0.85f);
layout.timeSize = font->CalcTextSizeA(timeFs, FLT_MAX, 0.0f, layout.displayTimestamp.c_str());
```

Replace with:
```cpp
layout.displayTimestamp = FormatDisplayTime(msg.epoch_ms);
if (g_ShowTimestamps) {
    float timeFs = font->FontSize * (fontScale * 0.85f);
    layout.timeSize = font->CalcTextSizeA(timeFs, FLT_MAX, 0.0f, layout.displayTimestamp.c_str());
} else {
    layout.timeSize = ImVec2(0, 0);
}
```

- [ ] **Step 3: Gate timestamp rendering**

Find lines ~3270-3276 in the bubble render loop:
```cpp
// Timestamp (right of name line, slightly smaller)
float timeFs = font->FontSize * (g_FontScale * 0.85f);
ImVec4 tsCol = g_ActiveTheme.timestamp;
tsCol.w *= msgAlpha;
dl->AddText(font, timeFs,
    ImVec2(bubbleX + layout.bubbleW - padding - layout.timeSize.x, cursor.y + padding + (layout.nameSize.y - layout.timeSize.y)),
    ImGui::ColorConvertFloat4ToU32(tsCol), layout.displayTimestamp.c_str());
```

Wrap in a guard:
```cpp
if (g_ShowTimestamps) {
    float timeFs = font->FontSize * (g_FontScale * 0.85f);
    ImVec4 tsCol = g_ActiveTheme.timestamp;
    tsCol.w *= msgAlpha;
    dl->AddText(font, timeFs,
        ImVec2(bubbleX + layout.bubbleW - padding - layout.timeSize.x, cursor.y + padding + (layout.nameSize.y - layout.timeSize.y)),
        ImGui::ColorConvertFloat4ToU32(tsCol), layout.displayTimestamp.c_str());
}
```

- [ ] **Step 4: Add checkbox to settings panel and invalidate cache on change**

Find the settings panel (search for `SliderInt.*Send delay` around line 3552). Add near the other display toggles:
```cpp
if (ImGui::Checkbox("Show message timestamps", &g_ShowTimestamps)) {
    InvalidateBubbleCache();
    SaveSettings();
}
```

- [ ] **Step 5: Persist in SaveSettings**

In `SaveSettings()`, add:
```cpp
f << "show_timestamps=" << (g_ShowTimestamps ? 1 : 0) << "\n";
```

- [ ] **Step 6: Load in LoadSettings**

In the `LoadSettings` while-loop, add:
```cpp
else if (key == "show_timestamps") g_ShowTimestamps = (val == "1");
```

- [ ] **Step 7: Build and test**

```bash
cd build && make -j$(nproc) 2>&1
```

Expected: clean build. In-game: open settings → uncheck "Show message timestamps" → bubbles shrink to fit content without the timestamp column. Re-check → timestamps return. Setting persists across sessions.

- [ ] **Step 8: Commit**

```bash
git add src/dllmain.cpp
git commit -m "feat: toggle to show/hide message timestamps"
```
