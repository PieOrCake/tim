# Tyrian Instant Messaging

A Guild Wars 2 addon for [Raidcore Nexus](https://raidcore.gg/Nexus) that provides an instant messaging window for whisper conversations.

## AI Notice

This addon has been 100% created in [Windsurf](https://windsurf.com/) using Claude. I understand that some folks have a moral, financial or political objection to creating software using an LLM. I just wanted to make a useful tool for the GW2 community, and this was the only way I could do it.

If an LLM creating software upsets you, then perhaps this repo isn't for you. Move on, and enjoy your day.

## Status

The addon is currently in development, and is not yet ready for use.

## Building

### Prerequisites
- CMake 3.20+
- MinGW cross-compiler (`x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-g++`)

### Build Commands
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-toolchain.cmake ..
make
```

The build produces `TyrianIM.dll`.

## Installation

1. Install [Nexus](https://raidcore.gg/Nexus)
2. Copy `TyrianIM.dll` to `<GW2>/addons/`
3. Launch GW2 — toggle window with Ctrl+Shift+M or the Quick Access icon

## License

This software is provided as-is, without a warranty of any kind. Use at your own risk. It might delete your files, melt your PC, burn your house down, or cause world peace. Probably not that last one, but one can hope.