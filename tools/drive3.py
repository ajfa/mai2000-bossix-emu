import os, pty, sys, time, select

# drive3.py <total secs> <break at secs> -- emu args...
# RESP env: 'PROMPT=>REPLY;...'   BRKCMD env: 'cmd1;cmd2;...'
secs = float(sys.argv[1])
brk = float(sys.argv[2])
args = sys.argv[3:]

resp = []
for pair in os.environ.get("RESP", "").split(";"):
    if "=>" in pair:
        pat, rep = pair.split("=>", 1)
        resp.append([pat, rep.replace("\\r", "\r"), 0])
brkcmds = [c for c in os.environ.get("BRKCMD", "").split(";") if c]

pid, fd = pty.fork()
if pid == 0:
    os.chdir(os.path.expanduser("~/mai/emu2000"))
    os.execv("./eagleemu", ["./eagleemu"] + args)

out = b""
tail = b""
t0 = time.time()
broke = False
while time.time() - t0 < secs:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        try:
            d = os.read(fd, 65536)
        except OSError:
            break
        if not d:
            break
        out += d
        tail = (tail + d)[-4000:]
        for item in resp:
            if item[2]:
                continue
            if item[0].encode("latin-1") in tail:
                time.sleep(0.4)
                os.write(fd, item[1].encode("latin-1"))
                item[2] = 1
                tail = b""
                break
    if (not broke) and (time.time() - t0 > brk):
        broke = True
        os.write(fd, b"\x18")
        time.sleep(1.0)
        for c in brkcmds:
            os.write(fd, (c + "\r").encode("latin-1"))
            time.sleep(1.2)
sys.stdout.write(out.decode("latin-1"))
