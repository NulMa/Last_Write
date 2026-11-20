@echo off
rem 제작 수기 GIF 용 입력 계획기를 빌드한다 (제출물 아님)
setlocal
set ROOT=%~dp0..\
where cl >nul 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%ROOT%build\rec" mkdir "%ROOT%build\rec"
cl /nologo /O2 /utf-8 -D_CRT_SECURE_NO_WARNINGS "%ROOT%tools\rec-plan.c" "%ROOT%src\sim.c" "%ROOT%src\levels.c" /Fe:"%ROOT%build\rec-plan.exe" /Fo:"%ROOT%build\rec\\"
