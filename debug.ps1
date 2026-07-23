try {
    cmake -B .\build
    cmake --build .\build
    .\build\Debug\moused.exe
}
finally {
    echo done
}