"""Talks to the music app in background threads, so the link to the display never waits on it.

Windows sometimes never answers a request about the music (Apple Music's cover is known for it),
and that request cannot be cancelled. If the companion asked directly, it would freeze and the
display would drop to NO SIGNAL. Here every request runs in a helper thread instead:

- one thread checks what is playing twice a second and carries out play, skip and seek,
- another fetches covers.

If a helper stops answering, it is left behind and a fresh one is started in its place.
"""
from __future__ import annotations

import queue
import threading
import time
from typing import Callable, Optional

POLL_EVERY = 0.5          # seconds between checks of what is playing
POLL_STUCK_AFTER = 6.0    # a check taking longer than this counts as stuck
ART_STUCK_AFTER = 5.0     # a cover taking longer than this counts as stuck
MAX_STUCK_ART = 3         # after this many stuck covers, stop asking the music app (online covers still work)


class MediaWorker:
    def __init__(self, factory: Callable[[], object], log=print, verbose: bool = False):
        self.factory = factory
        self.log = log
        self.verbose = verbose
        self.lock = threading.Lock()
        self.latest = None                 # what was playing at the last check
        self.commands: "queue.Queue[tuple]" = queue.Queue()
        self.poll_gen = 0                  # bumped when a stuck poll thread is replaced
        self.poll_beat = time.monotonic()
        self.poll_error: Optional[BaseException] = None
        self.art_requests: "queue.Queue[tuple]" = queue.Queue()
        self.art_results = {}              # song key -> cover bytes, or None if there is none
        self.art_pending = set()           # song keys with a cover request waiting in the helper
        self.art_gen = 0
        self.art_beat = None               # when the art thread started its current request, None if idle
        self.stuck_art = 0
        self._start_poll(first=True)
        self._start_art()

    # ---------------------------------------------------------------- threads
    def _start_poll(self, first: bool = False):
        self.poll_gen += 1
        gen = self.poll_gen
        ready = threading.Event()
        box = {}

        def run():
            try:
                media = self.factory()
            except BaseException as e:          # e.g. a missing package: report it to whoever started us
                box["error"] = e
                ready.set()
                return
            ready.set()
            next_poll = 0.0
            while gen == self.poll_gen:
                self.poll_beat = time.monotonic()
                try:
                    cmd = self.commands.get(timeout=max(0.0, next_poll - time.monotonic()))
                except queue.Empty:
                    cmd = None
                if gen != self.poll_gen:
                    return
                if cmd is not None:
                    name, arg = cmd
                    try:
                        getattr(media, name)(*arg)
                    except Exception as e:
                        self.log(f"Could not do '{name}': {e}")
                    next_poll = 0.0                 # look again soon so the display catches up
                    continue
                self.poll_beat = time.monotonic()
                try:
                    np = media.poll()
                    self.poll_error = None
                except Exception as e:
                    np = None
                    self.poll_error = e
                if gen != self.poll_gen:
                    return
                with self.lock:
                    self.latest = np
                next_poll = time.monotonic() + POLL_EVERY

        threading.Thread(target=run, daemon=True, name=f"media-poll-{gen}").start()
        if first:
            ready.wait(15)
            if "error" in box:
                raise box["error"]

    def _start_art(self):
        self.art_gen += 1
        gen = self.art_gen
        self.art_beat = None

        def run():
            try:
                media = self.factory()
            except Exception:
                return
            while gen == self.art_gen:
                np = self.art_requests.get()
                if gen != self.art_gen:
                    return
                self.art_beat = time.monotonic()
                data = None
                try:
                    media.poll()                    # Windows hands out the cover with what is playing
                    data = media.get_art(np)
                except Exception as e:
                    if self.verbose:
                        self.log("art error:", repr(e))
                if gen != self.art_gen:
                    return                          # took too long; a newer thread has taken over
                with self.lock:
                    self.art_results[np.key()] = data
                    self.art_pending.discard(np.key())
                self.art_beat = None

        threading.Thread(target=run, daemon=True, name=f"media-art-{gen}").start()

    def _check_stuck(self):
        now = time.monotonic()
        if now - self.poll_beat > POLL_STUCK_AFTER:
            self.log("The music app stopped answering. Starting over with it.")
            with self.lock:
                self.latest = None
            self.poll_beat = now
            self._start_poll()
        beat = self.art_beat
        if beat is not None and now - beat > ART_STUCK_AFTER:
            self.stuck_art += 1
            if self.verbose or self.stuck_art == 1:
                self.log("The music app did not hand over the cover. Looking it up online instead.")
            with self.lock:
                self.art_pending.clear()
            self._start_art()

    # ---------------------------------------------------------------- what the companion calls
    def poll(self):
        self._check_stuck()
        if self.poll_error is not None and self.latest is None:
            raise self.poll_error
        with self.lock:
            return self.latest

    def get_art(self, np) -> Optional[bytes]:
        """Never waits. Returns the newest cover the helper has fetched for this song (once), and
        asks it for a fresh one, since Windows can hand over a cover late or the old one first."""
        self._check_stuck()
        key = np.key()
        with self.lock:
            result = self.art_results.pop(key, None)
            if self.stuck_art >= MAX_STUCK_ART or key in self.art_pending:
                return result
            self.art_pending.add(key)
        self.art_requests.put(np)
        return result

    def play_pause(self):
        self.commands.put(("play_pause", ()))

    def next(self):
        self.commands.put(("next", ()))

    def previous(self):
        self.commands.put(("previous", ()))

    def seek(self, ms: int):
        self.commands.put(("seek", (ms,)))
