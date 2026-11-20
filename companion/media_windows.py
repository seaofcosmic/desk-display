"""Windows: read and control whatever app Windows shows in its media controls.

Uses the Windows media session (the same thing behind the volume pop-up and the
media keys), so it works with Apple Music, Spotify, browsers and most players.
"""
from __future__ import annotations

import asyncio
import datetime
from typing import Optional

from winrt.windows.media.control import (
    GlobalSystemMediaTransportControlsSessionManager as SessionManager,
    GlobalSystemMediaTransportControlsSessionPlaybackStatus as PlaybackStatus,
)
from winrt.windows.storage.streams import Buffer, InputStreamOptions

from nowplaying import NowPlaying


async def _wait(operation):
    return await operation


class WindowsMedia:
    def __init__(self, app_filter: Optional[str] = None):
        self.app_filter = (app_filter or "").lower().replace(" ", "")
        self.loop = asyncio.new_event_loop()
        self.manager = self._run(SessionManager.request_async())
        self._props = None

    def _run(self, operation, timeout: float = 3.0):
        # Windows sometimes never answers (Apple Music's cover is known for it). Give up after a few
        # seconds instead of freezing the companion, which would leave the display on NO SIGNAL.
        return self.loop.run_until_complete(asyncio.wait_for(_wait(operation), timeout))

    def _session(self):
        if self.app_filter:
            for s in self.manager.get_sessions():
                if self.app_filter in (s.source_app_user_model_id or "").lower():
                    return s
            return None
        return self.manager.get_current_session()

    def poll(self) -> Optional[NowPlaying]:
        s = self._session()
        if s is None:
            return None
        info = s.get_playback_info()
        status = info.playback_status
        if status not in (PlaybackStatus.PLAYING, PlaybackStatus.PAUSED):
            return None
        props = self._run(s.try_get_media_properties_async())
        if props is None or not props.title:
            return None
        self._props = props
        playing = status == PlaybackStatus.PLAYING

        artist, album = props.artist or "", props.album_title or ""
        if not album and " — " in artist:           # Apple Music reports "Artist — Album" in one field
            artist, album = artist.split(" — ", 1)

        tl = s.get_timeline_properties()
        start = tl.start_time
        duration_ms = int((tl.end_time - start).total_seconds() * 1000)
        position_ms = None
        if duration_ms > 0:
            pos = tl.position - start
            updated = tl.last_updated_time
            if playing and updated is not None and updated.year > 2000:
                if updated.tzinfo is None:
                    updated = updated.replace(tzinfo=datetime.timezone.utc)
                drift = datetime.datetime.now(datetime.timezone.utc) - updated
                if datetime.timedelta(0) <= drift < datetime.timedelta(hours=12):
                    pos += drift
            position_ms = max(0, min(int(pos.total_seconds() * 1000), duration_ms))
        else:
            duration_ms = 0

        # Apple Music for Windows ignores requests to jump to a spot in the song, so the bar is
        # locked for it (it still shows the position). Other apps get to try whenever the song has
        # a length, even if they say they can't: some say so and can anyway.
        app_id = (s.source_app_user_model_id or "").lower().replace(" ", "")
        can_seek = duration_ms > 0 and "applemusic" not in app_id
        return NowPlaying(props.title, artist, album, playing, position_ms, duration_ms, can_seek)

    def get_art(self, np: NowPlaying) -> Optional[bytes]:
        props = self._props
        if props is None or props.thumbnail is None:
            return None

        async def read():
            stream = await props.thumbnail.open_read_async()
            try:
                size = int(stream.size)
                if size <= 0:
                    return None
                buf = Buffer(size)
                result = await stream.read_async(buf, size, InputStreamOptions.READ_AHEAD)
                return bytes(memoryview(result))[: int(result.length)]
            finally:
                stream.close()

        return self.loop.run_until_complete(asyncio.wait_for(read(), 2.0))

    def play_pause(self):
        s = self._session()
        if s:
            self._run(s.try_toggle_play_pause_async())

    def next(self):
        s = self._session()
        if s:
            self._run(s.try_skip_next_async())

    def previous(self):
        s = self._session()
        if s:
            self._run(s.try_skip_previous_async())

    def seek(self, ms: int):
        s = self._session()
        if s:
            start_ticks = int(s.get_timeline_properties().start_time.total_seconds() * 10_000_000)
            ok = self._run(s.try_change_playback_position_async(start_ticks + int(ms) * 10_000))
            if not ok:
                raise RuntimeError("the music app did not allow jumping to that spot in the song")
