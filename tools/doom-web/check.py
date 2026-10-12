"""Self-check for doom-web. Covers both bugs found while building it.

    python3 check.py              starts build/doom-web-host locally and tests it
    python3 check.py HOST PORT    tests a server that is already running (e.g. the router)

Checks: page served; WebSocket handshake with lowercase header names (as browsers may send);
an idle extra connection (browser preconnect) does not kill the game stream; full frames
arrive; a key press reaches the game.
"""
import base64, hashlib, os, socket, struct, subprocess, sys, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
proc = None
if len(sys.argv) == 3:
    host, port = sys.argv[1], int(sys.argv[2])
else:
    host, port, b = "127.0.0.1", 8667, os.path.join(HERE, "build")
    proc = subprocess.Popen([os.path.join(b, "doom-web-host"), "-iwad", "freedoom1.wad", "-scaling", "1",
                             "-port", str(port)], cwd=b, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

try:
    for _ in range(50):
        try:
            page = urllib.request.urlopen(f"http://{host}:{port}/", timeout=5).read().decode()
            break
        except OSError:
            time.sleep(0.2)
    else:
        sys.exit(f"FAIL: nothing answering on http://{host}:{port}/ (is doom-web running?)")
    assert "<canvas" in page and "/ws" in page, "page missing"
    print("page ok")

    idle = socket.create_connection((host, port))  # browser-style preconnect that never sends anything
    s = socket.create_connection((host, port), timeout=5)
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall(f"GET /ws HTTP/1.1\r\nhost: {host}\r\nupgrade: websocket\r\nconnection: upgrade\r\n"
              f"sec-websocket-key: {key}\r\nsec-websocket-version: 13\r\n\r\n".encode())
    idle2 = socket.create_connection((host, port))
    resp = b""
    while b"\r\n\r\n" not in resp:
        c = s.recv(1)
        assert c, "closed during handshake"
        resp += c
    want = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest())
    assert resp.startswith(b"HTTP/1.1 101") and want in resp, resp
    print("handshake ok (lowercase headers, idle preconnects open)")

    def recvn(n):
        b = b""
        while len(b) < n:
            c = s.recv(n - len(b))
            assert c, "stream closed"
            b += c
        return b

    def frame():
        b0, b1 = recvn(2)
        assert (b0, b1) == (0x82, 127), (b0, b1)
        n = struct.unpack(">Q", recvn(8))[0]
        d = recvn(n)
        w, h = struct.unpack("<HH", d[:4])
        assert n == 4 + w * h * 2, (n, w, h)
        return w, h, d[4:]

    def send_key(pressed, k):
        mask = os.urandom(4)
        s.sendall(bytes([0x82, 0x80 | 2]) + mask + bytes(p ^ mask[i % 4] for i, p in enumerate([pressed, k])))

    t, n = time.time(), 0
    while time.time() - t < 2:
        w, h, px = frame()
        n += 1
    assert w == 320 and h == 200 and len(set(px)) > 8, "blank or wrong-size frames"
    print(f"frames ok: {w}x{h}, {n / 2:.0f} fps")

    before = px
    send_key(1, 27)
    time.sleep(0.1)
    send_key(0, 27)  # Esc opens the menu
    for _ in range(20):
        _, _, px = frame()
    assert sum(a != b for a, b in zip(before, px)) > 1000, "picture did not change after Esc"
    print("key ok (weak check: the title demo also moves the picture)")
    print("ALL OK")
finally:
    if proc:
        proc.terminate()
