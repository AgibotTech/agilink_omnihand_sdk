@echo off
REM Omnihand 2025 SDK Windows Install Script
setlocal enabledelayedexpansion
call :install_sdk %*
set "INSTALL_EXIT_CODE=!errorlevel!"
endlocal & exit /b %INSTALL_EXIT_CODE%

:install_sdk

set "SCRIPT_DIR=%~dp0"
set "INSTALL_DIR=C:\omnihand"

REM Check administrator privileges, request elevation if not available
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] Requesting administrator privileges...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ============================================
echo Omnihand 2025 SDK - Windows Installer
echo ============================================
echo.
echo Installing to: %INSTALL_DIR%
echo.

REM Install C++ SDK
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
for %%d in (include lib bin) do (
    xcopy /E /I /Y "%SCRIPT_DIR%cpp\%%d" "%INSTALL_DIR%\%%d" >nul
    if errorlevel 1 (
        echo [FAIL] Could not install cpp\%%d to %INSTALL_DIR%\%%d
        exit /b 1
    )
)
xcopy /E /I /Y "%SCRIPT_DIR%frontend" "%INSTALL_DIR%\share\omnihand\frontend" >nul
if errorlevel 1 (
    echo [FAIL] Could not install frontend. Check that the release package contains frontend\index.html.
    exit /b 1
)
if exist "%SCRIPT_DIR%cpp\share\cmake\omnihand" (
    xcopy /E /I /Y "%SCRIPT_DIR%cpp\share\cmake\omnihand" "%INSTALL_DIR%\share\cmake\omnihand" >nul
    if errorlevel 1 (
        echo [FAIL] Could not install the CMake package.
        exit /b 1
    )
)
if exist "%SCRIPT_DIR%cpp\cmake" (
    xcopy /E /I /Y "%SCRIPT_DIR%cpp\cmake" "%INSTALL_DIR%\cmake" >nul
    if errorlevel 1 (
        echo [FAIL] Could not install CMake helper files.
        exit /b 1
    )
)
echo [OK] C++ SDK installed to %INSTALL_DIR%
echo.

REM Detect Python and install matching wheel
echo Installing Python package...
set "PYTHON_CMD="

REM Priority 1: Check if running inside an active conda/venv environment
if defined CONDA_PREFIX (
    if exist "!CONDA_PREFIX!\python.exe" (
        set "PYTHON_CMD=!CONDA_PREFIX!\python.exe"
        echo [INFO] Detected active Conda environment: !CONDA_PREFIX!
    )
)
if defined VIRTUAL_ENV (
    if not defined PYTHON_CMD (
        if exist "!VIRTUAL_ENV!\Scripts\python.exe" (
            set "PYTHON_CMD=!VIRTUAL_ENV!\Scripts\python.exe"
            echo [INFO] Detected active virtual environment: !VIRTUAL_ENV!
        )
    )
)

REM Priority 2: System Python via PATH (skip conda base python if not in active env)
if not defined PYTHON_CMD (
    for /f "delims=" %%p in ('where python 2^>nul') do (
        if not defined PYTHON_CMD (
            set "_PY_PATH=%%p"
            echo !_PY_PATH! | findstr /I /C:"conda" >nul 2>&1
            if !errorlevel! neq 0 (
                set "PYTHON_CMD=%%p"
            ) else (
                if defined CONDA_PREFIX (
                    set "PYTHON_CMD=%%p"
                )
            )
        )
    )
)

REM Priority 3: Well-known install locations
if not defined PYTHON_CMD (
    if exist "%LOCALAPPDATA%\Programs\Python\Python313\python.exe" set "PYTHON_CMD=%LOCALAPPDATA%\Programs\Python\Python313\python.exe"
)
if not defined PYTHON_CMD (
    if exist "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" set "PYTHON_CMD=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
)
if not defined PYTHON_CMD (
    if exist "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" set "PYTHON_CMD=%LOCALAPPDATA%\Programs\Python\Python311\python.exe"
)
if not defined PYTHON_CMD (
    if exist "%LOCALAPPDATA%\Programs\Python\Python310\python.exe" set "PYTHON_CMD=%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
)
if not defined PYTHON_CMD (
    if exist "C:\Python313\python.exe" set "PYTHON_CMD=C:\Python313\python.exe"
)
if not defined PYTHON_CMD (
    if exist "C:\Python312\python.exe" set "PYTHON_CMD=C:\Python312\python.exe"
)
if not defined PYTHON_CMD (
    if exist "C:\Python311\python.exe" set "PYTHON_CMD=C:\Python311\python.exe"
)
if not defined PYTHON_CMD (
    if exist "C:\Python310\python.exe" set "PYTHON_CMD=C:\Python310\python.exe"
)

if defined PYTHON_CMD (
    echo Using Python: !PYTHON_CMD!

    REM Detect environment type (conda/venv/system) for pip args
    set "PIP_EXTRA_ARGS="
    REM Read a file instead of nesting a quoted Python command inside CMD /C.
    set "PY_ENV_TYPE="
    set "PY_VER="
    set "PYTHON_INFO_FILE=%TEMP%\omnihand-install-python-!RANDOM!-!RANDOM!.txt"
    "!PYTHON_CMD!" -c "import sys,os; in_conda=bool(os.environ.get('CONDA_PREFIX')) or 'conda' in sys.executable.lower(); in_venv=bool(os.environ.get('VIRTUAL_ENV')) or not (sys.prefix == sys.base_prefix); print('ENV:'+('conda' if in_conda else ('venv' if in_venv else 'system'))); print('VER:'+str(sys.version_info.major)+str(sys.version_info.minor))" > "!PYTHON_INFO_FILE!"
    if errorlevel 1 (
        if exist "!PYTHON_INFO_FILE!" del /q "!PYTHON_INFO_FILE!"
        echo [FAIL] Could not query Python: !PYTHON_CMD!
        exit /b 1
    )
    for /f "usebackq tokens=1,* delims=:" %%k in ("!PYTHON_INFO_FILE!") do (
        if "%%k"=="ENV" set "PY_ENV_TYPE=%%l"
        if "%%k"=="VER" set "PY_VER=%%l"
    )
    del /q "!PYTHON_INFO_FILE!"
    if not defined PY_ENV_TYPE (
        echo [FAIL] Could not detect the Python environment: !PYTHON_CMD!
        exit /b 1
    )
    if "!PY_ENV_TYPE!"=="system" (
        echo [INFO] System Python detected - installing with --user
        set "PIP_EXTRA_ARGS=--user"
    ) else (
        echo [INFO] !PY_ENV_TYPE! Python detected - installing to environment
    )

    if defined PY_VER (
        set "WHL_MATCH="
        set "INSTALLED="
        for %%f in ("%SCRIPT_DIR%python\*-cp!PY_VER!-cp!PY_VER!-*.whl") do if exist "%%f" (
            set "WHL_MATCH=1"
            echo Installing %%~nxf ^(Python !PY_VER!^) ...
            "!PYTHON_CMD!" -m pip install "%%f" --force-reinstall --no-deps !PIP_EXTRA_ARGS! --quiet
            if !errorlevel! equ 0 (
                set "INSTALLED=%%~nxf"
                echo [OK] Installed: %%~nxf
            ) else (
                echo [FAIL] %%~nxf
                exit /b 1
            )
        )
        if defined WHL_MATCH (
            echo [OK] Python package installed for Python !PY_VER!
        ) else (
            echo [WARN] No wheel for Python !PY_VER! - trying all wheels...
            set "INSTALLED="
            for %%f in ("%SCRIPT_DIR%python\*.whl") do if exist "%%f" (
                if not defined INSTALLED (
                    "!PYTHON_CMD!" -m pip install "%%f" --force-reinstall --no-deps !PIP_EXTRA_ARGS! --quiet 2>nul
                    if !errorlevel! equ 0 set "INSTALLED=%%~nxf"
                )
            )
            if defined INSTALLED (echo [OK] Installed: !INSTALLED!) else (
                echo [FAIL] No compatible wheel found
                exit /b 1
            )
        )
    ) else (
        echo [WARN] Could not detect Python version - trying each wheel until one succeeds...
        set "INSTALLED="
        for %%f in ("%SCRIPT_DIR%python\*.whl") do if exist "%%f" (
            if not defined INSTALLED (
                echo Trying %%~nxf ...
                "!PYTHON_CMD!" -m pip install "%%f" --force-reinstall --no-deps !PIP_EXTRA_ARGS! --quiet 2>nul
                if !errorlevel! equ 0 set "INSTALLED=%%~nxf"
            )
        )
        if defined INSTALLED (echo [OK] Installed: !INSTALLED!) else (
            echo [FAIL] No compatible wheel found
            exit /b 1
        )
    )
) else (
    echo [SKIP] Python not found. Install Python 3.10+ and run:
    echo        pip install "%SCRIPT_DIR%python\omnihand-*.whl" --force-reinstall --no-deps
)
echo.

REM Add C:\omnihand\bin to system PATH
powershell -NoProfile -Command "$p = [Environment]::GetEnvironmentVariable('Path', 'Machine'); if ($p -notlike '*C:\omnihand\bin*') { [Environment]::SetEnvironmentVariable('Path', $p.TrimEnd(';') + ';C:\omnihand\bin', 'Machine'); Write-Host '[OK] Added C:\omnihand\bin to system PATH' } else { Write-Host '[OK] C:\omnihand\bin already in PATH' }"
echo.

REM Register the SDK prefix for CMake package discovery
powershell -NoProfile -Command "$entry = 'C:\omnihand'; $p = [string][Environment]::GetEnvironmentVariable('CMAKE_PREFIX_PATH', 'Machine'); $items = @($p -split ';' | Where-Object { $_ }); if ($items -notcontains $entry) { $new = (@($items) + $entry) -join ';'; [Environment]::SetEnvironmentVariable('CMAKE_PREFIX_PATH', $new, 'Machine'); Write-Host '[OK] Added C:\omnihand to system CMAKE_PREFIX_PATH' } else { Write-Host '[OK] C:\omnihand already in CMAKE_PREFIX_PATH' }"
echo.

if exist "%SCRIPT_DIR%ros2" (
    echo [INFO] ROS2 packages available at: %SCRIPT_DIR%ros2
)

echo ============================================
echo Installation Complete
echo ============================================
echo.
echo C++ SDK: %INSTALL_DIR%
echo PATH: C:\omnihand\bin has been added to system environment.
echo CMAKE_PREFIX_PATH: C:\omnihand has been added to system environment.
echo Restart the terminal or IDE before running CMake.
echo.
pause
exit /b 0
