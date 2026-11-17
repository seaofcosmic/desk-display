"""Computer readings for the Desk Display's System app: CPU, memory, disk, temperatures.

CPU, memory and disk work everywhere. Temperatures depend on the computer:
    Windows  CPU and graphics temperature if LibreHardwareMonitor (free) is running;
             graphics temperature from NVIDIA cards without it.
    Mac      not available. macOS does not give temperatures to ordinary apps.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import threading
import time
from typing import Optional

try:
    import psutil
except ImportError:          # the System app then shows "--"
    psutil = None

_NO_WINDOW = 0x08000000 if sys.platform == "win32" else 0


def _run(cmd, timeout=8.0) -> Optional[str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, creationflags=_NO_WINDOW)
        return r.stdout.strip() if r.returncode == 0 and r.stdout.strip() else None
    except Exception:
        return None


def _sane(value) -> Optional[int]:
    try:
        v = float(value)
    except (TypeError, ValueError):
        return None
    return int(round(v)) if 1 <= v <= 125 else None


def pick_temps(sensors) -> tuple:
    """From LibreHardwareMonitor's temperature sensors -> (cpu, graphics)."""
    if isinstance(sensors, dict):
        sensors = [sensors]
    cpu = gpu = None
    cpu_all, gpu_all = [], []
    for s in sensors or []:
        name, parent, value = str(s.get("Name", "")), str(s.get("Parent", "")).lower(), _sane(s.get("Value"))
        if value is None:
            continue
        if "cpu" in parent:
            cpu_all.append(value)
            if name in ("CPU Package", "Core (Tctl/Tdie)", "Core Average") and cpu is None:
                cpu = value
        elif "gpu" in parent:
            gpu_all.append(value)
            if name == "GPU Core" and gpu is None:
                gpu = value
    return (cpu if cpu is not None else (max(cpu_all) if cpu_all else None),
            gpu if gpu is not None else (max(gpu_all) if gpu_all else None))


def _hardware_monitor() -> tuple:
    for namespace in ("root/LibreHardwareMonitor", "root/OpenHardwareMonitor"):
        out = _run(["powershell", "-NoProfile", "-Command",
                    f"Get-CimInstance -Namespace {namespace} -ClassName Sensor -ErrorAction Stop | "
                    "Where-Object SensorType -eq 'Temperature' | Select-Object Name,Value,Parent | ConvertTo-Json -Compress"])
        if out:
            try:
                cpu, gpu = pick_temps(json.loads(out))
            except ValueError:
                continue
            if cpu is not None or gpu is not None:
                return cpu, gpu
    return None, None


def _nvidia() -> Optional[int]:
    out = _run(["nvidia-smi", "--query-gpu=temperature.gpu", "--format=csv,noheader,nounits"], timeout=5.0)
    return _sane(out.splitlines()[0]) if out else None


def _linux() -> tuple:
    try:
        temps = psutil.sensors_temperatures()
    except Exception:
        return None, None
    for chip in ("coretemp", "k10temp", "zenpower", "cpu_thermal", "acpitz"):
        if temps.get(chip):
            return _sane(max(t.current for t in temps[chip])), None
    return None, None


class PcStats:
    def __init__(self):
        self.cpu_temp: Optional[int] = None
        self.gpu_temp: Optional[int] = None
        self.available = psutil is not None
        if self.available:
            psutil.cpu_percent(interval=None)        # the first call only sets a starting point
            threading.Thread(target=self._temp_loop, daemon=True).start()

    def _temp_loop(self):
        misses = 0
        while True:
            cpu = gpu = None
            if sys.platform == "win32":
                cpu, gpu = _hardware_monitor()
                if gpu is None:
                    gpu = _nvidia()
            elif sys.platform.startswith("linux"):
                cpu, gpu = _linux()
            self.cpu_temp, self.gpu_temp = cpu, gpu
            misses = 0 if (cpu is not None or gpu is not None) else misses + 1
            time.sleep(10 if misses < 3 else 120)     # nothing to read: stop asking so often

    def payload(self) -> Optional[bytes]:
        if not self.available:
            return None
        try:
            cpu = int(round(psutil.cpu_percent(interval=None)))
            mem = int(round(psutil.virtual_memory().percent))
            root = (os.environ.get("SystemDrive", "C:") + "\\") if sys.platform == "win32" else "/"
            disk = int(round(psutil.disk_usage(root).percent))
        except Exception:
            return None
        def show(v):
            return "-" if v is None else str(v)
        return f"{cpu}\n{mem}\n{disk}\n{show(self.cpu_temp)}\n{show(self.gpu_temp)}\n".encode()
