@echo off
setlocal EnableDelayedExpansion

REM output file
set OUTFILE=all_code_combined.cpp

REM remove existing output if needed
if exist "%OUTFILE%" del "%OUTFILE%"

REM recursively find .cpp and .h files under src and include directories
for /R "Render" %%F in (*.cpp *.h) do (
    echo // ----- File: %%~fF ----- >> "%OUTFILE%"
    type "%%~fF" >> "%OUTFILE%"
    echo. >> "%OUTFILE%"
)

echo Done. Combined into %OUTFILE%.