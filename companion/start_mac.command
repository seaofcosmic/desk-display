#!/bin/bash
cd "$(dirname "$0")"
if [ ! -d .venv ]; then
  echo "First run: setting up. This takes a minute."
  python3 -m venv .venv || { echo "Python 3 was not found. Install it from python.org, then run this again."; read -r; exit 1; }
fi
if .venv/bin/python -m pip install -q --disable-pip-version-check -r requirements.txt; then
  touch .venv/ready.txt
elif [ ! -f .venv/ready.txt ]; then
  echo "Setup failed. Check your internet connection, then run this again."; read -r; exit 1
fi
.venv/bin/python desk_display.py "$@"
