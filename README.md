# Moused
Moused aims to be a crossplatform Mouse Macro program, and now it had done windows part

## roadmap
1. windows: done
2. linux: done
3. GUI control panel: almost done
3. bsd (freebsd/openbsd/netbsd/dragonfly): done (input injection works under both X11 and Wayland; on OpenBSD/NetBSD hotkey capture is unavailable while the window system owns the keyboard; on DragonFly there is no uinput, so injection writes input_event structs into a real REL mouse's evdev node)
4. macos: done
