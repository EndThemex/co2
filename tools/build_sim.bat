@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"
cl /nologo /std:c++latest /permissive- /EHsc /O2 /W3 /utf-8 /I"..\include" ui_sim.cpp canvas_console.cpp ..\src\ui_pages.cpp ..\src\ui_face.cpp /Fe:ui_sim.exe /Fo:"obj\\"
del /q ui_sim.obj canvas_console.obj ui_pages.obj ui_face.obj 2>nul
rd /s /q obj 2>nul
mkdir obj 2>nul
cl /nologo /std:c++latest /permissive- /EHsc /O2 /W3 /utf-8 /I"..\include" /Fo"obj\\" /c ui_sim.cpp canvas_console.cpp ..\src\ui_pages.cpp ..\src\ui_face.cpp
if errorlevel 1 exit /b 1
link /nologo /out:ui_sim.exe obj\ui_sim.obj obj\canvas_console.obj obj\ui_pages.obj obj\ui_face.obj
exit /b %ERRORLEVEL%