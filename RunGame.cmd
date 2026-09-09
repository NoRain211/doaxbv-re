@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal DisableDelayedExpansion
py -3 -c "import sys; sys.exit(sys.version_info < (3, 12))" >nul 2>&1
if not errorlevel 1 goto py
python -c "import sys; sys.exit(sys.version_info < (3, 12))" >nul 2>&1
if not errorlevel 1 goto python
echo Install Python 3.12 or newer from https://www.python.org/downloads/windows/
echo Enable the Python launcher or add Python to PATH, then try again.
pause
exit /b 1
:py
py -3 "%~dp0tools\run_game.py"
goto done
:python
python "%~dp0tools\run_game.py"
:done
set "result=%errorlevel%"
echo.
pause
exit /b %result%
