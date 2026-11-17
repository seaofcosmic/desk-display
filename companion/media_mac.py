"""Mac: read and control the Music app (Apple Music) through AppleScript.

The first time this runs, macOS asks whether Terminal may control Music. Choose Allow.
"""
from __future__ import annotations

import os
import subprocess
import tempfile
from typing import Optional

from nowplaying import NowPlaying

SEP = "\x1f"

POLL_SCRIPT = '''
if application "Music" is not running then return "OFF"
tell application "Music"
	set st to (player state as text)
	if st is "stopped" then return "OFF"
	set sep to (ASCII character 31)
	set nm to ""
	set ar to ""
	set al to ""
	set du to 0
	set po to -1
	try
		set nm to (name of current track) as text
	end try
	try
		set ar to (artist of current track) as text
	end try
	try
		set al to (album of current track) as text
	end try
	try
		set du to (duration of current track)
	end try
	try
		set po to player position
	end try
	return st & sep & nm & sep & ar & sep & al & sep & (du as text) & sep & (po as text)
end tell
'''

ART_SCRIPT = '''
on run argv
	set outPath to item 1 of argv
	tell application "Music"
		try
			set artData to raw data of artwork 1 of current track
		on error
			return "NONE"
		end try
	end tell
	try
		set f to open for access (POSIX file outPath) with write permission
		set eof of f to 0
		write artData to f
		close access f
	on error
		try
			close access (POSIX file outPath)
		end try
		return "NONE"
	end try
	return "OK"
end run
'''


def _osascript(script: str, *args: str, timeout: float = 6.0) -> str:
    r = subprocess.run(["osascript", "-e", script, *args], capture_output=True, text=True, timeout=timeout)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip() or "osascript failed")
    return r.stdout.rstrip("\n")


def _number(text: str) -> Optional[float]:
    try:
        return float(text.strip().replace(",", "."))     # some system languages write 12,5
    except ValueError:
        return None


class MacMusic:
    def __init__(self):
        self.art_path = os.path.join(tempfile.gettempdir(), "desk_display_art.bin")

    def poll(self) -> Optional[NowPlaying]:
        out = _osascript(POLL_SCRIPT)
        if out == "OFF" or SEP not in out:
            return None
        parts = out.split(SEP)
        if len(parts) < 6:
            return None
        state, title, artist, album, dur, pos = parts[:6]
        duration = _number(dur) or 0.0
        position = _number(pos)
        duration_ms = int(duration * 1000) if duration > 0 else 0
        position_ms = None
        if position is not None and position >= 0 and duration_ms > 0:
            position_ms = min(int(position * 1000), duration_ms)
        return NowPlaying(title, artist, album, state == "playing", position_ms, duration_ms, duration_ms > 0)

    def get_art(self, np: NowPlaying) -> Optional[bytes]:
        try:
            if _osascript(ART_SCRIPT, self.art_path) != "OK":
                return None
            with open(self.art_path, "rb") as f:
                data = f.read()
            return data or None
        except Exception:
            return None

    def play_pause(self):
        _osascript('tell application "Music" to playpause')

    def next(self):
        _osascript('tell application "Music" to next track')

    def previous(self):
        _osascript('tell application "Music" to back track')

    def seek(self, ms: int):
        _osascript(f'tell application "Music" to set player position to {ms / 1000:.2f}')
