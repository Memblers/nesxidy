-- Mesen 2.1.1 Lua script: dump key RAM/vars to file on write to $4026

local outFile = emu.getScriptDataFolder() .. "/dump_4026.txt"

local function dump_bytes(addr, len)
    local t = {}
    for i = 0, len-1 do
        t[#t+1] = string.format("%02X", emu.read(addr+i, emu.memType.nesMemory))
    end
    return table.concat(t, " ")
end

local function dump_word(addr)
    local lo = emu.read(addr, emu.memType.nesMemory)
    local hi = emu.read(addr+1, emu.memType.nesMemory)
    return string.format("%04X", hi*256 + lo)
end

local function dump_vars()
    local f = io.open(outFile, "a")
    f:write("---- DUMP ON $4026 WRITE ----\n")
    -- PC table (example: 0x8000, 32 bytes, adjust as needed)
    f:write("PC Table: " .. dump_bytes(0x8000, 0x20) .. "\n")
    -- code_index
    f:write("code_index ($0091): " .. dump_bytes(0x0091, 1) .. "\n")
    -- flash_code_bank ($6b33), flash_code_address ($6b31)
    f:write("flash_code_bank ($6b33): " .. dump_bytes(0x6b33, 1) .. "\n")
    f:write("flash_code_address ($6b31): " .. dump_word(0x6b31) .. "\n")
    -- Add more as needed from [vicemap.map](http://_vscodecontentref_/0)
    f:close()
end

emu.addMemoryCallback(
    function(address, value)
        dump_vars()
    end,
    emu.callbackType.write,
    0x4026,
    0x4026,
    emu.cpuType.nes
)

emu.displayMessage("Lua", "Dump script loaded. Will write to: " .. outFile)