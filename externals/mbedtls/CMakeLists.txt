@echo off
setlocal enabledelayedexpansion

echo Searching for CMakeLists.txt files recursively...

set "target_major=3"
set "target_minor=10"
set "new_target_version=3.10"

REM Loop through all CMakeLists.txt files starting from the current directory (.)
for /R . %%F in (CMakeLists.txt) do (
    echo Processing: "%%F"

    REM Use PowerShell to read, check, modify (preserving suffixes), and write back the file content
    powershell -Command ^
        $filePath = '%%F'; ^
        $targetMajor = %target_major%; ^
        $targetMinor = %target_minor%; ^
        $newTargetVersion = '%new_target_version%'; ^
        $content = Get-Content -Path $filePath -Raw; ^
        $needsUpdate = $false; ^
        $regex = '(?im)^\s*cmake_minimum_required\s*\(\s*VERSION\s+(\d+)\.(\d+)(\.\d+)?(\.\d+)?(\s+.*?)?\s*\)'; ^
        $newContent = $content -replace $regex, { ^
            param($match) ^
            $major = [int]$match.Groups[1].Value; ^
            $minor = [int]$match.Groups[2].Value; ^
            $patch = $match.Groups[3].Value;  ^ REM Optional .patch (e.g., .1)
            $tweak = $match.Groups[4].Value;  ^ REM Optional .tweak (e.g., .2)
            $suffix = $match.Groups[5].Value; ^ REM Optional suffix (e.g., " FATAL_ERROR") - includes leading space if present
            
            $originalVersionForMessage = "$major.$minor$patch$tweak" ^
            
            if (($major -lt $targetMajor) -or ($major -eq $targetMajor -and $minor -lt $targetMinor)) { ^
                # Construct the replacement line, preserving the original suffix
                # Ensure suffix keeps its leading space if it had one, or add one if needed and suffix exists but had no space
                $suffix = $suffix.TrimStart(); ^ REM Remove potential leading space captured by (\s+.*?)?
                if (-not [string]::IsNullOrWhiteSpace($suffix)) { $suffix = ' ' + $suffix }; ^ REM Add exactly one leading space if suffix exists
                
                $newLine = "cmake_minimum_required(VERSION $newTargetVersion" + $suffix + ")" ^
                Write-Host ('  Found version {0}. Updating to {1} (preserving suffix: "{2}")' -f $originalVersionForMessage, $newTargetVersion, $suffix.TrimStart()) -ForegroundColor Yellow; ^
                $script:needsUpdate = $true; ^
                $newLine; # Return the newly constructed line for replacement
            } else { ^
                Write-Host ('  Found version {0}. No update needed.' -f $originalVersionForMessage); ^
                $match.Value; # Return the original matched text
            } ^
        }; ^
        if ($script:needsUpdate) { ^
            try { ^
                # Use UTF8 without BOM for compatibility, unless original file had BOM
                $encoding = (Get-Content -Path $filePath -Encoding Byte -ReadCount 0 -TotalCount 0).Preamble.Count -gt 0 ? 'utf8' : 'utf8NoBOM'; ^
                Set-Content -Path $filePath -Value $newContent -Encoding $encoding -ErrorAction Stop; ^
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