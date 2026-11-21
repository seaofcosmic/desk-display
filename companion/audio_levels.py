"""Sound levels for the display's music visualizer: 16 bars, lowest notes first, each 0..255.

Windows: listens to whatever the computer is playing through its speakers or headphones
(the PyAudioWPatch package records the sound on its way out; no microphone is used).
It hears everything the computer plays, not only the music app.

Mac, or Windows without that package: the Mac has no built-in way for an app to hear the
computer's own sound, so the bars follow a made-up pattern that moves in time but does not
match the song. The log line at start-up says which of the two is in use.
"""
from __future__ import annotations

import math
import sys
import threading
import time

BARS = 16
# Shaped after cava, the visualizer most others copy: 50 Hz to 10 kHz spread evenly by pitch,
# bass on the left, mids (where voices sit) in the middle, treble on the right.
LOW_HZ, HIGH_HZ = 50.0, 10000.0
FFT_SIZE = 4096                 # long window: used only for the bass bars, so they get real detail
FFT_FAST = 1024                 # short window (about 20 ms) for everything above the bass, so the bars keep time
BASS_SPLIT_HZ = 300.0           # bars below this use the long window, the rest the short one
RANGE_DB = 34.0                 # loudness range from an empty bar to a full one (smaller = more sensitive)
TILT_DB_PER_OCTAVE = 1.0        # a slight lift for the treble, which is usually quieter than the rest of a song
SPREAD = 1.8                    # "Monstercat" smoothing: each bar lifts its neighbors a little (1 = none)


def band_edges(rate: int, size: int = FFT_SIZE):
    """The FFT bin where each bar starts and ends: 16 bands spaced evenly by pitch."""
    top = min(HIGH_HZ, rate / 2 * 0.95)
    edges = []
    last = 0
    for i in range(BARS + 1):
        hz = LOW_HZ * (top / LOW_HZ) ** (i / BARS)
        b = max(int(round(hz * size / rate)), last + (1 if i else 0))
        edges.append(b)
        last = b
    return edges


class Spectrum:
    """Turns chunks of sound into bar heights, 0..255.

    Like cava: total energy per band, a tilt so treble isn't always tiny, a loudness (dB) scale,
    automatic sensitivity that backs off when bars hit the top and creeps back up when the music
    is quiet, and a light spread between neighboring bars so they read as one shape.
    """

    def __init__(self, rate: int):
        import numpy as np
        self.np = np
        self.rate = rate
        self.window = np.hanning(FFT_SIZE)
        self.window_fast = np.hanning(FFT_FAST)
        self.edges = band_edges(rate)
        self.edges_fast = band_edges(rate, FFT_FAST)
        self.slow_bars = sum(1 for i in range(BARS) if self.edges[i + 1] * rate / FFT_SIZE <= BASS_SPLIT_HZ)
        centers = [((self.edges[i] + self.edges[i + 1]) / 2) * rate / FFT_SIZE for i in range(BARS)]
        self.tilt_db = np.array([TILT_DB_PER_OCTAVE * np.log2(max(c, 1.0) / 1000.0) for c in centers])
        self.ref_db = -20.0           # the loudness that fills a bar; adjusted as the music plays

    def bars(self, mono) -> bytes:
        np = self.np
        if len(mono) < FFT_SIZE:
            mono = np.concatenate([np.zeros(FFT_SIZE - len(mono)), mono])
        power = np.abs(np.fft.rfft(mono[-FFT_SIZE:] * self.window)) ** 2 / (FFT_SIZE * FFT_SIZE / 16)
        fast = np.abs(np.fft.rfft(mono[-FFT_FAST:] * self.window_fast)) ** 2 / (FFT_FAST * FFT_FAST / 16)
        e, ef = self.edges, self.edges_fast
        band = np.array([power[e[i]:e[i + 1]].sum() if i < self.slow_bars
                         else fast[ef[i]:max(ef[i + 1], ef[i] + 1)].sum() for i in range(BARS)])
        if band.max() < 1e-9:                                  # silence
            return bytes(BARS)
        db = 10 * np.log10(band + 1e-12) + self.tilt_db
        level = (db - (self.ref_db - RANGE_DB)) / RANGE_DB    # 0 = empty, 1 = full
        # automatic sensitivity: back off quickly when bars hit the top, recover slowly
        top = level.max()
        if top > 1.2:
            self.ref_db += min(0.8, (top - 1.2) * RANGE_DB * 0.25)   # back off gently: one loud hit can't empty the bars
        elif top < 0.95:
            self.ref_db -= 0.8                                        # and climb back quickly when it's quiet
        self.ref_db = max(-90.0, min(20.0, self.ref_db))
        level = np.clip(level, 0.0, 1.0)
        if SPREAD > 1.0:                                       # neighbors lift each other a little
            spread = level.copy()
            for i in range(BARS):
                for j in range(BARS):
                    if i != j:
                        spread[i] = max(spread[i], level[j] / SPREAD ** abs(i - j))
            level = spread
        return bytes(int(v * 255) for v in level)


class SpeakerListener:
    """Windows: records what the speakers are playing, in the background."""

    def __init__(self):
        import numpy as np
        import pyaudiowpatch as pyaudio          # Windows only
        self.np, self.pyaudio = np, pyaudio
        self.lock = threading.Lock()
        self.samples = np.zeros(FFT_SIZE)
        self.last_data = 0.0
        self.pa = None
        self.stream = None
        self.spectrum = None
        self.opened_at = 0.0
        self._open()

    def _open(self):
        self.close()
        pa = self.pyaudio.PyAudio()
        try:
            dev = pa.get_default_wasapi_loopback()
            self.channels = max(1, int(dev["maxInputChannels"]))
            rate = int(dev["defaultSampleRate"])
            self.spectrum = Spectrum(rate)
            self.stream = pa.open(format=self.pyaudio.paInt16, channels=self.channels, rate=rate, input=True,
                                  input_device_index=dev["index"], frames_per_buffer=1024,
                                  stream_callback=self._on_sound)
        except Exception:
            pa.terminate()
            raise
        self.pa = pa
        self.opened_at = time.monotonic()

    def _on_sound(self, data, frame_count, time_info, status):
        np = self.np
        chunk = np.frombuffer(data, dtype=np.int16).astype(np.float64) / 32768.0
        if self.channels > 1:
            chunk = chunk[: len(chunk) // self.channels * self.channels].reshape(-1, self.channels).mean(axis=1)
        with self.lock:
            self.samples = np.concatenate([self.samples, chunk])[-FFT_SIZE:]
            self.last_data = time.monotonic()
        return (None, self.pyaudio.paContinue)

    def read(self) -> bytes:
        now = time.monotonic()
        with self.lock:
            samples, age = self.samples.copy(), now - self.last_data
        if age > 0.4:                                # Windows sends nothing while nothing is playing
            if age > 5.0 and now - self.opened_at > 15.0:
                try:                                 # the sound may have moved to other speakers: start again
                    self._open()
                except Exception:
                    self.opened_at = now
            return bytes(BARS)
        return self.spectrum.bars(samples)

    def close(self):
        try:
            if self.stream:
                self.stream.stop_stream()
                self.stream.close()
            if self.pa:
                self.pa.terminate()
        except Exception:
            pass
        self.stream = self.pa = None


def pattern(t: float) -> bytes:
    """A made-up pattern that moves like music: a beat in the low bars, ripples in the high ones."""
    beat = (t * 2.0) % 1.0                           # two beats a second
    kick = max(0.0, 1.0 - beat * 3.5)
    out = []
    for i in range(BARS):
        x = i / (BARS - 1)
        wave = 0.5 + 0.5 * math.sin(t * (2.1 + i * 0.37) + i * 1.7)
        slow = 0.5 + 0.5 * math.sin(t * 0.6 + i * 0.5)
        v = (1.0 - x) * (0.25 + 0.75 * kick) + x * 0.55 * wave * (0.4 + 0.6 * slow) + 0.12 * wave
        out.append(int(max(0.0, min(1.0, v)) * 255))
    return bytes(out)


class AppListener:
    """Windows 10 (2004 or later) and 11: hears one app only, such as Apple Music, through the
    proc-tap package. Other apps' sound (browser tabs, games, calls) never reaches the bars."""

    def __init__(self, app: str):
        import numpy as np
        import proctap        # noqa: F401  (Windows only; installed from requirements-visualizer.txt)
        import psutil
        self.np, self.proctap, self.psutil = np, proctap, psutil
        self.app = app.lower().replace(" ", "")
        # Processes that may play Apple Music's sound, best guess first. A process that stays silent
        # while a song plays is skipped for the next one.
        self.names = [self.app] + [n for n in ("applemusic", "amplibraryagent", "itunes") if n != self.app]
        self.tried = set()
        self.log = print
        self.lock = threading.Lock()
        self.samples = np.zeros(FFT_SIZE)
        self.last_data = 0.0
        self.cap = None
        self.pid = None
        self.next_look = 0.0
        self.spectrum = Spectrum(48000)       # proc-tap always delivers 48 kHz stereo

    def _candidates(self):
        """(pid, name) of every process that might be playing Apple Music, best guess first."""
        found = []
        for p in self.psutil.process_iter(["pid", "name", "ppid"]):
            name = (p.info.get("name") or "").lower().replace(" ", "")
            for rank, want in enumerate(self.names):
                if want in name:
                    try:
                        parent = (self.psutil.Process(p.info["ppid"]).name() or "").lower().replace(" ", "")
                    except Exception:
                        parent = ""
                    top = 0 if want not in parent else 1        # the app's main process before its helpers
                    found.append((rank, top, p.info["pid"], p.info.get("name") or name))
                    break
        found.sort()
        return [(pid, name) for _, _, pid, name in found]

    def _find_pid(self):
        for pid, name in self._candidates():
            if pid not in self.tried:
                self.name = name
                return pid
        return None

    def skip_current(self) -> bool:
        """The process being listened to stayed silent during a song: move on to the next one.
        Returns False when there's nothing left to try."""
        if self.pid is not None:
            self.tried.add(self.pid)
        self.close()
        self.next_look = 0.0
        left = [pid for pid, _ in self._candidates() if pid not in self.tried]
        if not left:
            self.tried.clear()           # start over later (Apple Music may have restarted)
            self.next_look = time.monotonic() + 30.0
            return False
        return True

    def _on_sound(self, data, frames):
        np = self.np
        chunk = np.frombuffer(data, dtype=np.float32)
        chunk = chunk[: len(chunk) // 2 * 2].reshape(-1, 2).mean(axis=1).astype(np.float64)
        with self.lock:
            self.samples = np.concatenate([self.samples, chunk])[-FFT_SIZE:]
            self.last_data = time.monotonic()

    def close(self):
        try:
            if self.cap:
                self.cap.close()
        except Exception:
            pass
        self.cap, self.pid = None, None

    def read(self) -> bytes:
        now = time.monotonic()
        if now >= self.next_look:            # every few seconds: is the app still the same process?
            self.next_look = now + 3.0
            if self.pid is not None and not self.psutil.pid_exists(self.pid):
                self.close()
            if self.cap is None:
                pid = self._find_pid()
                if pid is not None:
                    cap = self.proctap.ProcessAudioCapture(pid, on_data=self._on_sound)
                    cap.start()
                    self.cap, self.pid = cap, pid
                    self.log(f"Visualizer: listening to {self.name} (process {pid}).")
        with self.lock:
            samples, age = self.samples.copy(), now - self.last_data
        if self.cap is None or age > 0.5:
            return bytes(BARS)
        return self.spectrum.bars(samples)


class Levels:
    """What the companion uses: read() gives the 16 bar heights for this moment.

    With app="applemusic" (the companion's default on Windows) the bars hear only that app.
    If that can't be done on this computer, they fall back to everything the computer plays.
    """

    def __init__(self, log=print, verbose: bool = False, pretend: bool = False, app: str = None):
        self.log, self.verbose = log, verbose
        self.listener = None
        self.real = False
        self.app_only = False
        self.silent_since = None
        self.heard_app = False             # has the one-app capture ever heard anything?
        self.still = False                 # Apple Music only, but it can't be heard: keep the bars still
        if pretend:
            log("Visualizer: demo mode, the bars follow a made-up pattern.")
            return
        if sys.platform == "win32":
            if app:
                try:
                    self.listener = AppListener(app)
                    self.listener.log = log
                    self.real = self.app_only = True
                    log("Visualizer: following only the sound from Apple Music." if "applemusic" in app.lower()
                        else f"Visualizer: following only the sound from {app}.")
                    return
                except Exception as e:
                    # Never fall back to the whole computer's sound when only Apple Music was asked for.
                    log("Visualizer: can't listen to Apple Music by itself on this computer, so the bars stay still.",
                        f"({type(e).__name__}: {e})")
                    log("  If this says proctap is missing: close this, run start_windows.bat once and wait for it to finish.")
                    self.still = True
                    return
            try:
                self.listener = SpeakerListener()
                self.real = True
                log("Visualizer: following the sound this computer is playing.")
                return
            except Exception as e:
                if verbose:
                    log("visualizer error:", repr(e))
                log("Visualizer: could not listen to this computer's sound, so the bars follow a made-up pattern.")
                return
        log("Visualizer: this computer's sound cannot be read here, so the bars follow a made-up pattern.")

    def read(self, playing: bool) -> bytes:
        if not playing or self.still:
            self.silent_since = None
            return bytes(BARS)
        if self.listener:
            try:
                levels = self.listener.read()
            except Exception as e:
                if self.verbose:
                    self.log("visualizer error:", repr(e))
                levels = bytes(BARS)
            if self.app_only:
                # Playing, yet nothing heard from this Apple process for a while: it isn't the one
                # making the sound. Try the next one. Never fall back to the rest of the computer's sound.
                now = time.monotonic()
                if any(levels):
                    self.silent_since = None
                    if not self.heard_app:
                        self.heard_app = True
                        self.log("Visualizer: hearing Apple Music.")
                elif self.silent_since is None:
                    self.silent_since = now
                elif now - self.silent_since > 8.0:
                    self.silent_since = None
                    if self.heard_app:
                        pass                    # it worked before: just a quiet part of the song
                    elif not self.listener.skip_current():
                        self.log("Visualizer: could not hear Apple Music by itself, so the bars stay still "
                                 "(other apps' sound is never used). Run start_windows.bat --verbose and "
                                 "send the Visualizer lines.")
            return levels
        return pattern(time.monotonic())
