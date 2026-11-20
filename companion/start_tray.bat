@echo off
rem Starts the Desk Display companion as an icon in the taskbar corner, with no window.
rem Right-click the icon to see what it is doing, open its log, or quit.
cd /d "%~dp0"
if not exist .venv\ready.txt (
  echo First run: setting up. This takes a minute.
  if not exist .venv ( py -3 -m venv .venv || goto :nopython )
)
.venv\Scripts\python -m pip install -q --disable-pip-version-check -r requirements.txt
if errorlevel 1 if not exist .venv\ready.txt goto :failed
echo ok> .venv\ready.txt
.venv\Scripts\python -m pip install -q --disable-pip-version-check -r requirements-visualizer.txt >nul 2>&1
start "" .venv\Scripts\pythonw.exe desk_display.py --tray %*
exit /b
:nopython
echo Python was not found. Install Python 3.11 or newer from python.org, then run this again.
pause
exit /b
:failed
echo Setup failed. Check your internet connection, then run this again.
pause
