import os, pty, sys, time, select

# drive5.py <seconds> -- emu args...
# RESP env: repeatable prompt=>reply pairs, as drive4.
# SEQ env: prompt=>reply pairs consumed strictly IN ORDER, one per appearance,
# checked before RESP. This is how a conversation with a shell is scripted:
# every entry keyed on the same prompt fires on successive appearances.
secs = float(sys.argv[1])
args = sys.argv[2:]

def decode(rep):
    return rep.replace("\\r", "\r").replace("\\n", "\n").replace("\\e", chr(27)).replace("\030", chr(24))

resp = []
for pair in os.environ.get("RESP", "").split(";"):
    if "=>" in pair:
        pat, rep = pair.split("=>", 1)
        resp.append((pat.encode("latin-1"), decode(rep).encode("latin-1")))

seq = []
for pair in os.environ.get("SEQ", "").split(";"):
    if "=>" in pair:
        pat, rep = pair.split("=>", 1)
        seq.append((pat.encode("latin-1"), decode(rep).encode("latin-1")))
seqIdx = 0

pid, fd = pty.fork()
if pid == 0:
    os.chdir(os.path.expanduser("~/mai/emu2000"))
    os.execv("./eagleemu", ["./eagleemu"] + args)

out = b""
tail = b""
t0 = time.time()
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
        fired = False
        if seqIdx < len(seq) and seq[seqIdx][0] in tail:
            time.sleep(0.6)
            os.write(fd, seq[seqIdx][1])
            seqIdx += 1
            tail = b""
            fired = True
        if not fired:
            for pat, rep in resp:
                if pat in tail:
                    time.sleep(0.5)
                    os.write(fd, rep)
                    tail = b""
                    break
sys.stdout.write(out.decode("latin-1"))
