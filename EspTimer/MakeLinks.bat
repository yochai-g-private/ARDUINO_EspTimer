@echo off

set ProjectDir=%~dp0
set Inc=%ProjectDir%include\
set Lib=%ProjectDir%lib\

set Libraries=..\..\..\Libraries\

cd %ProjectDir%

::============================================
:: Create links
::============================================

call :Link_LIB              TimeLib             .
rem call :Link_LIB              DS3231              .
rem call :Link_LIB              NTPClient           .
call :Link_LIB              ESPAsyncWebServer   src
call :Link_LIB              ESPAsyncTCP         src

call :Link_LIB              NYG

call :Link_NYG              NYG
call :Link_NYG              Defines
call :Link_NYG              Enums
call :Link_NYG              Types
call :Link_NYG              SystemFacilities
call :Link_NYG              Logger

call :Link_NYG              Html
call :Link_NYG              IOutput
call :Link_NYG              Observer
rem call :Link_NYG              TimeEx
rem call :Link_NYG              RTC
rem call :Link_NYG              Location
rem call :Link_NYG              Scheduler
call :Link_NYG              WiFiUtils
call :Link_NYG              AsyncWebServerEx
rem call :Link_NYG              AnalogClock

pause

goto :eof


::---------------------------
:Link_LIB
::---------------------------
if exist %Lib%%1        rmdir  /s/q %Lib%%1
if exist %Inc%%1        rmdir  /s/q %Inc%%1

if not exist %Lib%      mkdir  %Lib%

mklink /J %Lib%%1       %Libraries%%1
mkdir     %Inc%%1

if "%2"==""         goto :eof

set SOURCE_DIR=%Libraries%%1\
set TARGET_DIR=%Inc%%1\

if not "%2"=="."    (
    set SOURCE_DIR=%SOURCE_DIR%%2\
    set TARGET_DIR=%TARGET_DIR%%2\
    )

if not exist %TARGET_DIR%   mkdir %TARGET_DIR%

for %%a in (%SOURCE_DIR%*.h) do (
    call :Link_H %%a
    )

goto :eof

::---------------------------
:Link_H
::---------------------------
set SOURCE=%1

For %%A in ("%SOURCE%") do (
    Set TARGET=%TARGET_DIR%%%~nxA
)

mklink /H %TARGET%  %SOURCE%

goto :eof

::---------------------------
:Link_NYG
::---------------------------
::echo mklink /H %Inc%NYG\%1.h   %Libraries%NYG\%1.h
::dir ..\..\..\
::mklink /H %Inc%NYG\%1.h   %Libraries%NYG\%1.h
mklink %Inc%NYG\%1.h   %Libraries%NYG\%1.h

goto :eof

::---------------------------
:MkLink_INC
::---------------------------
call :MkLink    %Inc%%1     %2
goto :eof

::---------------------------
:MkLink
::---------------------------
if exist %Libraries%%2   (
    mklink %1   %Libraries%%2
    ) else (
    echo %Libraries%%2 not found.
    )
goto :eof

