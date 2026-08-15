## a template hotkey-properties should like that
```toml
[key]
keys = ["L"]
enabled = <bool>
type = 'inline' or 'replay'
[key.onActive]
val = [[cmd_arr1], [cmd_arr2]...] or """path to replay file(if `replay`)"""
# file paths are resolved relative to this config's directory, so
# `val = "./rightTransfer.replay"` means `<config dir>/rightTransfer.replay`.
# Windows absolute paths: use forward slashes
# (`val = "C:/Users/.../rightTransfer.replay"`) or a single-quoted literal string
# (`val = 'C:\Users\...\rightTransfer.replay'`). NEVER use a double-quoted
# backslash path (`val = "C:\Users\..."`) — in TOML `\U`, `\9`, `\A`...
# are invalid escapes and abort parsing.
[key.onInterrupt]
val = [[cmd_arr1], [cmd_arr2]...] or """path to replay file(if `replay`)"""
# why onInterrupt: some macro such as `{cmd = 'press', args = {'LMB'}, delay = 0}`, 
# when we want to interrupt it, program  will run 
# `{cmd = 'press', args = {'LMB'}, delay = 0}` instead of `{cmd = 'release', args = {'LMB'}, delay = 0}`
# omitting [key.onInterrupt] makes it fall back to [key.onActive].val
[key.loop]
enabled = <bool>
times = <int>
# times = -1: infinity loop
delay = <double>
# delay unit: ms
```

## example combo 
(table name is just an identifier, must be unique):
```toml
[CTRL_L]                      # required
enabled = true                # required
keys = ["LEFT_CONTROL", "L"]  # required
type = 'inline'               # required
[key.onActive]                # required
val = """..."""               # required
[key.onInterrupt]             # omittable
val = """..."""               # omittable
                              # when omitted, it falls back to [key.onActive].val
[CTRL_L.loop]                 # required
enabled = true                # required
times = 0                     # required
delay = 0.0                   # required
```

while a longer combo is held, its shorter overlapping sub-key combos are
suppressed (e.g. holding Ctrl+L keeps plain L from firing).


the code must be a array of folowing array
```toml
{cmd = <cmd>, args = [<double>, <double>, ...], delay = <double>},
```
EG:
```toml
[
  {cmd = 'wheel', args = ['WU', 10], delay = 0},
  {cmd = 'wheel', args = ['WD', 11], delay = 0.1}
]
```


### `args` 
are all command param (doubles); the C++ side casts them to
whatever type each adapter function needs.
command and argument lists:
| cmd | param |
| - | - |
| `'translate':`| `[<angle>, <distance>, <time_ms?>]`       |
| `'move_to':`  | `[<x(px)>, <y(px)>, <time_ms?>]`          |
| `'click':`    | `['LMB' \| 'RMB' \| 'MMB' \| 'XB1' \| 'XB2']` |
| `'press':`    | `['LMB' \| 'RMB' \| 'MMB' \| 'XB1' \| 'XB2']` |
| `'release':`  | `['LMB' \| 'RMB' \| 'MMB' \| 'XB1' \| 'XB2']` |
| `'wheel':`    | `['WU' \| 'WD', <scale>]`                  |
if angle = 0,
it will point to x+ direction
which is the right direction in physical world
if angle > 0,
it will point to the direction CW rotate `angle` degs from x+
if angle < 0,
it will point to the direction CCW rotate `angle` degs from x+



### `delay` 
is OPTIONAL (measured in ms): the time to wait BEFORE this
  instruction. When omitted, it falls back to 0.0.
  The examples below always write it out explicitly — that is the
  canonical template form.

## prop means
`smooth_frametime_ms = 4`
when smooth moving,
prog will slice the path to dest into pieces,
and the value below is how long will stay in per pieces

`max_window_width`
`max_window_height`
window size upper bound; 0 = no limit

`silent_launch`
when true, prog starts without showing the main window — only the tray icon
appears (open the window again from the tray menu)
