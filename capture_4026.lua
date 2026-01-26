local outFile = emu.getScriptDataFolder() .. "/debug_4026.txt"
local line = ""

emu.addMemoryCallback(
    function(address, value)
        if value == 0x0A then
            local f = io.open(outFile, "a")
            f:write(line .. "\\n")
            f:close()
            line = ""
        else
            line = line .. string.char(value)
        end
    end,
    emu.callbackType.write,
    0x4026,
    0x4026,
    emu.cpuType.nes
)

emu.displayMessage("Lua", "Debug capture loaded. Will write to: " .. outFile)