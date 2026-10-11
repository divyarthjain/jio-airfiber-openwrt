#!/usr/bin/env python3
# Minimal read-only TFTP server (RFC1350 + blksize/tsize OACK). Serves one dir.
import socket, os, sys, struct
HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.2"
ROOT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else ".")
OACK, DATA, ACK, ERR, RRQ = 6, 3, 4, 5, 1
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((HOST, 69))
print(f"TFTP read-only server on {HOST}:69 serving {ROOT}", flush=True)
def send_file(path, cli, opts):
    blksize = int(opts.get("blksize", 512))
    with open(path, "rb") as f:
        data = f.read()
    t = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    t.settimeout(5)
    # OACK if options requested
    if opts:
        o = opts.copy()
        if "tsize" in o: o["tsize"] = str(len(data))
        pkt = struct.pack("!H", OACK) + b"".join(k.encode()+b"\0"+str(v).encode()+b"\0" for k,v in o.items())
        t.sendto(pkt, cli)
        try: t.recvfrom(1024)  # ack block 0
        except socket.timeout: print("no OACK ack", flush=True); return
    blk, off = 1, 0
    while True:
        chunk = data[off:off+blksize]
        for _try in range(5):
            t.sendto(struct.pack("!HH", DATA, blk & 0xFFFF) + chunk, cli)
            try:
                r,_ = t.recvfrom(1024)
                if len(r)>=4 and struct.unpack("!H", r[:2])[0]==ACK and struct.unpack("!H", r[2:4])[0]==(blk & 0xFFFF):
                    break
            except socket.timeout:
                continue
        else:
            print(f"timeout blk {blk}", flush=True); t.close(); return
        off += blksize; blk += 1
        if len(chunk) < blksize:
            print(f"sent {path} ({len(data)} bytes)", flush=True); break
    t.close()
while True:
    try:
        pkt, cli = s.recvfrom(2048)
    except KeyboardInterrupt:
        print("bye", flush=True); break
    if struct.unpack("!H", pkt[:2])[0] != RRQ: continue
    parts = pkt[2:].split(b"\0")
    fname = parts[0].decode(errors="replace")
    fields = [p.decode(errors="replace") for p in parts[1:] if p != b""]
    mode = fields[0] if fields else "octet"
    opts = {}
    it = iter(fields[1:])
    for k in it:
        try: opts[k.lower()] = next(it)
        except StopIteration: break
    path = os.path.abspath(os.path.join(ROOT, os.path.basename(fname)))
    print(f"RRQ {fname} mode={mode} opts={opts} from {cli}", flush=True)
    if not path.startswith(ROOT) or not os.path.isfile(path):
        s2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s2.sendto(struct.pack("!HH", ERR, 1)+b"not found\0", cli); s2.close()
        print("  -> not found", flush=True); continue
    send_file(path, cli, opts)
