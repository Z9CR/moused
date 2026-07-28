echo $XDG_SESSION_TYPE
echo $WAYLAND_DISPLAY   
cmake -B ./build 
cmake --build ./build
./build/moused
echo $XDG_SESSION_TYPE
echo $WAYLAND_DISPLAY   