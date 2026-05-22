local ln = require('linenoise-luv')

ln.historysetmaxlen(100)
ln.setmultiline(true)

ln.setcompletion(function(completions, line)
    if line:sub(1, 1) == "h" then
        completions:add("hello")
        completions:add("help")
    end
end)

ln.sethints(function(line)
    if line == "hello" then
        return " world", { color = 35, bold = true }
    end
end)

while true do
    local line = ln.linenoise("> ")
    if not line then break end
    print("echo: " .. line)
    ln.historyadd(line)
end
