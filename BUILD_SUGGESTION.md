# FreeBSD
- install pkgs `sudo pkg install -y git cmake pkgconf gtk3 wayland wayland-protocols libxkbcommon bzip2 freetype2 polkit libepoxy gettext glib dbus`
- The GENERIC kernel ships `device evdev` and `device uinput`, so `/dev/uinput`
  and `/dev/input/event*` are created automatically. moused elevates itself
  via pkexec (needs the `sysutils/polkit` package, which is in the list above).

# OpenBSD
- install pkgs `doas pkg_add git cmake pkgconf gtk3+`
- X11, wscons and the mouse mux (`/dev/wsmouse`) are in the base system, so no
  extra input packages are needed.
- There is no reliable polkit/pkexec on OpenBSD: launch with doas and keep the
  X/Wayland display environment, e.g.
  `doas env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ./moused`
- NOTE: hotkeys need a free `/dev/wskbd*`. While the window system owns the
  keyboard (the normal case) wscons gives no second process access to the key
  state, so hotkeys are unavailable there — the app still runs for manual
  config/editing, but macros can't be triggered.

# NetBSD
- pkgsrc: `pkgin install git cmake pkgconf gtk3`
- wscons mouse mux `/dev/wsmouse` (a.k.a. `/dev/wsmux0`) is in the base system.
- Same launch note as OpenBSD (doas/sudo + keep DISPLAY), and the same hotkey
  caveat applies.
- The base compiler (gcc 10) has no `std::jthread`; build with base/pkgsrc
  clang or `lang/gcc12+`, e.g. `CC=clang CXX=clang++ cmake ..`

# DragonFly
- install pkgs `sudo pkg install -y git cmake pkgconf gtk3`
- Enable evdev in the kernel (`device evdev` or the `evdev` module); `/dev/input/event*`
  then appears under devfs.
- There is NO uinput on DragonFly: moused injects by writing `input_event` structs
  directly into a REL mouse's `/dev/input/event*` node (the kernel's
  `evdev_write()` -> `evdev_inject_event()` path), so the display server sees the
  events in the very stream it reads. It picks the first plain REL mouse (not a
  touchpad); with several pointers you may need to check which node is used.
- Launch with doas/sudo and keep the display environment:
  `doas env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ./moused`
- Keyboard capture uses evdev `EVIOCGKEY` (same as FreeBSD) and is best-effort.

# Input backend summary (all BSDs work under both X11 and Wayland)
- FreeBSD: `/dev/uinput` (Linux-compatible virtual mouse) + evdev
  `EVIOCGKEY` keyboard capture.
- DragonFly: evdev `input_event` write-injection into a REL mouse's
  `/dev/input/event*` node + evdev `EVIOCGKEY` keyboard capture (no uinput).
- OpenBSD/NetBSD: wscons `WSMUXIO_INJECTEVENT` on `/dev/wsmouse` (the mouse
  mux the window system reads) + best-effort `/dev/wskbd*` keyboard capture.

