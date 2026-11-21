"""Lyrics for the display's lyric mode, from LRCLIB (lrclib.net), a free lyrics service with no account.

Most songs there have "synced" lyrics: every line has the time it is sung, so the display can
show the line being sung right now. Some only have plain lyrics (no times); the display then
moves through them evenly over the length of the song.
"""
from __future__ import annotations

import json
import re
import threading
import urllib.parse
import urllib.request
from typing import List, Optional, Tuple

API = "https://lrclib.net/api"
USER_AGENT = "DeskDisplay/1.1 (https://github.com/seaofcosmic/desk-display)"
MAX_BYTES = 6000          # what the display can hold for one song
MAX_LINE = 80             # characters per line sent to the display

_TIME = re.compile(r"\[(\d+):(\d+(?:\.\d+)?)\]")

Lines = List[Tuple[int, str]]


def parse_lrc(text: str) -> Lines:
    """'[01:02.50]Some words' -> [(62500, 'Some words')], sorted by time. A line can carry several times."""
    out: Lines = []
    for raw in (text or "").splitlines():
        stamps = _TIME.findall(raw)
        if not stamps:
            continue
        words = _TIME.sub("", raw).strip()
        for mm, ss in stamps:
            out.append((int(mm) * 60000 + int(round(float(ss) * 1000)), words))
    out.sort(key=lambda x: x[0])
    return out


def _get(path: str, params: dict, timeout: float):
    url = f"{API}/{path}?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        raise


def _pick(record) -> Tuple[str, Lines]:
    synced = parse_lrc(record.get("syncedLyrics") or "")
    if synced:
        return "S", synced
    plain = [(0, line.strip()) for line in (record.get("plainLyrics") or "").splitlines()]
    if any(t for _, t in plain):
        return "P", plain
    return "N", []


def find(title: str, artist: str, album: str = "", duration_s: Optional[float] = None,
         timeout: float = 8.0) -> Tuple[str, Lines]:
    """Returns ("S", lines) for timed lyrics, ("P", lines) for untimed ones, ("N", []) if none were found.
    Raises on network trouble, so the caller can tell 'no lyrics' from 'couldn't look'."""
    if not title:
        return "N", []
    params = {"track_name": title, "artist_name": artist or ""}
    if album:
        params["album_name"] = album
    if duration_s:
        params["duration"] = int(round(duration_s))
    rec = _get("get", params, timeout)
    if rec and not rec.get("instrumental"):
        kind, lines = _pick(rec)
        if kind != "N":
            return kind, lines
    # Not an exact match (album names often differ): search and take the closest length.
    results = _get("search", {"track_name": title, "artist_name": artist or ""}, timeout) or []
    if duration_s:
        results.sort(key=lambda r: abs((r.get("duration") or 0) - duration_s))
    for want_synced in (True, False):
        for r in results[:10]:
            if r.get("instrumental"):
                continue
            if duration_s and r.get("duration") and abs(r["duration"] - duration_s) > 8:
                continue
            kind, lines = _pick(r)
            if (kind == "S") if want_synced else (kind == "P"):
                return kind, lines
    return "N", []


def payload(kind: str, lines: Lines, to_ascii) -> bytes:
    """What the display gets: a first line S (timed), P (untimed), N (none), W (looking) or E (couldn't
    look), then one "milliseconds|words" line per lyric line."""
    out = bytearray(kind.encode())
    for ms, words in lines:
        row = b"\n" + str(int(ms)).encode() + b"|" + to_ascii(words, MAX_LINE).replace(b"\n", b" ")
        if len(out) + len(row) > MAX_BYTES:
            break
        out += row
    return bytes(out)


class LyricsFetcher:
    """Looks lyrics up in the background and remembers recent songs."""

    def __init__(self, log=print, verbose: bool = False):
        self.log, self.verbose = log, verbose
        self.cache = {}
        self.results = []          # (song key, kind, lines) waiting to be sent
        self.lock = threading.Lock()

    def request(self, key, title, artist, album, duration_s):
        if key in self.cache:
            kind, lines = self.cache[key]
            with self.lock:
                self.results.append((key, kind, lines))
            return

        def work():
            try:
                kind, lines = find(title, artist, album, duration_s)
                self.cache[key] = (kind, lines)
                if len(self.cache) > 30:
                    self.cache.pop(next(iter(self.cache)))
            except Exception as e:
                kind, lines = "E", []
                if self.verbose:
                    self.log("lyrics error:", repr(e))
            if self.verbose:
                said = {"S": "timed", "P": "untimed", "N": "none found", "E": "couldn't reach the lyrics service"}[kind]
                self.log(f"lyrics: {said} ({len(lines)} lines)")
            with self.lock:
                self.results.append((key, kind, lines))

        threading.Thread(target=work, daemon=True, name="lyrics").start()

    def take(self):
        with self.lock:
            out, self.results = self.results, []
        return out
