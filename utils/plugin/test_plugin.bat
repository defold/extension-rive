@echo off
setlocal EnableExtensions

set "LIBNAME=RiveExt"
set "CLASS_NAME=com.dynamo.bob.pipeline.Rive"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
for %%I in ("%SCRIPT_DIR%\..\..") do set "REPO_ROOT=%%~fI"
set "JAR=%REPO_ROOT%\defold-rive\plugins\share\plugin%LIBNAME%.jar"

set "PLUGIN_PLATFORM=x86_64-win32"
set "PLUGIN_PLATFORM_DIR=%REPO_ROOT%\defold-rive\plugins\lib\%PLUGIN_PLATFORM%"

if not exist "%JAR%" (
    echo Plugin jar not found: %JAR% 1>&2
    exit /b 1
)

if not defined BOB (
    if defined DYNAMO_HOME (
        set "BOB=%DYNAMO_HOME%\share\java\bob.jar"
    ) else (
        echo BOB is not set. Set BOB to your bob.jar path or set DYNAMO_HOME to the Defold SDK root. 1>&2
        exit /b 1
    )
)

if not exist "%BOB%" (
    echo bob.jar not found: %BOB% 1>&2
    exit /b 1
)

if not exist "%PLUGIN_PLATFORM_DIR%" (
    echo Plugin library directory not found: %PLUGIN_PLATFORM_DIR% 1>&2
    exit /b 1
)

where.exe java.exe >nul 2>nul
if errorlevel 1 (
    echo java.exe not found in PATH 1>&2
    exit /b 1
)

echo BOB=%BOB%
echo JAR=%JAR%
echo CLASS_NAME=%CLASS_NAME%
echo LIBNAME=%LIBNAME%

set "JNI_DEBUG_FLAGS=-Xcheck:jni"

set "DM_RIVE_LOG_LEVEL=DEBUG"
echo libjsig not found; skipping preload

set "DUMP_FILE=%REPO_ROOT%\build\rive_signal_dump.log"
if not exist "%REPO_ROOT%\build" (
    mkdir "%REPO_ROOT%\build"
)
if exist "%DUMP_FILE%" (
    del /f /q "%DUMP_FILE%" >nul 2>nul
)
set "DM_RIVE_SIGNAL_DUMP_PATH=%DUMP_FILE%"

set "JAVA_PLUGIN_PLATFORM_DIR=%PLUGIN_PLATFORM_DIR%"
set "JAVA_JAR=%JAR%"
set "JAVA_BOB=%BOB%"
set "JAVA_CLASSPATH=%JAVA_JAR%;%JAVA_BOB%"

java.exe %JNI_DEBUG_FLAGS% -Djava.library.path="%JAVA_PLUGIN_PLATFORM_DIR%" -Djni.library.path="%JAVA_PLUGIN_PLATFORM_DIR%" -cp "%JAVA_CLASSPATH%" %CLASS_NAME% %*
exit /b %ERRORLEVEL%
