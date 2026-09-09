@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal DisableDelayedExpansion
if "%~1"=="" goto usage
if not "%~2"=="" goto usage
py -3 -c "import sys; sys.exit(sys.version_info < (3, 12))" >nul 2>&1
if not errorlevel 1 goto py
python -c "import sys; sys.exit(sys.version_info < (3, 12))" >nul 2>&1
if not errorlevel 1 goto python
echo Install Python 3.12 or newer from https://www.python.org/downloads/windows/
echo Enable the Python launcher or add Python to PATH, then try again.
pause
exit /b 1
:py
py -3 "%~dp0tools\extract_iso.py" "%~f1"
goto done
:python
python "%~dp0tools\extract_iso.py" "%~f1"
:done
set "result=%errorlevel%"
echo.
pause
exit /b %result%
:usage
echo Drag one Xbox ISO onto ExtractIso.cmd.
echo Extract the entire download first. Python 3.12 or newer is required.
pause
exit /b 1
