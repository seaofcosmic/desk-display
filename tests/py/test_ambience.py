import sys, types, time, datetime, math
sys.path.insert(0, '/home/claude/desk-display/companion')
import numpy as np
import extras, audio_levels

# ---- sun line
cur = {"is_day": 1}
assert extras.sun_line(cur, {"sunrise": ["2026-11-16T06:27"], "sunset": ["2026-11-16T16:52"]}) == b"D|387|1012"
assert extras.sun_line({"is_day": 0}, {"sunrise": ["2026-11-16T06:27"], "sunset": ["2026-11-16T16:52"]}) == b"N|387|1012"
assert extras.sun_line(cur, {}) == b"D"
assert extras.sun_line(cur, {"sunrise": [None], "sunset": ["x"]}) == b"D"
assert extras.sun_line(cur, {"sunrise": [], "sunset": []}) == b"D"
assert extras.sun_line(cur, {"sunrise": ["2026-11-16T18:00"], "sunset": ["2026-11-16T06:00"]}) == b"D"   # polar oddity: ignored

# ---- the whole payload, from a response shaped like Open-Meteo's
fc = {"current": {"temperature_2m": 17.6, "relative_humidity_2m": 58, "apparent_temperature": 16.9, "weather_code": 1,
                  "wind_speed_10m": 9.4, "is_day": 1},
      "daily": {"time": ["2026-11-16", "2026-11-17", "2026-11-18"], "weather_code": [1, 61, 0],
                "temperature_2m_max": [22.1, 18.0, 23.4], "temperature_2m_min": [12.9, 11.2, 13.0],
                "sunrise": ["2026-11-16T06:27", "2026-11-17T06:28", "2026-11-18T06:29"],
                "sunset": ["2026-11-16T16:52", "2026-11-17T16:51", "2026-11-18T16:51"]}}
to_ascii = lambda s, n: s.encode("ascii", "replace")[:n]
pl = extras.weather_payload(("Culver City", 34, -118), fc, False, False, to_ascii, datetime.datetime(2026, 11, 16, 16, 50)).split(b"\n")
assert pl[12] == b"D|387|1012", pl[12]
assert pl[13].startswith(b"TODAY|"), pl[13]
print("payload:", pl[:14])

# ---- demo skies: the device's own rule, in Python, must give the wanted time of day at any minute
def phase(m, rise, sset, glow=35):
    if abs(m - rise) <= glow: return "dawn"
    if abs(m - sset) <= glow: return "dusk"
    return "day" if rise < m < sset else "night"
bad = 0
for now in range(1440):
    for step, (code, want) in enumerate(extras.DEMO_SKIES):
        f = extras.demo_forecast(step, datetime.date(2026, 11, 16), now)
        line = extras.sun_line(f["current"], f["daily"]).decode().split("|")
        if len(line) != 3: got = "night" if line[0] == "N" else "day"
        else: got = phase(now, int(line[1]), int(line[2]))
        if got != want:
            bad += 1
            if bad < 6: print("demo mismatch", now, want, got, line)
print("demo sky mismatches:", bad, "of", 1440 * len(extras.DEMO_SKIES))

# ---- spectrum: a low tone lights the low bars, a high tone the high bars, silence nothing
rate = 48000
edges = audio_levels.band_edges(rate)
assert len(edges) == 17 and all(b > a for a, b in zip(edges, edges[1:])), edges
print("band edges (Hz):", [round(e * rate / audio_levels.FFT_SIZE) for e in edges])
sp = audio_levels.Spectrum(rate)
t = np.arange(8192) / rate
def peak_bar(hz, amp=0.3):
    sp = audio_levels.Spectrum(rate)
    for _ in range(60): bars = sp.bars(amp * np.sin(2 * np.pi * hz * t))
    return max(range(16), key=lambda i: bars[i]), list(bars)
for hz in (60, 200, 1000, 4000, 10000):
    i, bars = peak_bar(hz)
    lo, hi = edges[i] * rate / audio_levels.FFT_SIZE, edges[i + 1] * rate / audio_levels.FFT_SIZE
    print(f"{hz} Hz -> bar {i} ({lo:.0f}-{hi:.0f} Hz), heights {bars}")
    assert lo * 0.8 <= hz <= hi * 1.25, (hz, lo, hi)
assert sp.bars(np.zeros(4096)) == bytes(16)
assert sp.bars(np.zeros(100)) == bytes(16)
# a quiet song still fills the scale; then a loud one does not clip
sp2 = audio_levels.Spectrum(rate)
for _ in range(100): q = sp2.bars(0.01 * np.sin(2 * np.pi * 440 * t))
for _ in range(30): l = sp2.bars(0.9 * np.sin(2 * np.pi * 440 * t))
print("quiet max", max(q), "loud max", max(l)); assert max(q) > 200 and max(l) > 200
# mixed music-like signal: several bars above zero
mix = sum(a * np.sin(2 * np.pi * f * t) for f, a in ((80, .5), (250, .3), (900, .2), (3000, .1), (8000, .05)))
print("mix:", list(audio_levels.Spectrum(rate).bars(mix)))

# ---- pattern: 16 bytes, moves, stays in range
a, b = audio_levels.pattern(1.0), audio_levels.pattern(1.07)
assert len(a) == 16 and a != b
lv = audio_levels.Levels(log=lambda *x: print("log:", *x), pretend=True)
assert lv.read(False) == bytes(16) and len(lv.read(True)) == 16 and any(lv.read(True))
lv = audio_levels.Levels(log=lambda *x: print("log:", *x))          # on Linux: falls back to the pattern
assert not lv.real and any(lv.read(True))

# ---- the Windows listener, against a stand-in for the PyAudioWPatch package
class FakeStream:
    def __init__(self, cb): self.cb = cb; self.stopped = False
    def stop_stream(self): self.stopped = True
    def close(self): pass
class FakePA:
    made = []
    def __init__(self): FakePA.made.append(self); self.terminated = False
    def get_default_wasapi_loopback(self):
        return {"index": 7, "maxInputChannels": 2, "defaultSampleRate": 48000.0, "name": "Speakers [Loopback]", "isLoopbackDevice": True}
    def open(self, **kw):
        assert kw["input"] and kw["input_device_index"] == 7 and kw["channels"] == 2 and kw["rate"] == 48000
        self.stream = FakeStream(kw["stream_callback"]); return self.stream
    def terminate(self): self.terminated = True
fake = types.ModuleType("pyaudiowpatch"); fake.PyAudio = FakePA; fake.paInt16 = 8; fake.paContinue = 0
sys.modules["pyaudiowpatch"] = fake
ls = audio_levels.SpeakerListener()
assert ls.read() == bytes(16)                               # nothing heard yet
tone = (0.4 * np.sin(2 * np.pi * 1000 * np.arange(1024 * 8) / 48000) * 32767).astype(np.int16)
stereo = np.repeat(tone, 2)                                 # left and right the same
for k in range(8):
    ret = ls.stream.cb(stereo[k * 2048:(k + 1) * 2048].tobytes(), 1024, None, 0)
    assert ret == (None, 0)
for _ in range(40): bars = ls.read()
i = max(range(16), key=lambda j: bars[j])
print("listener heard 1 kHz in bar", i, list(bars)); assert bars[i] > 200 and edges[i] * 48000 / audio_levels.FFT_SIZE * 0.8 <= 1000 <= edges[i + 1] * 48000 / audio_levels.FFT_SIZE * 1.25
ls.last_data -= 1.0; assert ls.read() == bytes(16)          # sound stopped
ls.last_data -= 10; ls.opened_at -= 20; n = len(FakePA.made); ls.read(); assert len(FakePA.made) == n + 1 and FakePA.made[0].terminated   # reopened
sys_platform = sys.platform; sys.platform = "win32"
lv = audio_levels.Levels(log=lambda *x: print("log:", *x)); assert lv.real
def boom(self): raise OSError("no WASAPI")
FakePA.get_default_wasapi_loopback = boom
lv = audio_levels.Levels(log=lambda *x: print("log:", *x)); assert not lv.real and any(lv.read(True))
sys.platform = sys_platform
print("ALL OK")
