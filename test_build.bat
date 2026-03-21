call "\VC\Auxiliary\Build\vcvars64.bat"
"C:\Program Files\CMake\bin\cmake.exe" -S . -B build_x64 -G "Visual Studio 17 2022" -A x64
"C:\Program Files\CMake\bin\cmake.exe" --build build_x64 --config RelWithDebInfo
