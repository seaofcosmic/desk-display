"""Runs the companion as a small icon in the taskbar corner, with no window.

Started by start_tray.bat. Right-click the icon to see what it is doing, open its log,
or quit. Everything the window version would print goes to companion.log instead.
"""
from __future__ import annotations

import os
import socket
import sys
import threading
import time

LOG_NAME = "companion.log"
LOG_MAX_BYTES = 1_000_000       # start the log over when it gets bigger than this
SINGLE_PORT = 47213             # a local port held while running, so it can't start twice


class _LogFile:
    """Stands in for the screen: writes to the log file and remembers the last line for the menu."""

    def __init__(self, path: str):
        if os.path.exists(path) and os.path.getsize(path) > LOG_MAX_BYTES:
            os.replace(path, path + ".old")
        self.f = open(path, "a", encoding="utf-8", buffering=1)
        self.last = "Starting"
        self.lock = threading.Lock()

    def write(self, text: str):
        with self.lock:
            self.f.write(text)
            for line in text.splitlines():
                line = line.strip()
                if line:
                    self.last = line
        return len(text)

    def flush(self):
        self.f.flush()


def _icon_image():
    """A small amber play symbol on a dark rounded square, in the display's colors."""
    from PIL import Image, ImageDraw
    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((2, 2, 61, 61), radius=12, fill=(14, 12, 10, 255), outline=(239, 159, 39, 255), width=3)
    d.polygon([(20, 16), (20, 48), (46, 32)], fill=(239, 159, 39, 255))
    return img


def _status(last: str) -> str:
    """The latest log line without its time, short enough for the menu and the tooltip."""
    parts = last.split(" ", 1)
    text = parts[1] if len(parts) == 2 and parts[0].count(":") == 2 else last
    return text if len(text) <= 60 else text[:57] + "..."


def run(start_companion, data_dir: str):
    """start_companion() runs the companion and does not return. It is started in the background."""
    # Only one copy at a time: a second one could not reach the display anyway.
    guard = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        guard.bind(("127.0.0.1", SINGLE_PORT))
    except OSError:
        sys.exit(0)

    log_path = os.path.join(data_dir, LOG_NAME)
    out = _LogFile(log_path)
    sys.stdout = sys.stderr = out
    print(time.strftime("%H:%M:%S"), "Running in the taskbar.", flush=True)

    import pystray

    def open_log(icon, item):
        os.startfile(log_path) if hasattr(os, "startfile") else None

    def quit_app(icon, item):
        print(time.strftime("%H:%M:%S"), "Stopped from the taskbar.", flush=True)
        icon.visible = False
        icon.stop()
        os._exit(0)

    menu = pystray.Menu(
        pystray.MenuItem(lambda item: _status(out.last), None, enabled=False),
        pystray.Menu.SEPARATOR,
        pystray.MenuItem("Open log", open_log),
        pystray.MenuItem("Quit Desk Display", quit_app),
    )
    icon = pystray.Icon("DeskDisplay", _icon_image(), "Desk Display", menu)

    def keep_status_fresh():
        shown = None
        while True:
            text = "Desk Display: " + _status(out.last)
            if text != shown:
                shown = text
                try:
                    icon.title = text[:120]
                    icon.update_menu()
                except Exception:
                    pass
            time.sleep(1.0)

    def setup(icon):
        icon.visible = True
        threading.Thread(target=start_companion, daemon=True, name="companion").start()
        threading.Thread(target=keep_status_fresh, daemon=True, name="tray-status").start()

    icon.run(setup=setup)
    os._exit(0)
