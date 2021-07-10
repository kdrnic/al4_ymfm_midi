@echo off
call ../setenv.bat
mingw32-make regular
set SUCCESS=%ERRORLEVEL%
echo.%cmdcmdline% | find /I "%~0" >nul
if not errorlevel 1 pause
@echo on
