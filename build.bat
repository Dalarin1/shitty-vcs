@echo off

if "%~1"=="" (
    rem -O2         некоторый оптимизон
    rem -std=c++17  версия с++ (ниже нельзя из-за std::filesystem)
    rem -L          директория с .lib и с .dll файлами
    rem -lzlib1     линковать zlib1.dll
    
    call g++.exe ^
        -Wall ^
        -I include ^
        -I include\zlib ^
        -O0 ^
        -std=c++17 ^
        -L include\zlib ^
        -lzlib1 ^
        vcshit.cpp ^
        -o main.exe ^
        -g
) else (
    call g++.exe ^
        -Wall ^
        -I include ^
        -I include\zlib ^ 
        -O2 ^
        -std=c++17 ^
        -L include\zlib ^
        -lzlib1 ^
        vcshit.cpp ^
        -o "%~1"
)
