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
LOW_HZ, HIGH_HZ = 45.0, 12000.0
FFT_SIZE = 2048


def band_edges(rate: int, size: int = FFT_SIZE):
    """The FFT bin where each bar starts and ends: 16 bands spaced evenly by pitch."""
    top = min(HIGH_HZ, rate / 2 * 0.95)
    edges = []
    last = 1
    for i in range(BARS + 1):
        hz = LOW_HZ * (top / LOW_HZ) ** (i / BARS)
        b = max(int(round(hz * size / rate)), last + (1 if i else 0))
        edges.append(b)
        last = b
    return edges


class Spectrum:
    """Turns chunks of sound into bar heights. The loudest recent sound sets the top of the scale."""

    def __init__(self, rate: int):
        import numpy as np
        self.np = np
        self.rate = rate
        self.window = np.hanning(FFT_SIZE)
        self.edges = band_edges(rate)
        self.peak = 1e-4            # recent loudest band, decays slowly so quiet songs still move

    def bars(self, mono) -> bytes:
        np = self.np
        if len(mono) < FFT_SIZE:
            mono = np.concatenate([np.zeros(FFT_SIZE - len(mono)), mono])
        spec = np.abs(np.fft.rfft(mono[-FFT_SIZE:] * self.window)) / (FFT_SIZE / 4)
        levels = []
        for i in range(BARS):
            a, b = self.edges[i], self.edges[i + 1]
            band = spec[a:b]
            v = float(np.sqrt(np.mean(band * band))) if len(band) else 0.0
            levels.append(v * (1.0 + i * 0.35))            # high notes are quieter; lift them
        loudest = max(levels)
        self.peak = max(loudest, self.peak * 0.995, 1e-4)
        if loudest < 2e-4:                                  # silence
            return bytes(BARS)
        out = []
        for v in levels:
            db = 20 * math.log10(max(v / self.peak, 1e-6))  # 0 dB = as loud as the recent peak
            out.append(int(max(0.0, min(1.0, (db + 36.0) / 36.0)) * 255))
        return bytes(out)


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


class Levels:
    """What the companion uses: read() gives the 16 bar heights for this moment."""

    def __init__(self, log=print, verbose: bool = False, pretend: bool = False):
        self.listener = None
        self.real = False
        if pretend:
            log("Visualizer: demo mode, the bars follow a made-up pattern.")
            return
        if sys.platform == "win32":
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
        if not playing:
            return bytes(BARS)
        if self.listener:
            try:
                return self.listener.read()
            except Exception:
                return bytes(BARS)
        return pattern(time.monotonic())
