import os, pty, sys, time, select, re

# usage: drive2.py <seconds> -- <emu args...>  with responses given as
# PROMPT=>REPLY pairs in the RESP environment variable, separated by ';'
secs = float(sys.argv[1])
args = sys.argv[2:]
resp = []
for pair in os.environ.get("RESP", "").split(";"):
    if "=>" in pair:
        pat, rep = pair.split("=>", 1)
        rep = rep.replace("\e", chr(27))
        resp.append([pat, rep.replace("\\r", "\r").replace("\\n", "\n"), 0])

pid, fd = pty.fork()
if pid == 0:
    os.chdir(os.path.expanduser("~/mai/emu2000"))
    os.execv("./eagleemu", ["./eagleemu"] + args)

out = b""
tail = b""
t0 = time.time()
while time.time() - t0 < secs:
    r, _, _ = select.select([fd], [], [], 1.0)
    if r:
        try:
            d = os.read(fd, 65536)
        except OSError:
            break
        if not d:
            break
        out += d
        tail += d
        if len(tail) > 4000:
            tail = tail[-4000:]
        for item in resp:
            pat, rep, used = item
            if used:
                continue
            if pat.encode("latin-1") in tail:
                time.sleep(0.4)
                os.write(fd, rep.encode("latin-1"))
                item[2] = 1
                tail = b""
                break
sys.stdout.write(out.decode("latin-1"))
