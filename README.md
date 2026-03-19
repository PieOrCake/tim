# Tyrian Instant Messaging

A Guild Wars 2 addon for [Raidcore Nexus](https://raidcore.gg/Nexus) that provides an instant messaging window for whisper conversations, styled like ICQ or Signal Desktop.

## Status

**Work in progress.** The UI shell and addon infrastructure are complete. Whisper interception requires reverse engineering of `Gw2-64.exe` to find the whisper send/receive handler functions.

Currently using **squad/party chat** (via Unofficial Extras) as a testbed for the messaging UI.

### What works now
- Nexus addon with ImGui messenger window
- Contact list with conversation threading
- Message history persistence
- Squad/party chat interception via Unofficial Extras events
- Keybind toggle (Ctrl+Shift+M)
- Quick Access shortcut

### What needs RE work
- Incoming whisper interception (hook the receive handler in `Gw2-64.exe`)
- Outgoing whisper interception (hook the send handler)
- Sending whispers from the UI (either via hooked send function or input simulation)

## Architecture

```
src/
  dllmain.cpp       - Nexus addon entry, ImGui messenger UI, event handling
  ChatManager.h/cpp - Message storage, conversation threading, persistence
  WhisperHook.h/cpp - Pattern scanning & MinHook infrastructure for whisper interception
```

### WhisperHook

`WhisperHook.cpp` contains the scaffolding for hooking whisper functions:
- Pattern scanner for finding functions by byte signature
- MinHook integration via Nexus API
- Placeholder TODO blocks where function signatures go after RE

Once you identify the whisper handler functions in Ghidra, fill in:
1. The byte patterns and masks in `InstallHooks()`
2. The detour function signatures matching the original calling convention
3. Call `s_Callback()` from the detours to feed messages into ChatManager

## Building

### Prerequisites
- CMake 3.20+
- MinGW cross-compiler (`x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-g++`)

### Build Commands
```bash
mkdir build && cd build
cmake ..
make
```

The build produces `TyrianIM.dll`.

## Installation

1. Install [Nexus](https://raidcore.gg/Nexus)
2. Copy `TyrianIM.dll` to `<GW2>/addons/`
3. For squad/party chat testbed: also install ArcDPS + Unofficial Extras
4. Launch GW2 - toggle window with Ctrl+Shift+M or the Quick Access icon

## License

MIT
