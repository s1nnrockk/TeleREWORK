@echo OFF

setlocal enabledelayedexpansion
set "FullScriptPath=%~dp0"

rem Restore generated Rust wallet C++ binding after CI cache cleanup.
if defined LibrariesPath if exist "%LibrariesPath%\tdesktop_rust\out\include\wallet_engine\wallet_engine.cpp" (
    if not exist "%LibrariesPath%\tdesktop_rust\out\src\wallet_engine" mkdir "%LibrariesPath%\tdesktop_rust\out\src\wallet_engine"
    copy /Y "%LibrariesPath%\tdesktop_rust\out\include\wallet_engine\wallet_engine.cpp" "%LibrariesPath%\tdesktop_rust\out\src\wallet_engine\wallet_engine.cpp" >nul
    if errorlevel 1 goto error
)

python %FullScriptPath%configure.py %*
if %errorlevel% neq 0 goto error

exit /b

:error
echo FAILED
exit /b 1
