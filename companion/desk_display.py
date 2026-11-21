#!/usr/bin/env python3
"""Desk Display companion app.

Reads what is playing on this computer and sends it to the Cheap Yellow Display
over the USB cable. Button taps and scrubs on the display come back the same way
and control the music app.

    Windows: follows whatever Windows shows in its media controls (Apple Music,
             Spotify, a browser tab, ...).
    Mac:     follows the Music app (Apple Music).

It also keeps the display's clock set and sends it the weather for the city in
settings.json, the list in reminders.txt, upcoming items from a Notion database,
and this computer's CPU, memory, disk and temperature readings.

Run:   python desk_display.py            find the display and start
       python desk_display.py --demo     show fake tracks, weather and calendar, to test the display
       python desk_display.py --port COM5
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import io
import json
import os
import queue
import struct
import sys
import subprocess
import threading
import time
import unicodedata
import urllib.parse
import urllib.request
from typing import Optional

import audio_levels
import lyrics
import extras
import notion_calendar
import pc_stats
from nowplaying import NowPlaying

ART_SIZE = 140
ART_BYTES = ART_SIZE * ART_SIZE * 2
BAUD = 460800

STATE_PLAYING, STATE_HAS_POS, STATE_CAN_SEEK = 0x01, 0x02, 0x04


# --------------------------------------------------------------------------- protocol
def frame(kind: bytes, payload: bytes = b"") -> bytes:
    """'D' 'T' type, 4-byte length, payload, 2-byte checksum. Matches protocol.h."""
    return b"DT" + kind + struct.pack("<I", len(payload)) + payload + struct.pack("<H", sum(payload) & 0xFFFF)


_PUNCT = {
    "‘": "'", "’": "'", "“": '"', "”": '"', "–": "-", "—": "-",
    "…": "...", "•": "*", "×": "x", "ß": "ss", "æ": "ae", "Æ": "AE",
    "ø": "o", "Ø": "O", "œ": "oe", "Œ": "OE", " ": " ",
}


def to_ascii(text: str, limit: int = 96) -> bytes:
    """The display's fonts only have basic Latin letters, so fold accents and swap the rest for '?'."""
    out = []
    for ch in text or "":
        ch = _PUNCT.get(ch, ch)
        for c in unicodedata.normalize("NFKD", ch):
            if unicodedata.combining(c):
                continue
            out.append(c if 32 <= ord(c) < 127 else "?")
    return " ".join("".join(out).split())[:limit].encode("ascii")


def encode_meta(np: NowPlaying) -> bytes:
    return frame(b"M", b"\n".join([to_ascii(np.title), to_ascii(np.artist), to_ascii(np.album)]))


def encode_state(np: NowPlaying) -> bytes:
    has_pos = np.position_ms is not None and np.duration_ms > 0
    flags = (STATE_PLAYING if np.playing else 0) | (STATE_HAS_POS if has_pos else 0) | \
            (STATE_CAN_SEEK if (np.can_seek and has_pos) else 0)
    pos = max(0, min(int(np.position_ms or 0), 0xFFFFFFFF))
    dur = max(0, min(int(np.duration_ms or 0), 0xFFFFFFFF))
    return frame(b"S", struct.pack("<BII", flags, pos, dur))


def encode_art(image_bytes: bytes) -> bytes:
    """Any image file -> 140 x 140 RGB565, high byte first."""
    from PIL import Image, ImageOps
    img = Image.open(io.BytesIO(image_bytes)).convert("RGB")
    img = ImageOps.fit(img, (ART_SIZE, ART_SIZE), Image.LANCZOS)
    rgb = img.tobytes()
    out = bytearray(ART_BYTES)
    j = 0
    for i in range(0, len(rgb), 3):
        v = ((rgb[i] & 0xF8) << 8) | ((rgb[i + 1] & 0xFC) << 3) | (rgb[i + 2] >> 3)
        out[j] = v >> 8
        out[j + 1] = v & 0xFF
        j += 2
    return frame(b"A", bytes(out))


# --------------------------------------------------------------------------- online art fallback
def lookup_art_online(np: NowPlaying, timeout: float = 5.0) -> Optional[bytes]:
    """Ask Apple's public iTunes Search for the cover. Used only when the music app gives no picture."""
    term = " ".join(x for x in (np.artist, np.title) if x)
    if not term:
        return None
    url = "https://itunes.apple.com/search?" + urllib.parse.urlencode({"term": term, "entity": "song", "limit": 5})
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            results = json.load(r).get("results", [])
        if not results:
            return None
        want = (np.album or "").lower()
        best = next((x for x in results if want and x.get("collectionName", "").lower() == want), results[0])
        art_url = best.get("artworkUrl100")
        if not art_url:
            return None
        art_url = art_url.replace("100x100", "300x300")
        with urllib.request.urlopen(art_url, timeout=timeout) as r:
            return r.read()
    except Exception:
        return None


# --------------------------------------------------------------------------- demo source
class DemoMedia:
    """Fake tracks, for testing the display without any music app."""
    TRACKS = [
        ("Fury Road", "The Wasteland Radio", "Guzzoline Sessions", 214),
        ("A Very Long Song Title That Needs To Wrap Across Several Lines", "Artist With A Long Name Too", "Deluxe Edition", 372),
        ("Chrome", "War Rig", "Witness", 95),
    ]

    def __init__(self):
        self.i, self.playing, self.pos, self.t = 0, True, 0.0, time.monotonic()

    def _tick(self):
        now = time.monotonic()
        if self.playing:
            self.pos += now - self.t
        self.t = now
        if self.pos >= self.TRACKS[self.i][3]:
            self.next()

    def poll(self):
        self._tick()
        t = self.TRACKS[self.i]
        return NowPlaying(t[0], t[1], t[2], self.playing, int(self.pos * 1000), t[3] * 1000, True)

    COVERS = [((226, 120, 30), (40, 28, 20)), ((24, 44, 130), (120, 170, 245)), ((36, 12, 44), (214, 44, 150))]

    def get_art(self, np):
        # Each fake cover has one main color, so the "album colors" setting has something to show.
        # The four small squares are red, green, blue, white, left to right: if the display shows
        # them in other colors, the screen-chip setting is wrong.
        from PIL import Image, ImageDraw
        back, front = self.COVERS[self.i % len(self.COVERS)]
        img = Image.new("RGB", (280, 280), back)
        d = ImageDraw.Draw(img)
        d.ellipse([50, 60, 230, 240], fill=front)
        d.ellipse([112, 122, 168, 178], fill=back)
        for k, c in enumerate([(220, 30, 30), (30, 190, 60), (40, 70, 220), (240, 240, 240)]):
            d.rectangle([12 + k * 26, 12, 34 + k * 26, 34], fill=c)
        buf = io.BytesIO()
        img.save(buf, "PNG")
        return buf.getvalue()

    def play_pause(self):
        self._tick(); self.playing = not self.playing

    def next(self):
        self.i = (self.i + 1) % len(self.TRACKS); self.pos = 0.0

    def previous(self):
        if self.pos > 3:
            self.pos = 0.0
        else:
            self.i = (self.i - 1) % len(self.TRACKS); self.pos = 0.0

    def seek(self, ms):
        self._tick(); self.pos = ms / 1000.0


def make_media(args):
    if args.demo:
        return DemoMedia()
    if sys.platform == "win32":
        from media_windows import WindowsMedia
        return WindowsMedia(app_filter=args.app)
    if sys.platform == "darwin":
        from media_mac import MacMusic
        return MacMusic()
    sys.exit("This app supports Windows and macOS. Use --demo to test the display on other systems.")


# --------------------------------------------------------------------------- serial link
class Link:
    def __init__(self, port: str):
        import serial
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = BAUD
        self.ser.timeout = 0.1
        self.ser.write_timeout = 5
        self.ser.dtr = False          # keep these low so opening the port does not hold the board in reset
        self.ser.rts = False
        self.ser.open()
        self.lines: "queue.Queue[str]" = queue.Queue()
        self.error: Optional[Exception] = None
        self._stop = False
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        buf = b""
        while not self._stop:
            try:
                data = self.ser.read(256)
            except Exception as e:      # unplugged
                self.error = e
                return
            if not data:
                continue
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("ascii", "ignore").strip()
                if text.startswith("DT:"):
                    self.lines.put(text)
                elif text:
                    self.lines.put("RAW:" + text)        # anything else the board prints, such as a crash report
            if len(buf) > 4096:
                buf = buf[-256:]

    def send(self, data: bytes):
        if self.error:
            raise self.error
        self.ser.write(data)

    def close(self):
        self._stop = True
        try:
            self.ser.close()
        except Exception:
            pass


def find_port(preferred: Optional[str]) -> Optional[str]:
    from serial.tools import list_ports
    if preferred:
        return preferred
    usb_serial_chips = {0x1A86, 0x10C4, 0x0403}      # CH340 (most of these boards), CP210x, FTDI
    ports = [p for p in list_ports.comports() if p.vid in usb_serial_chips]
    ports.sort(key=lambda p: (p.vid != 0x1A86, p.device))
    return ports[0].device if ports else None


# --------------------------------------------------------------------------- main loop
class Companion:
    ART_RETRY_AT = (0.3, 0.9, 1.6, 2.5, 3.5, 5.0, 7.0, 10.0, 13.0)   # seconds after a track change; covers can arrive late

    def __init__(self, media, args):
        self.media = media
        self.args = args
        self.online_results: "queue.Queue" = queue.Queue()
        self.lyrics = lyrics.LyricsFetcher(self.log, args.verbose)      # for the display's lyric mode

        self.settings = extras.load_settings(args.data_dir, self.log)
        # What the display's Settings app is set to. The display reports these when it connects.
        self.device = {"H24": 0, "WC": 1, "RA": 1, "CA": 1, "VZ": 1}   # the display's settings, until it reports them
        self.reminders = extras.Reminders(os.path.join(args.data_dir, "reminders.txt"), False, to_ascii)
        self.stats = pc_stats.PcStats()
        self.levels = audio_levels.Levels(self.log, args.verbose, pretend=args.demo,
                                          app=getattr(args, "app", None))         # for the visualizer
        try:   # settings.json "visualizer_delay_ms": hold the bars back this long (for Bluetooth headphones)
            self.viz_delay = max(0.0, min(1.0, float(self.settings.get("visualizer_delay_ms", 0)) / 1000.0))
        except (TypeError, ValueError):
            self.viz_delay = 0.0
        self.fired_events = set()

        self.weather = ("err", "Getting the weather.")      # or ("ok", place, forecast, fetched at)
        self.weather_updates: "queue.Queue" = queue.Queue()
        threading.Thread(target=self._weather_loop, daemon=True).start()

        self.calendar = ("err", "Getting your calendar.")   # or ("ok", events)
        self.calendar_updates: "queue.Queue" = queue.Queue()
        threading.Thread(target=self._calendar_loop, daemon=True).start()

    def _weather_loop(self):
        """Runs in the background: fetch now, then every 15 minutes. Retry sooner after a failure."""
        if self.args.demo:
            step, started = 0, datetime.datetime.now()
            while True:                                   # a new made-up sky every few seconds
                self.weather_updates.put(("ok", ("Demo City", 0, 0), extras.demo_forecast(step), started))
                step += 1
                time.sleep(extras.DEMO_SKY_SECONDS)
        has_place = self.settings.get("city") or ("latitude" in self.settings and "longitude" in self.settings)
        if not has_place:
            self.weather_updates.put(("err", "Set your city in settings.json"))
            return
        place, have_data = None, False
        while True:
            try:
                place, forecast = extras.fetch_weather(self.settings, place)
                self.weather_updates.put(("ok", place, forecast, datetime.datetime.now()))
                have_data = True
                wait = 15 * 60
            except LookupError as e:
                self.log(f"Weather: {e}. Check the city in settings.json.")
                self.weather_updates.put(("err", "City not found. Check settings.json"))
                return
            except Exception as e:
                if self.args.verbose:
                    self.log("weather error:", repr(e))
                if not have_data:
                    self.weather_updates.put(("err", "Could not get the weather."))
                wait = 60
            time.sleep(wait)

    def _calendar_loop(self):
        """Runs in the background: read the Notion database now, then every 5 minutes."""
        if self.args.demo:
            self.calendar_updates.put(("ok", notion_calendar.demo_events()))
            return
        token = str(self.settings.get("notion_token") or "").strip()
        database = str(self.settings.get("notion_database_id") or "").strip()
        if not token:
            self.calendar_updates.put(("err", "Add your Notion token to settings.json"))
            return
        if not database:
            self.calendar_updates.put(("err", "Add notion_database_id to settings.json"))
            return
        source_id, columns, have_data = None, None, False
        while True:
            try:
                if source_id is None:
                    source = notion_calendar.find_data_source(token, database)
                    columns = notion_calendar.pick_columns(source, str(self.settings.get("notion_date_property") or ""))
                    source_id = source["id"]
                    self.log(f"Calendar: reading Notion dates from the '{columns[1]}' column.")
                self.calendar_updates.put(("ok", notion_calendar.read_events(token, source_id, columns)))
                have_data = True
                wait = 5 * 60
            except Exception as e:
                message = notion_calendar.explain(e)
                self.log(f"Calendar: {message}.")
                if self.args.verbose:
                    self.log("calendar error:", repr(e))
                if not have_data:
                    self.calendar_updates.put(("err", message))
                wait = 120
            time.sleep(wait)

    def _weather_frame(self) -> bytes:
        if self.weather[0] != "ok":
            return frame(b"W", extras.weather_error(self.weather[1]))
        _, place, forecast, fetched_at = self.weather
        try:
            return frame(b"W", extras.weather_payload(place, forecast, bool(self.device["WC"]), bool(self.device["H24"]), to_ascii, fetched_at))
        except Exception as e:
            if self.args.verbose:
                self.log("weather format error:", repr(e))
            return frame(b"W", extras.weather_error("Could not read the weather."))

    def _calendar_frame(self) -> bytes:
        if self.calendar[0] != "ok":
            return frame(b"E", notion_calendar.error_payload(self.calendar[1]))
        return frame(b"E", notion_calendar.events_payload(self.calendar[1], bool(self.device["H24"]), extras.fmt_clock, to_ascii))

    def _event_alerts(self):
        """Timed events that start within the next ten minutes and have not been announced."""
        if self.calendar[0] != "ok":
            return []
        now = datetime.datetime.now()
        out = []
        for e in self.calendar[1]:
            start = e["start"]
            if start is None:
                continue
            key = (start, e["title"])
            if datetime.timedelta(0) <= start - now <= datetime.timedelta(minutes=10) and key not in self.fired_events:
                self.fired_events.add(key)
                out.append(extras.fmt_clock(start.hour, start.minute, bool(self.device["H24"])).encode() + b"\n" + to_ascii(e["title"], 64) + b"\n")
        return out

    def _device_settings(self, text: str) -> bool:
        """'H24=0,WC=1,...' from the display. Returns True if anything changed."""
        changed = False
        for part in text.split(","):
            key, _, value = part.partition("=")
            if key in self.device and value.strip().lstrip("-").isdigit() and self.device[key] != int(value):
                self.device[key] = int(value)
                changed = True
        self.reminders.h24 = bool(self.device["H24"])
        return changed

    def log(self, *a):
        print(time.strftime("%H:%M:%S"), *a, flush=True)

    def _watch_for_freezes(self):
        """Runs in the background. If the main loop stops moving for a few seconds, says where it is stuck."""
        import traceback
        main_id = None
        reported = 0.0
        while True:
            time.sleep(1.0)
            beat = getattr(self, "_beat", None)
            if beat is None:
                continue
            stuck = time.monotonic() - beat
            if stuck > 3.0 and beat != reported:
                reported = beat
                frame = sys._current_frames().get(main_id or getattr(self, "_loop_ident", 0))
                where = traceback.format_stack(frame)[-4:] if frame else ["(unknown)"]
                self.log(f"The companion has been stuck for {stuck:.0f} seconds here:")
                for part in where:
                    for line in part.rstrip().splitlines():
                        self.log("   ", line.strip())

    def run(self):
        threading.Thread(target=self._watch_for_freezes, daemon=True).start()
        waiting_shown = False
        busy_shown = False
        while True:
            port = find_port(self.args.port)
            if not port:
                if not waiting_shown:
                    self.log("Looking for the display. Plug it in with a USB data cable.")
                    waiting_shown = True
                time.sleep(2)
                continue
            waiting_shown = False
            link = None
            try:
                link = Link(port)
                self.log(f"Opened {port}. Waiting for the display to answer.")
                self.session(link)
            except KeyboardInterrupt:
                raise
            except Exception as e:
                if "Access is denied" in str(e) or isinstance(e, PermissionError):
                    if not busy_shown:          # say it once, then keep trying quietly
                        self.log(f"{port} is in use by another program, most likely another copy of this companion "
                                 "(the taskbar icon). Quit that one and this one connects by itself.")
                        busy_shown = True
                else:
                    busy_shown = False
                    self.log(f"Lost the display ({e}). Trying again.")
            finally:
                self._beat = None                 # not talking to the display now: nothing to watch
                if link:
                    link.close()
            time.sleep(2)

    def session(self, link: Link):
        self._loop_ident = threading.get_ident()      # the thread to look at if the companion freezes
        ready = False
        last_hello = 0.0
        sent_key = None
        last_state = None
        last_state_at = 0.0
        next_poll = 0.0
        art_due: list = []
        art_hash = None
        art_frame = None
        online_started = False
        current: Optional[NowPlaying] = None
        warned_no_answer = False
        opened_at = time.monotonic()
        last_clock_at = 0.0          # when the time, reminders and alerts were last sent or checked
        last_reminder_check = 0.0
        last_alert_check = 0.0
        last_stats_at = 0.0
        last_calendar_at = 0.0
        weather_sent = None
        calendar_sent = None
        last_levels_at = 0.0
        levels_sent = None
        old_art, old_album, last_album, track_started = None, None, None, 0.0
        viz_queue = []

        while True:
            now = time.monotonic()
            self._beat = now
            if link.error:
                raise link.error

            # ---- messages from the display
            while True:
                try:
                    line = link.lines.get_nowait()
                except queue.Empty:
                    break
                if line.startswith("RAW:"):
                    if any(k in line for k in ("Guru", "panic", "Brownout", "rst:", "Backtrace", "abort", "assert", "Stack", "WDT")):
                        self.log("display says:", line[4:200])
                    elif self.args.verbose:
                        self.log("display says:", line[4:200])
                    continue
                if line.startswith("DT:READY"):
                    if ready:
                        self.log("The display restarted.")
                    else:
                        self.log("Display connected.")
                    ready = True
                    sent_key, last_state, art_hash, art_frame = None, None, None, None
                    next_poll = 0.0
                    last_clock_at, weather_sent, calendar_sent = 0.0, None, None
                    self.reminders.stamp = None            # send the list again
                elif line.startswith("DT:SET:"):
                    if self._device_settings(line[7:]):
                        self.reminders.stamp = None        # times may need reformatting
                        last_calendar_at = 0.0
                elif line.startswith("DT:ERR:NODISPLAY"):
                    self.log("The board cannot find its screen. Set DISPLAY_DRIVER to 1 or 2 in the firmware and upload again.")
                elif line == "DT:NAK:A" and art_frame:
                    link.send(art_frame)
                elif line.startswith("DT:CMD:REM:"):
                    try:
                        self.reminders.toggle(int(line[11:]))
                    except Exception as e:
                        self.log(f"Could not update reminders.txt: {e}")
                    self.reminders.stamp = None
                    last_reminder_check = 0.0
                elif line.startswith("DT:CMD:"):
                    self.command(line[7:])
                    next_poll = min(next_poll, now + 0.15)

            if not ready:
                if now - last_hello >= 1.0:
                    link.send(frame(b"H"))
                    last_hello = now
                if not warned_no_answer and now - opened_at > 12:
                    self.log("No answer yet. Check that the firmware is on the board, or pass --port to pick another port.")
                    warned_no_answer = True
                time.sleep(0.05)
                continue

            # ---- clock, weather, calendar, reminders, computer stats
            if now - last_clock_at >= 30.0:
                link.send(frame(b"T", extras.time_payload()))
                last_clock_at = now

            try:
                while True:
                    self.weather = self.weather_updates.get_nowait()
            except queue.Empty:
                pass
            weather_frame = self._weather_frame()
            if weather_frame != weather_sent:
                link.send(weather_frame)
                weather_sent = weather_frame

            try:
                while True:
                    self.calendar = self.calendar_updates.get_nowait()
                    last_calendar_at = 0.0
            except queue.Empty:
                pass
            if now - last_calendar_at >= 30.0:              # also keeps "TODAY" and passed events current
                last_calendar_at = now
                calendar_frame = self._calendar_frame()
                if calendar_frame != calendar_sent:
                    link.send(calendar_frame)
                    calendar_sent = calendar_frame

            if now - last_reminder_check >= 1.0:
                last_reminder_check = now
                if self.reminders.changed():
                    self.reminders.load()
                    link.send(frame(b"R", self.reminders.payload()))
            if now - last_alert_check >= 5.0:
                last_alert_check = now
                alerts = (self.reminders.due_alerts() if self.device["RA"] else []) + \
                         (self._event_alerts() if self.device["CA"] else [])
                for alert in alerts:
                    link.send(frame(b"N", alert))
                    self.log("Alert:", alert.decode().replace("\n", " ").strip())

            if now - last_stats_at >= 2.0:
                last_stats_at = now
                stats = self.stats.payload()
                if stats:
                    link.send(frame(b"Y", stats))

            # ---- what is playing
            if now >= next_poll:
                next_poll = now + 0.5
                try:
                    current = self.media.poll()
                except Exception as e:
                    if self.args.verbose:
                        self.log("media error:", repr(e))
                    current = None

                if current is None:
                    if sent_key != "IDLE" or now - last_state_at >= 1.0:
                        link.send(frame(b"X"))
                        if sent_key != "IDLE":
                            self.log("Nothing playing.")
                        sent_key, last_state, last_state_at = "IDLE", None, now
                else:
                    if current.key() != sent_key:
                        link.send(encode_meta(current))
                        self.log(f"Now playing: {current.title} / {current.artist}")
                        sent_key = current.key()
                        last_state = None
                        art_due = [now + t for t in self.ART_RETRY_AT]
                        # The music app can hand over the last song's cover for a moment: remember it, to skip it.
                        old_art, old_album = art_hash, last_album
                        last_album = current.album
                        track_started = now
                        art_hash, art_frame, online_started = None, None, False
                        # lyrics: tell the display we're looking, then look in the background
                        link.send(frame(b"L", b"W"))
                        self.lyrics.request(current.key(), current.title, current.artist, current.album,
                                            (current.duration_ms or 0) / 1000.0 or None)
                    state = (current.playing, current.can_seek, current.duration_ms, current.position_ms is None)
                    if state != last_state or now - last_state_at >= 1.0:
                        link.send(encode_state(current))
                        last_state, last_state_at = state, now

            # ---- lyrics that have come back for the song that's playing
            for key, kind, lines in self.lyrics.take():
                if current is not None and key == current.key():
                    link.send(frame(b"L", lyrics.payload(kind, lines, to_ascii)))

            # ---- sound levels for the visualizer, about 14 times a second while music plays
            if now - last_levels_at >= 0.04:              # 25 times a second
                last_levels_at = now
                levels = self.levels.read(bool(current and current.playing and self.device["VZ"]))
                if self.viz_delay > 0:                     # hold the bars back to match slow (e.g. Bluetooth) speakers
                    viz_queue.append((now, levels))
                    levels = None
                    while viz_queue and now - viz_queue[0][0] >= self.viz_delay:
                        levels = viz_queue.pop(0)[1]
                    if len(viz_queue) > 200:
                        del viz_queue[:-200]
                if levels is not None and (levels != levels_sent or any(levels)):
                    link.send(frame(b"V", levels))
                    levels_sent = levels

            # ---- album art
            if art_due and now >= art_due[0] and current is not None and current.key() == sent_key:
                art_due.pop(0)
                data = None
                try:
                    data = self.media.get_art(current)
                except Exception as e:
                    if self.args.verbose:
                        self.log("art error:", repr(e))
                if data:
                    h = hashlib.sha1(data).digest()
                    stale = h == old_art and current.album != old_album      # the previous song's cover, late
                    if stale and self.args.verbose:
                        self.log("skipped the previous song's cover; asking again")
                    if h != art_hash and not stale:
                        try:
                            art_frame = encode_art(data)
                            link.send(art_frame)
                            art_hash = h
                        except Exception as e:
                            if self.args.verbose:
                                self.log("could not read the picture:", repr(e))
                if art_hash is None and not online_started and now - track_started >= 3.0 and not self.args.no_online_art:
                    online_started = True
                    threading.Thread(target=self._online_art, args=(current,), daemon=True).start()

            try:
                key, data = self.online_results.get_nowait()
            except queue.Empty:
                key, data = None, None
            if data and key == sent_key and art_hash is None:
                try:
                    online_frame = encode_art(data)
                except Exception:
                    online_frame = None
                if online_frame:
                    art_frame = online_frame
                    link.send(art_frame)
                    art_hash = hashlib.sha1(data).digest()

            time.sleep(0.01)

    def _online_art(self, np: NowPlaying):
        self.online_results.put((np.key(), lookup_art_online(np)))

    def command(self, cmd: str):
        try:
            if cmd == "PLAY":
                self.media.play_pause()
            elif cmd == "NEXT":
                self.media.next()
            elif cmd == "PREV":
                self.media.previous()
            elif cmd.startswith("SEEK:"):
                self.media.seek(int(cmd[5:]))
            elif cmd == "OPENAPP":
                self.log("Opening Apple Music.")
                threading.Thread(target=open_music_app, args=(self.log,), daemon=True).start()
            if self.args.verbose:
                self.log("display:", cmd)
        except Exception as e:
            self.log(f"Could not do '{cmd}': {e}")


APPLE_MUSIC_WINDOWS_ID = "AppleInc.AppleMusicWin_nzyj5cx40ttqa!App"   # the Microsoft Store app's usual ID
_music_app_id = None


def open_music_app(log=print):
    """Starts Apple Music (or brings it to the front). Windows: the Store app, or iTunes if that's
    what's installed. Mac: the Music app."""
    global _music_app_id
    try:
        if sys.platform == "darwin":
            subprocess.run(["open", "-a", "Music"], timeout=10)
            return
        if sys.platform != "win32":
            log("Opening Apple Music only works on Windows and Mac.")
            return
        if _music_app_id is None:
            # Ask Windows for the app's ID, since it can differ between installs.
            out = subprocess.run(
                ["powershell", "-NoProfile", "-Command",
                 "Get-StartApps | Where-Object { $_.Name -eq 'Apple Music' -or $_.Name -eq 'iTunes' } | "
                 "Sort-Object Name | Select-Object -First 1 -ExpandProperty AppID"],
                capture_output=True, text=True, timeout=15,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)).stdout.strip()
            _music_app_id = out.splitlines()[0].strip() if out else APPLE_MUSIC_WINDOWS_ID
        subprocess.Popen(["explorer.exe", "shell:AppsFolder\\" + _music_app_id])
    except Exception as e:
        log(f"Could not open Apple Music: {e}")


def main():
    ap = argparse.ArgumentParser(description="Send what's playing to the Desk Display.")
    ap.add_argument("--port", help="serial port of the display, e.g. COM5 or /dev/cu.usbserial-110 (found automatically if left out)")
    ap.add_argument("--demo", action="store_true", help="show fake tracks instead of real music, to test the display")
    ap.add_argument("--app", help="Windows only: the app to follow and control (default: applemusic). "
                                   "--app any follows whatever Windows is showing")
    ap.add_argument("--no-online-art", action="store_true", help="never look covers up online when the music app has none")
    ap.add_argument("--data-dir", default=os.path.dirname(os.path.abspath(__file__)),
                    help="folder holding settings.json and reminders.txt (default: this folder)")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--tray", action="store_true", help="Windows: run as an icon in the taskbar corner, with no window")
    args = ap.parse_args()
    if sys.platform == "win32" and not args.demo:
        # Only Apple Music: the display shows and controls nothing else (no browser tabs, games or videos).
        if args.app is None:
            args.app = "applemusic"
        elif args.app.strip().lower() in ("any", "all", "*"):
            args.app = None

    try:
        import serial  # noqa: F401
        import PIL     # noqa: F401
    except ImportError:
        sys.exit("Missing packages. Run:  python -m pip install -r requirements.txt")

    def start():
        if args.demo:
            media = make_media(args)
        else:
            from media_worker import MediaWorker      # keeps a stuck music app from freezing the link to the display
            log = lambda *a: print(time.strftime("%H:%M:%S"), *a, flush=True)
            media = MediaWorker(lambda: make_media(args), log=log, verbose=args.verbose)
        Companion(media, args).run()

    if args.tray:
        import tray
        tray.run(start, args.data_dir)
        os._exit(0)
    try:
        start()
    except KeyboardInterrupt:
        print("\nStopped.", flush=True)
    # End right away. A helper still waiting on the music app or the sound card
    # must not keep the companion running in the background.
    os._exit(0)


if __name__ == "__main__":
    main()
