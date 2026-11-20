@echo off
rem 녹화 모드(--rec)가 켜진 exe 를 따로 빌드한다 → build\floppy_rec.exe
rem 제출물이 아니다. 평소 build.bat 의 exe 크기에는 영향이 없다.
rem   floppy_rec.exe --rec <level> <script> <outdir> <loadout-mask>
rem   script 는 tools\rec-plan.exe 가 만든다
setlocal
set ROOT=%~dp0..\
set SRC=%ROOT%vendor\raylib-5.5\src
set RLIB=%ROOT%vendor\raylib-mt\raylib_mt.lib
where cl >nul 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%ROOT%build\rec_obj" mkdir "%ROOT%build\rec_obj"
cl /nologo /O2 /MT /W3 /utf-8 -D_CRT_SECURE_NO_WARNINGS -DLASTWRITE_REC /I"%SRC%" ^
   "%ROOT%src\main.c" "%ROOT%src\sim.c" "%ROOT%src\levels.c" "%ROOT%src\progress.c" "%ROOT%src\sfx.c" "%ROOT%src\bgm.c" ^
   /Fe:"%ROOT%build\floppy_rec.exe" /Fo:"%ROOT%build\rec_obj\\" ^
   /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup ^
   "%RLIB%" opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib
