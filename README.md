# linenoise-luv

Lua bindings for [linenoise](https://github.com/antirez/linenoise), including
the blocking prompt API and linenoise's multiplexed edit API for use with event
loops such as [luv](https://luarocks.org/modules/luarocks/luv).

The module is loaded as:

```lua
local ln = require("linenoise-luv")
```

## Install

From LuaRocks after publishing:

```sh
luarocks install linenoise-luv
```

From a checkout:

```sh
luarocks make
```

## Example

Blocking input:

```lua
local ln = require("linenoise-luv")

ln.historysetmaxlen(100)

while true do
   local line = ln.linenoise("> ")
   if not line then
      break
   end
   print("echo: " .. line)
   ln.historyadd(line)
end
```

See `example_luv.lua` for use with `luv`.

## API

- `linenoise(prompt)` returns a line, or `nil` on EOF/error.
- `editstart(prompt)` starts a non-blocking edit session.
- `editfeed()` advances the active edit session and returns `nil, true` while
  editing continues, a completed line when done, or `nil, nil` on EOF/error.
- `editstop()` stops the active edit session.
- `editlen()` returns the current non-blocking edit buffer length in bytes.
- `setcompletion(fn)` registers a completion callback.
- `sethints(fn)` registers a hints callback.
- `historyadd(line)`, `historysetmaxlen(len)`, `historysave(path)`, and
  `historyload(path)` manage history.
- `clearscreen()`, `setmultiline(enabled)`, `setmaskmode(enabled)`, and
  `printkeycodes()` expose linenoise terminal helpers.

## Publishing

Before uploading to LuaRocks:

1. Commit the repository and tag the release:

   ```sh
   git tag v0.1.1
   git push origin main --tags
   ```

2. Check the rock locally:

   ```sh
   luarocks lint linenoise-luv-0.1.1-1.rockspec
   luarocks make linenoise-luv-0.1.1-1.rockspec
   luarocks pack linenoise-luv-0.1.1-1.rockspec
   ```

3. Upload:

   ```sh
   luarocks upload linenoise-luv-0.1.1-1.rockspec --api-key=YOUR_API_KEY
   ```

LuaRocks package names are global. The package name here is
`linenoise-luv`, and the Lua module it installs is required as
`linenoise-luv`.

## License

This binding is MIT licensed. The vendored linenoise sources in `deps/` are
BSD-2-Clause licensed; see `deps/LICENSE.linenoise`.
