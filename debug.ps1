try {
    cmake -B .\build
    cmake --build .\build
    print 'compile work done'
    .\build\Debug\moused.exe
}
finally {
    Remove-Item .\build\Debug\*
    print 'all the works done'
}