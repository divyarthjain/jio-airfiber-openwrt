# Doom on the router, played in a browser

The router has no screen, so the game runs on the router's CPU and streams its frames to a browser tab. Keys go back the other way.

```
browser tab  <-- WebSocket: 320x200 frames (RGB565) --  doom-web on the router (MT7986, aarch64)
             --  WebSocket: key down / key up       -->
```

## Why not the terminal
[doom-ascii](https://github.com/wojciech-graj/doom-ascii) runs over SSH, but it draws with text characters, so it looks rough. Terminals also never send key-up events, so held keys stutter. A browser canvas shows the real pixels and sends proper key-down/up.

The OpenWrt repo has `gzdoom` and `sdl2-doom`, but both need a display.

## How it is built
- Engine: doom-ascii (a doomgeneric port), pinned commit, with its terminal backend swapped for [`tools/doom-web/doomgeneric_web.c`](../tools/doom-web/doomgeneric_web.c). About 370 lines: a non-blocking HTTP + WebSocket server inside the game loop, plus the page.
- Cross-compiled on a Mac with Zig (`aarch64-linux-musl`, static, 527 KB). Zig comes through `uv`, so no Homebrew or toolchain install.
- Game data: [Freedoom](https://freedoom.github.io/) 0.13.0 (free licence), checksum verified by the build script.

## Use
```sh
cd tools/doom-web
./build.sh                     # router binary, host binary, Freedoom
python3 check.py               # self-check on this machine
./play.sh root@192.168.5.1     # copy to router RAM and run; Ctrl+C stops
```
Then open `http://192.168.5.1:8666`.

Controls: arrows or WASD move, Space or Ctrl fire, E use, Shift run, 1-7 weapons, Esc menu.

## Limits
- No sound, no mouse. One player at a time; a new tab takes over.
- About 4.5 MB/s of frames, so LAN only. It is not opened on the Tailscale firewall zone.
- Lives in `/tmp` (RAM): nothing written to flash, gone after a reboot.

## Bugs found on the way
Both are covered by `check.py`.
1. The server kept only one waiting connection. Browsers open idle preconnects, and each new one replaced the game's WebSocket before it was read. Symptom: page loads, then "disconnected, retrying" forever. Fix: up to 8 waiting connections, evict the oldest.
2. The `Sec-WebSocket-Key` header was matched case-sensitively. HTTP header names are case-insensitive and some clients send them in lowercase, so the server answered the upgrade with the HTML page. Found with Node's built-in WebSocket client. Fix: case-insensitive header lookup.

## Status
Working. Confirmed in a real browser against the router on 12 Oct 2026. Measured 70 fps from the router to a test client on the Mac over Wi-Fi. The fixed build passes `check.py`.
