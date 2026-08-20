# moused i18n (gettext)

`_("...")` in the code is wxWidgets' `wxGetTranslation()`. At startup
`moused::OnInit()` (in `src/main.cpp`) initializes a `wxLocale` and loads the
`moused` catalog from `locale/<lang>/LC_MESSAGES/moused.mo` next to the
executable.

## Workflow

1. **Extract** new strings into the template (GNU gettext required):

   ```
   xgettext --keyword=_ -C -o i18n/moused.pot src/*.cpp
   ```
   **ps** install `xgettext` for windows: https://github.com/mlocati/gettext-iconv-windows/releases

2. **Translate**: 
   1. copy `moused.pot` (or an existing `.po`) to `i18n/<lang>.po`
      (e.g. `zh_CN.po`) and fill in the `msgstr` lines. Keep the file UTF-8.
   2. translate [polkit policy](../policy/com.moused.program.policy)
   Note: wxWidgets uses POSIX locale tags — `zh_CN`, `en_US`, `de_DE`, ... —
   which are exactly the `NAME_WE` of each `i18n/*.po` file.



3. **Build**: CMake detects `msgfmt` on `PATH`, compiles every
   `i18n/<lang>.po` into `locale/<lang>/LC_MESSAGES/moused.mo` and copies it
   next to the built executable automatically. Without `msgfmt` the build
   still succeeds and `_()` shows the raw strings.

## Manual compile (no CMake)

```
msgfmt -o locale/zh_CN/LC_MESSAGES/moused.mo i18n/zh_CN.po
```
