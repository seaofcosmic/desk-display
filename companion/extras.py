"""Clock, weather and reminders for the Desk Display.

    settings.json    city for the weather, temperature unit, 12 or 24 hour clock
    reminders.txt    one reminder per line; tap one on the display to check it off
"""
from __future__ import annotations

import calendar
import datetime
import json
import os
import re
import struct
import urllib.parse
import urllib.request
from typing import Optional

DEFAULT_SETTINGS = {"city": "", "temperature": "F", "clock_24_hour": False}

REMINDERS_TEMPLATE = """# Desk Display reminders. One per line. Save the file and the display updates.
# Start a line with a time (3:30pm or 15:30) to get a full-screen alert at that time today.
# Tapping a reminder on the display marks it done by putting [x] in front of it here.
# Lines starting with # are ignored.

Tap me on the display to check me off
"""

MAX_SENT = 10


def load_settings(data_dir: str, log=print) -> dict:
    settings = dict(DEFAULT_SETTINGS)
    path = os.path.join(data_dir, "settings.json")
    try:
        with open(path, encoding="utf-8") as f:
            loaded = json.load(f)
        if isinstance(loaded, dict):
            settings.update(loaded)
    except FileNotFoundError:
        pass
    except Exception as e:
        log(f"settings.json could not be read ({e}). Using the defaults.")
    return settings


# --------------------------------------------------------------------------- clock
def fmt_clock(hour: int, minute: int, h24: bool) -> str:
    if h24:
        return f"{hour:02d}:{minute:02d}"
    return f"{hour % 12 or 12}:{minute:02d} {'PM' if hour >= 12 else 'AM'}"


def time_payload(h24: bool, now: Optional[datetime.datetime] = None) -> bytes:
    """Local wall-clock time as seconds since 1970, so the display needs no time zone rules."""
    now = now or datetime.datetime.now()
    return struct.pack("<IB", calendar.timegm(now.timetuple()), 1 if h24 else 0)


# --------------------------------------------------------------------------- weather
# Open-Meteo weather codes -> (words on the display, icon number in apps.h)
WEATHER_CODES = {
    0: ("CLEAR", 0), 1: ("MOSTLY CLEAR", 1), 2: ("PARTLY CLOUDY", 1), 3: ("CLOUDY", 2),
    45: ("FOG", 3), 48: ("FOG", 3),
    51: ("LIGHT DRIZZLE", 4), 53: ("DRIZZLE", 4), 55: ("HEAVY DRIZZLE", 4), 56: ("FREEZING DRIZZLE", 4), 57: ("FREEZING DRIZZLE", 4),
    61: ("LIGHT RAIN", 4), 63: ("RAIN", 4), 65: ("HEAVY RAIN", 4), 66: ("FREEZING RAIN", 4), 67: ("FREEZING RAIN", 4),
    71: ("LIGHT SNOW", 5), 73: ("SNOW", 5), 75: ("HEAVY SNOW", 5), 77: ("SNOW", 5),
    80: ("LIGHT SHOWERS", 4), 81: ("SHOWERS", 4), 82: ("HEAVY SHOWERS", 4),
    85: ("SNOW SHOWERS", 5), 86: ("SNOW SHOWERS", 5),
    95: ("THUNDERSTORM", 6), 96: ("THUNDERSTORM", 6), 99: ("THUNDERSTORM", 6),
}


def _get_json(url: str, timeout: float = 8.0):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.load(r)


def find_place(city: str):
    """'Culver City' or 'Springfield, Illinois' -> (name, latitude, longitude)."""
    parts = [p.strip() for p in city.split(",") if p.strip()]
    hint = " ".join(parts[1:]).lower()
    url = "https://geocoding-api.open-meteo.com/v1/search?" + urllib.parse.urlencode(
        {"name": parts[0], "count": 10, "language": "en", "format": "json"})
    results = _get_json(url).get("results") or []
    if not results:
        raise LookupError(f"No place called {city}")
    best = results[0]
    if hint:
        for r in results:
            where = [str(r.get(k, "")).lower() for k in ("admin1", "country", "country_code")]
            if any(hint == w or (len(hint) > 2 and hint in w) for w in where):
                best = r
                break
    return best["name"], best["latitude"], best["longitude"]


def fetch_weather(settings: dict, place=None):
    """Returns (place, forecast). 'place' can be passed back in to skip the name lookup."""
    if place is None:
        if "latitude" in settings and "longitude" in settings:
            place = (settings.get("city") or "WEATHER", settings["latitude"], settings["longitude"])
        else:
            place = find_place(settings["city"])
    metric = str(settings.get("temperature", "F")).upper().startswith("C")
    url = "https://api.open-meteo.com/v1/forecast?" + urllib.parse.urlencode({
        "latitude": place[1], "longitude": place[2],
        "current": "temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m",
        "daily": "weather_code,temperature_2m_max,temperature_2m_min",
        "temperature_unit": "celsius" if metric else "fahrenheit",
        "wind_speed_unit": "kmh" if metric else "mph",
        "timezone": "auto", "forecast_days": 3,
    })
    return place, _get_json(url)


def weather_payload(place, forecast: dict, settings: dict, to_ascii, now: Optional[datetime.datetime] = None) -> bytes:
    now = now or datetime.datetime.now()
    metric = str(settings.get("temperature", "F")).upper().startswith("C")
    cur, daily = forecast["current"], forecast["daily"]
    words, icon = WEATHER_CODES.get(int(cur["weather_code"]), ("", 2))

    def deg(v):
        return str(int(round(float(v))))

    lines = [
        b"OK",
        to_ascii(str(place[0]).upper(), 26),
        deg(cur["temperature_2m"]).encode(),
        b"C" if metric else b"F",
        words.encode(),
        str(icon).encode(),
        deg(daily["temperature_2m_max"][0]).encode(),
        deg(daily["temperature_2m_min"][0]).encode(),
        deg(cur["apparent_temperature"]).encode(),
        f"{deg(cur['wind_speed_10m'])} {'KM/H' if metric else 'MPH'}".encode(),
        f"{deg(cur['relative_humidity_2m'])}%".encode(),
        fmt_clock(now.hour, now.minute, bool(settings.get("clock_24_hour"))).encode(),
    ]
    for i, day in enumerate(daily["time"][:3]):
        name = "TODAY" if i == 0 else datetime.date.fromisoformat(day).strftime("%a").upper()
        day_icon = WEATHER_CODES.get(int(daily["weather_code"][i]), ("", 2))[1]
        lines.append(f"{name}|{day_icon}|{deg(daily['temperature_2m_max'][i])}|{deg(daily['temperature_2m_min'][i])}".encode())
    return b"\n".join(lines) + b"\n"


def weather_error(message: str) -> bytes:
    return b"ERR\n" + message.encode("ascii", "replace")[:60] + b"\n"


DEMO_WEATHER = b"OK\nDEMO CITY\n68\nF\nPARTLY CLOUDY\n1\n72\n55\n66\n8 MPH\n61%\n7:42 AM\nTODAY|1|72|55\nTUE|4|64|52\nWED|0|75|56\n"


# --------------------------------------------------------------------------- reminders
_DONE_RE = re.compile(r"^\[([ xX])\]\s*")
_TIME_RE = re.compile(r"^(?:(\d{1,2}):(\d{2})\s*([ap])?\.?m?\.?|(\d{1,2})\s*([ap])\.?m\.?)(?=[\s,:-]|$)[\s,:-]*", re.I)


def parse_reminder(line: str):
    """-> (done, minutes after midnight or None, text), or None for blank lines and comments."""
    s = line.strip()
    if not s or s.startswith("#"):
        return None
    if s.startswith(("- ", "* ")):
        s = s[2:].lstrip()
    done = False
    m = _DONE_RE.match(s)
    if m:
        done = m.group(1) in "xX"
        s = s[m.end():]
    minutes = None
    m = _TIME_RE.match(s)
    if m and s[m.end():].strip():
        hour = int(m.group(1) or m.group(4))
        minute = int(m.group(2) or 0)
        ampm = (m.group(3) or m.group(5) or "").lower()
        ok = minute < 60 and ((1 <= hour <= 12) if ampm else hour < 24)
        if ok:
            if ampm:
                hour = hour % 12 + (12 if ampm == "p" else 0)
            minutes = hour * 60 + minute
            s = s[m.end():]
    s = s.strip()
    return (done, minutes, s) if s else None


class Reminders:
    def __init__(self, path: str, h24: bool, to_ascii):
        self.path = path
        self.h24 = h24
        self.to_ascii = to_ascii
        self.stamp = None
        self.items = []        # (line number, done, minutes, text)
        self.sent = []         # line numbers, in the order last sent to the display
        self.fired = set()     # alerts already shown
        if not os.path.exists(path):
            try:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(REMINDERS_TEMPLATE)
            except OSError:
                pass

    def _stamp(self):
        try:
            st = os.stat(self.path)
            return (st.st_mtime_ns, st.st_size)
        except OSError:
            return None

    def changed(self) -> bool:
        return self._stamp() != self.stamp

    def load(self):
        self.stamp = self._stamp()
        self.items = []
        try:
            with open(self.path, encoding="utf-8", errors="replace") as f:
                for n, line in enumerate(f):
                    parsed = parse_reminder(line)
                    if parsed:
                        self.items.append((n,) + parsed)
        except OSError:
            pass

    def payload(self) -> bytes:
        ordered = [i for i in self.items if not i[1]] + [i for i in self.items if i[1]]
        ordered = ordered[:MAX_SENT]
        self.sent = [i[0] for i in ordered]
        lines = [str(sum(1 for i in self.items if not i[1])).encode()]
        for _, done, minutes, text in ordered:
            when = fmt_clock(minutes // 60, minutes % 60, self.h24) if minutes is not None else ""
            body = self.to_ascii(text, 64).replace(b"|", b"/")
            lines.append((b"1" if done else b"0") + b"|" + when.encode() + b"|" + body)
        return b"\n".join(lines) + b"\n"

    def toggle(self, index: int):
        """The display tapped item 'index' of the list it was last sent."""
        if not 0 <= index < len(self.sent):
            return
        line_no = self.sent[index]
        with open(self.path, encoding="utf-8", errors="replace", newline="") as f:
            lines = f.read().splitlines(keepends=True)
        if line_no >= len(lines):
            return
        raw = lines[line_no]
        body = raw.rstrip("\r\n")
        ending = raw[len(body):]
        indent = body[: len(body) - len(body.lstrip())]
        rest = body.lstrip()
        bullet = ""
        if rest.startswith(("- ", "* ")):
            bullet, rest = rest[:2], rest[2:].lstrip()
        m = _DONE_RE.match(rest)
        was_done = bool(m and m.group(1) in "xX")
        if m:
            rest = rest[m.end():]
        lines[line_no] = indent + bullet + ("" if was_done else "[x] ") + rest + ending
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8", newline="") as f:
            f.write("".join(lines))
        os.replace(tmp, self.path)

    def due_alerts(self, now: Optional[datetime.datetime] = None):
        """Timed reminders that came due in the last ten minutes and have not been shown yet."""
        now = now or datetime.datetime.now()
        minute_now = now.hour * 60 + now.minute
        out = []
        for _, done, minutes, text in self.items:
            if done or minutes is None:
                continue
            key = (now.date(), minutes, text)
            if 0 <= minute_now - minutes < 10 and key not in self.fired:
                self.fired.add(key)
                out.append(fmt_clock(minutes // 60, minutes % 60, self.h24).encode() + b"\n" + self.to_ascii(text, 64) + b"\n")
        return out
