$root = Resolve-Path "$PSScriptRoot\.."
try {
    cmake -B "$root\build"
    cmake --build "$root\build"
    Write-Host 'compile work done'
    & "$root\build\Debug\moused.exe" 2>&1
    # 'cause wxWidget's start func isnt `main` but `WinMain` instead, 
    # we neet to use `>` to see stderr
}
finally {
    Write-Host 'all the works done'
}