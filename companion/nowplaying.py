"""The one shape every music source reports in."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


@dataclass
class NowPlaying:
    title: str
    artist: str
    album: str
    playing: bool
    position_ms: Optional[int]      # None when the app does not report a position
    duration_ms: int
    can_seek: bool

    def key(self):
        return (self.title, self.artist, self.album)
