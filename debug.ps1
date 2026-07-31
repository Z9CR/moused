try {
    cmake -B .\build
    cmake --build .\build
    Write-Host 'compile work done'
    .\build\Debug\moused.exe 2>&1 
    # 'cause wxWidget's start func isnt `main` but `WinMain` instead, 
    # we neet to use `>` to see stderr
}
finally {
    #Remove-Item .\build\Debug\*
    Write-Host 'all the works done'
}