import sys, time, threading
sys.path.insert(0, '/home/claude/desk-display/companion')
import media_worker as mw
from nowplaying import NowPlaying
mw.POLL_STUCK_AFTER = 1.5; mw.ART_STUCK_AFTER = 1.0
logs = []
def log(*a): logs.append(" ".join(map(str, a))); print("  log:", *a)

class Fake:
    made = 0
    hang_art = True; hang_poll_once = False; cmds = []
    def __init__(self):
        Fake.made += 1; self.n = Fake.made
    def poll(self):
        if Fake.hang_poll_once and self.n == 1 and threading.current_thread().name.startswith("media-poll"):
            Fake.hang_poll_once = False; time.sleep(3600)
        return NowPlaying(title="Death", artist="C418", album="Alpha", playing=True, position_ms=1000, duration_ms=100000, can_seek=True)
    def get_art(self, np):
        if Fake.hang_art: time.sleep(3600)
        return b"PNGDATA"
    def play_pause(self): Fake.cmds.append("pp")
    def next(self): Fake.cmds.append("next")
    def previous(self): pass
    def seek(self, ms): Fake.cmds.append(("seek", ms))

w = mw.MediaWorker(Fake, log=log)
time.sleep(0.3)
np = w.poll(); assert np and np.title == "Death", np
# 1. a cover request that never returns must not block, and is reported and replaced
t = time.monotonic(); assert w.get_art(np) is None; assert time.monotonic() - t < 0.05
time.sleep(1.3); assert w.get_art(np) is None
assert any("cover" in l for l in logs), logs
# main keeps polling quickly meanwhile
t = time.monotonic(); [w.poll() for _ in range(100)]; assert time.monotonic() - t < 0.1
# 2. commands go through without waiting
w.play_pause(); w.next(); w.seek(5000); time.sleep(0.3)
assert Fake.cmds == ["pp", "next", ("seek", 5000)], Fake.cmds
# 3. a new song with a working cover arrives
Fake.hang_art = False
np2 = NowPlaying(title="Aria Math", artist="C418", album="Beta", playing=True, position_ms=0, duration_ms=1, can_seek=True)
w.get_art(np2); time.sleep(2.0); w.get_art(np2); time.sleep(0.3); assert w.get_art(np2) == b"PNGDATA"
print("art + commands OK")
# 4. the poll thread hangs: it is replaced and polling resumes
Fake.hang_poll_once = True; Fake.made = 0
w2 = mw.MediaWorker(Fake, log=log)
t = time.monotonic()
while time.monotonic() - t < 4:
    w2.poll(); time.sleep(0.05)
assert any("stopped answering" in l for l in logs), logs
assert w2.poll() is not None and w2.poll().title == "Death"
print("poll recovery OK")
# 5. a factory that fails reports the error to the caller
def bad(): raise ImportError("winrt missing")
try: mw.MediaWorker(bad, log=log); print("NO ERROR?!")
except ImportError as e: print("factory error passed on:", e)
print("ALL OK")
