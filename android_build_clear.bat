cd src\android\app\build\outputs\apk\
del /Q /F *.*
for /D %%i in (*) do rd /S /Q "%%i"