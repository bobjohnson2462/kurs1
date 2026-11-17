@echo off
chcp 65001 >nul
setlocal

set GXX=C:\msys64\mingw64\bin\g++.exe
if not exist "%GXX%" (
    echo Не нашёл g++ по пути %GXX%.
    echo Поправьте переменную GXX в начале этого файла.
    exit /b 1
)

if not exist build mkdir build

echo Собираю приложение...
"%GXX%" -std=c++17 -O2 -Wall -Wextra -Wpedantic -static -Iinclude -Isrc ^
    src\model.cpp src\money.cpp src\depreciation.cpp src\service.cpp src\report.cpp ^
    src\sqlserver_store.cpp src\main.cpp ^
    src\ui\input.cpp src\ui\reports.cpp src\ui\menu.cpp ^
    -o build\osmod.exe -lodbc32
if errorlevel 1 exit /b 1

echo Собираю тесты...
"%GXX%" -std=c++17 -O2 -Wall -Wextra -Wpedantic -static -Iinclude -Isrc ^
    src\model.cpp src\money.cpp src\depreciation.cpp src\service.cpp src\report.cpp ^
    src\sqlserver_store.cpp tests\test_main.cpp ^
    -o build\osmod_tests.exe -lodbc32
if errorlevel 1 exit /b 1

echo.
echo Готово:
echo   build\osmod.exe        запуск меню
echo   build\osmod.exe --demo учебный сценарий
echo   build\osmod_tests.exe  проверки
