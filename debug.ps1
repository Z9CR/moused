try {
    cmake -B .\build
    cmake --build .\build
    Write-Host 'compile work done'
    .\build\Debug\moused.exe
}
finally {
    #Remove-Item .\build\Debug\*
    Write-Host 'all the works done'
}