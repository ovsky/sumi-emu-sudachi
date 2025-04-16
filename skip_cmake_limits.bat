@echo off
setlocal enabledelayedexpansion

echo Searching for CMakeLists.txt files recursively...

set "target_major=3"
set "target_minor=10"
set "new_version_line=cmake_minimum_required(VERSION 3.10)"

REM Loop through all CMakeLists.txt files starting from the current directory (.)
for /R . %%F in (CMakeLists.txt) do (
    echo Processing: "%%F"

    REM Use PowerShell to read, check, modify, and write back the file content
    powershell -Command ^
        $filePath = '%%F'; ^
        $targetMajor = %target_major%; ^
        $targetMinor = %target_minor%; ^
        $newVersionLine = '%new_version_line%'; ^
        $content = Get-Content -Path $filePath -Raw; ^
        $needsUpdate = $false; ^
        $newContent = $content -replace '(?im)^\s*cmake_minimum_required\s*\(\s*VERSION\s+(\d+)\.(\d+)(\.\d+)?(\.\d+)?\s*\)', { ^
            param($match) ^
            $major = [int]$match.Groups[1].Value; ^
            $minor = [int]$match.Groups[2].Value; ^
            if (($major -lt $targetMajor) -or ($major -eq $targetMajor -and $minor -lt $targetMinor)) { ^
                Write-Host "  Found version $major.$minor. Updating to 3.10." -ForegroundColor Yellow; ^
                $script:needsUpdate = $true; ^
                $newVersionLine; ^
            } else { ^
                Write-Host "  Found version $major.$minor. No update needed."; ^
                $match.Value; ^
            } ^
        }; ^
        if ($script:needsUpdate) { ^
            try { ^
                Set-Content -Path $filePath -Value $newContent -Encoding UTF8 -ErrorAction Stop; ^
                Write-Host "  Successfully updated: $filePath" -ForegroundColor Green; ^
            } catch { ^
                Write-Host "  ERROR updating file: $filePath - $($_.Exception.Message)" -ForegroundColor Red; ^
            } ^
        }

    REM Check if PowerShell command succeeded (optional, basic check)
    if errorlevel 1 (
      echo   ERROR processing file "%%F" with PowerShell.
    )

)

echo.
echo Script finished.
endlocal
pause