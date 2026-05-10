# Pin Conversations — Design Spec

## Context

Users may have a handful of contacts they whisper frequently. Without pinning, those contacts drift down the list the moment a less-important conversation gets a new message. Pinning lets users anchor specific conversations to the top of the contact list permanently, regardless of activity.

---

## Data Model

`Conversation` (ChatManager.h) gains one new field: a boolean `pinned` flag, defaulting to false.

`ChatManager` gains two new public methods:
- **TogglePin(contact)** — flips the pinned flag for a contact, marks the sort cache dirty, and immediately saves settings.
- **IsPinned(contact)** — returns whether a contact is currently pinned (used to label the right-click menu item correctly).

---

## Sort Order

The existing sort in `ChatManager::GetConversations()` is extended with one rule: pinned conversations always appear above unpinned ones. Within each group (pinned / unpinned), the existing recency order (most recent message first) is preserved.

---

## Persistence

A new `pinned_contacts` key is added to `settings.ini`, storing a comma-separated list of pinned account names (e.g. `Name.1234,Other.5678`).

- **On load** (`LoadSettings`): the list is read and the pinned flag is stamped onto any matching conversations already in memory.
- **On toggle** (`TogglePin`): settings are saved immediately so pins survive a game restart or crash.
- **On new conversation** (`GetOrCreate`): the contact name is checked against the pinned list and the flag is set if it matches, so pins are correctly applied to conversations that arrive after load.

---

## UI — Right-Click Menu

Right-clicking a contact row in the left panel opens a small context menu with a single item: **"Pin"** or **"Unpin"** depending on current state. Selecting it calls `TogglePin`.

---

## UI — Visual Indicator

Pinned contact rows display a small gold pin marker (a Unicode pin character rendered in the GW2 warm-gold accent colour) on the right side of the row. This is drawn inline within the existing contact row rendering, after the contact name and unread badge. Unpinned rows show nothing extra.

Optionally, a subtle separator line can be drawn between the last pinned row and the first unpinned row, making the two groups visually distinct at a glance.

---

## Files to Modify

| File | Change |
|---|---|
| `src/ChatManager.h` | Add `pinned` field to `Conversation`; declare `TogglePin` and `IsPinned` |
| `src/ChatManager.cpp` | Implement `TogglePin`, `IsPinned`; update sort comparator; update `LoadSettings` / `SaveSettings`; stamp flag in `GetOrCreate` |
| `src/dllmain.cpp` | Add right-click context menu to contact rows; render pin indicator on pinned rows; draw optional separator between groups |

---

## Verification

1. Build the DLL (`cmake` + `make`) — must produce no errors or warnings.
2. In-game: right-click a contact → "Pin" → contact jumps to top of list, gold pin marker appears.
3. Right-click again → "Unpin" → contact returns to its recency position, marker disappears.
4. Pin two contacts — confirm they are ordered by recency among themselves.
5. Restart GW2 — pins must survive (check `settings.ini` for `pinned_contacts` key).
6. Receive a whisper from an unpinned contact — confirm it does not displace a pinned contact.
