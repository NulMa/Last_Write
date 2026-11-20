@echo off
rem 제작 수기에 넣을 웹 빌드 → devlog\play\
rem 제출물이 아니다 (공모전 규정상 제출은 네이티브 exe). 소개용 사본이다.
rem emsdk 위치는 EMSDK 환경변수로 바꿀 수 있다
setlocal
set ROOT=%~dp0..\
if not defined EMSDK set EMSDK=C:\Unity\tools\emsdk
call "%EMSDK%\emsdk_env.bat" >nul 2>nul
set SRC=%ROOT%vendor\raylib-5.5\src
set OBJ=%ROOT%build\web
set LIB=%OBJ%\libraylib.a
if not exist "%OBJ%" mkdir "%OBJ%"

if exist "%LIB%" goto game
echo [web] raylib 을 웹용으로 빌드한다 (처음 한 번만)
for %%F in (rcore rshapes rtextures rtext rmodels raudio utils) do (
    call emcc -c "%SRC%\%%F.c" -o "%OBJ%\%%F.o" -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 || exit /b 1
)
call emar rcs "%LIB%" "%OBJ%\rcore.o" "%OBJ%\rshapes.o" "%OBJ%\rtextures.o" "%OBJ%\rtext.o" "%OBJ%\rmodels.o" "%OBJ%\raudio.o" "%OBJ%\utils.o" || exit /b 1

:game
if not exist "%ROOT%devlog\play" mkdir "%ROOT%devlog\play"
call emcc -Os -DPLATFORM_WEB -I"%SRC%" ^
   "%ROOT%src\main.c" "%ROOT%src\sim.c" "%ROOT%src\levels.c" "%ROOT%src\progress.c" "%ROOT%src\sfx.c" "%ROOT%src\bgm.c" ^
   "%LIB%" -sUSE_GLFW=3 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 ^
   -sEXPORTED_RUNTIME_METHODS=HEAPF32 ^
   --shell-file "%ROOT%tools\web\shell.html" -o "%ROOT%devlog\play\index.html" || exit /b 1
rem 캐시 무효화: index.js · index.wasm 주소에 빌드 시각을 붙인다
powershell -NoProfile -Command "$f='%ROOT%devlog\play\index.html'; $v=Get-Date -Format yyyyMMddHHmmss; $t=[IO.File]::ReadAllText($f); $t=$t.Replace('__VER__',$v).Replace('src=index.js>','src=index.js?v='+$v+'>').Replace('src=\"index.js\"','src=\"index.js?v='+$v+'\"'); [IO.File]::WriteAllText($f,$t)" || exit /b 1
echo [web] devlog\play\index.html
