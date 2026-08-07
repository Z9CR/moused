$files = @(Get-ChildItem .\src -Filter "*.cpp" -File) +
         @(Get-ChildItem .\include -Filter "*.hpp" -File);

clang-format.exe -i $files.FullName