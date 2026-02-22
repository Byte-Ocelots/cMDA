@echo off
REM tests\test_cli.bat — Windows CLI smoke tests for md2, md4, md5
REM Run from repo root after: make all
setlocal enabledelayedexpansion
set PASS=0
set FAIL=0
set BIN=bin

REM Helper: run binary, capture hash word (3rd token)
REM ------ MD5 "abc" -------------------------------------------------------
for /f "tokens=3" %%H in ('%BIN%\md5.exe "abc"') do set HASH=%%H
if /i "!HASH!"=="900150983cd24fb0d6963f7d28e17f72" (
    echo PASS  md5^(abc^) = !HASH!
    set /a PASS+=1
) else (
    echo FAIL  md5^(abc^): got !HASH!
    set /a FAIL+=1
)

REM ------ MD5 --format upper -----------------------------------------------
for /f "tokens=3" %%H in ('%BIN%\md5.exe --format upper "abc"') do set HASH=%%H
if /i "!HASH!"=="900150983CD24FB0D6963F7D28E17F72" (
    echo PASS  md5 upper
    set /a PASS+=1
) else (
    echo FAIL  md5 upper: !HASH!
    set /a FAIL+=1
)

REM ------ MD5 --compare match (exit 0) --------------------------------------
"%BIN%\md5" --compare "900150983cd24fb0d6963f7d28e17f72" "abc" >nul 2>&1
if !ERRORLEVEL!==0 (
    echo PASS  md5 --compare match ^(exit 0^)
    set /a PASS+=1
) else (
    echo FAIL  md5 --compare match ^(exit !ERRORLEVEL!^)
    set /a FAIL+=1
)

REM ------ MD5 --compare mismatch (exit 1) ------------------------------------
"%BIN%\md5" --compare "000000000000000000000000000000ff" "abc" >nul 2>&1
if !ERRORLEVEL!==1 (
    echo PASS  md5 --compare mismatch ^(exit 1^)
    set /a PASS+=1
) else (
    echo FAIL  md5 --compare mismatch ^(exit !ERRORLEVEL!^, want 1^)
    set /a FAIL+=1
)

REM ------ MD4 "a" -----------------------------------------------------------
for /f "tokens=3" %%H in ('%BIN%\md4.exe "a"') do set HASH=%%H
if /i "!HASH!"=="bde52cb31de33e46245e05fbdbd6fb24" (
    echo PASS  md4^(a^)
    set /a PASS+=1
) else (
    echo FAIL  md4^(a^): !HASH!
    set /a FAIL+=1
)

REM ------ MD2 "a" -----------------------------------------------------------
for /f "tokens=3" %%H in ('%BIN%\md2.exe "a"') do set HASH=%%H
if /i "!HASH!"=="32ec01ec4a6dac72c0ab96fb34c0b5d1" (
    echo PASS  md2^(a^)
    set /a PASS+=1
) else (
    echo FAIL  md2^(a^): !HASH!
    set /a FAIL+=1
)

REM ------ Summary -----------------------------------------------------------
echo.
echo CLI tests: %PASS% passed, %FAIL% failed.
if %FAIL% GTR 0 exit /b 1
exit /b 0
