@echo off
cd /d "%~dp0"
if not exist .venv (
  echo First run: setting up. This takes a minute.
  py -3 -m venv .venv || goto :nopython
)
.venv\Scripts\python -m pip install -q --disable-pip-version-check -r requirements.txt
if errorlevel 1 if not exist .venv\ready.txt goto :failed
echo ok> .venv\ready.txt
rem Optional: lets the visualizer follow the computer's sound. The display works without it.
.venv\Scripts\python -m pip install -q --disable-pip-version-check -r requirements-visualizer.txt >nul 2>&1
.venv\Scripts\python desk_display.py %*
pause
exit /b
:nopython
echo Python was not found. Install Python 3.11 or newer from python.org, then run this again.
pause
exit /b
:failed
echo Setup failed. Check your internet connection, then run this again.
pause
