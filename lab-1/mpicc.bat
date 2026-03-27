@echo off
REM Простая обёртка "mpicc" для g++ и MS-MPI
REM Использование из этой папки:
REM   .\mpicc.bat main.cpp -o main.exe

g++ %* ^
  -I"C:/Program Files (x86)/Microsoft SDKs/MPI/Include" ^
  -L"C:/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64" ^
  -lmsmpi

