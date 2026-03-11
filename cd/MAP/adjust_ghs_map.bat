@echo off
setlocal enabledelayedexpansion

:: adjust_ghs_map.bat - normalize GHS.MAP by subtracting the minimum X and Y
:: Usage: place this .bat in the same folder as GHS.MAP and run it.

:: Path to map file (same folder as this script)
set "MAP=%~dp0GHS.MAP"
if not exist "%MAP%" (
    echo File not found: %MAP%
    exit /b 1
)

:: Backup original
copy "%MAP%" "%MAP%.bak" >nul

:: Find minimum X and Y (columns 2 and 3)
set "minX="
set "minY="

for /f "usebackq tokens=1,2,3*" %%A in ("%MAP%") do (
    rem %%A=col1 (name), %%B=col2 (X), %%C=col3 (Y), %%D=rest of line (optional)
    set "x=%%B"
    set "y=%%C"

    if defined x if defined y (
        if not defined minX (
            rem initialize minima from first valid numeric line
            set "minX=!x!"
            set "minY=!y!"
        ) else (
            rem update minima if a smaller value is found
            if !x! LSS !minX! set "minX=!x!"
            if !y! LSS !minY! set "minY=!y!"
        )
    )
)

if not defined minX (
    echo No numeric X/Y found in %MAP%
    exit /b 1
)

echo Found minima: X=!minX! Y=!minY!

:: Prepare output temporary file (overwrite if exists)
set "OUT=%MAP%.tmp"
break > "%OUT%" 2>nul

:: Second pass: normalize each line and write to temp file
for /f "usebackq tokens=1,2,3*" %%A in ("%MAP%") do (
    set "col1=%%A"
    set "col2=%%B"
    set "col3=%%C"
    set "rest=%%D"

    if defined col2 if defined col3 (
        rem arithmetic expansion: newX = col2 - minX, newY = col3 - minY
        set /a newX=col2 - minX
        set /a newY=col3 - minY

        if defined rest (
            >> "%OUT%" echo !col1!	!newX!	!newY!	!rest!
        ) else (
            >> "%OUT%" echo !col1!	!newX!	!newY!
        )
    ) else (
        rem Fallback: preserve the parsed tokens as-is (best-effort for irregular lines)
        >> "%OUT%" echo %%A %%B %%C %%D
    )
)

:: Replace original file with normalized file
move /y "%OUT%" "%MAP%" >nul
echo Normalization complete. Backup at %MAP%.bak

endlocal
exit /b 0
+