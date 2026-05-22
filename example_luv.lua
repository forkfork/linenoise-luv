local ln = require('linenoise-luv')
local uv = require('luv')

ln.historysetmaxlen(100)
ln.historyload(".history")

ln.setcompletion(function(completions, line)
    if line:sub(1, 1) == "h" then
        completions:add("hello")
        completions:add("help")
    end
end)

ln.sethints(function(line)
    if line == "hello" then
        return " world", { color = 35, bold = false }
    end
end)

local poll

local function prompt()
    ln.editstart("> ")

    -- Use poll, NOT read_start. editfeed() calls read() internally,
    -- so we just need to know when stdin is readable, not consume the bytes.
    poll = uv.new_poll(0)
    poll:start("r", function()
        local line, more = ln.editfeed()

        if more then
            return -- still editing
        end

        poll:stop()
        poll:close()

        if line then
            print("got: " .. line)
            ln.historyadd(line)

            if line == "quit" then
                ln.historysave(".history")
                return
            end

            prompt()
        else
            -- ctrl-d or error
            print("")
            ln.historysave(".history")
        end
    end)
end

prompt()
uv.run()
