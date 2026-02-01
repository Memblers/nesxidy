-- JIT Debug Instrumentation Script for Mesen
-- Captures state at key points to debug flash cache execution

local projectDir = "c:/proj/c/NES/nesxidy-co/nesxidy"
local outFile = projectDir .. "/jit_debug.log"
local dumpFile = projectDir .. "/jit_dump.bin"

-- Clear log file on start
local f = io.open(outFile, "w")
f:write("=== JIT Debug Log Started ===\n")
f:write("Monitoring addresses: $32AE-$32C0 (sidetrac screen copy loop)\n\n")
f:close()

-- Key zero page addresses (from vicemap.map)
local ZP_PC_LO = 0x58
local ZP_PC_HI = 0x59
local ZP_A = 0x5B
local ZP_X = 0x5C
local ZP_Y = 0x5D
local ZP_STATUS = 0x5E
local ZP_DECODED_ADDR = 0x79
local ZP_CACHE_INDEX = 0x75

-- Memory regions
local SCREEN_RAM_BASE = 0x0300   -- NES screen RAM
local RAM_BASE = 0x6C00          -- Emulated 6502 RAM (zero page starts here)

-- Addresses of interest (sidetrac screen copy loop)
local WATCH_PC_START = 0x32AE
local WATCH_PC_END = 0x32C0

local logCount = 0
local maxLogs = 500  -- Limit to prevent huge files

-- Helper to read a byte from CPU memory
local function readByte(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

-- Helper to read 16-bit value
local function read16(addrLo)
    return readByte(addrLo) + (readByte(addrLo + 1) * 256)
end

-- Helper to format hex
local function hex8(val)
    return string.format("%02X", val)
end

local function hex16(val)
    return string.format("%04X", val)
end

-- Dump zero page state
local function dumpState(trigger)
    if logCount >= maxLogs then return end
    logCount = logCount + 1
    
    -- Just read emulated state from memory (real CPU registers not easily accessible)
    local emulatedPC = read16(ZP_PC_LO)
    local emulatedA = readByte(ZP_A)
    local emulatedX = readByte(ZP_X)
    local emulatedY = readByte(ZP_Y)
    local emulatedStatus = readByte(ZP_STATUS)
    local decodedAddr = read16(ZP_DECODED_ADDR)
    
    -- Read emulated zero page pointer at $00 and $25
    local ptr00 = readByte(RAM_BASE + 0x00) + (readByte(RAM_BASE + 0x01) * 256)
    local ptr25 = readByte(RAM_BASE + 0x25) + (readByte(RAM_BASE + 0x26) * 256)
    
    local line = string.format(
        "[%04d] %s\n" ..
        "  Emulated: PC=%s A=%s X=%s Y=%s P=%s\n" ..
        "  DecodedAddr=%s  Ptr($00)=%s  Ptr($25)=%s\n",
        logCount, trigger,
        hex16(emulatedPC), hex8(emulatedA), hex8(emulatedX), hex8(emulatedY), hex8(emulatedStatus),
        hex16(decodedAddr), hex16(ptr00), hex16(ptr25)
    )
    
    local f = io.open(outFile, "a")
    f:write(line .. "\n")
    f:close()
end

-- Dump memory region to log
local function dumpMemory(startAddr, length, label)
    if logCount >= maxLogs then return end
    
    local f = io.open(outFile, "a")
    f:write(string.format("=== %s ($%04X, %d bytes) ===\n", label, startAddr, length))
    
    local line = ""
    for i = 0, length - 1 do
        if i % 16 == 0 then
            if line ~= "" then f:write(line .. "\n") end
            line = string.format("%04X: ", startAddr + i)
        end
        line = line .. hex8(readByte(startAddr + i)) .. " "
    end
    if line ~= "" then f:write(line .. "\n") end
    f:write("\n")
    f:close()
end

-- Monitor writes to emulated PC (signals block exit)
emu.addMemoryCallback(
    function(address, value)
        local emulatedPC = read16(ZP_PC_LO)
        if emulatedPC >= WATCH_PC_START and emulatedPC <= WATCH_PC_END then
            dumpState("PC write: $" .. hex16(emulatedPC))
        end
    end,
    emu.callbackType.write,
    ZP_PC_LO,
    ZP_PC_HI,
    emu.cpuType.nes
)

-- Monitor writes to screen RAM (catches the STA ($00),Y results)
local screenWriteCount = 0
emu.addMemoryCallback(
    function(address, value)
        if screenWriteCount < 100 then
            screenWriteCount = screenWriteCount + 1
            local emulatedPC = read16(ZP_PC_LO)
            local f = io.open(outFile, "a")
            f:write(string.format("ScreenWrite[%d]: $%04X <- $%02X (emPC=$%04X)\n",
                screenWriteCount, address, value, emulatedPC))
            f:close()
        end
    end,
    emu.callbackType.write,
    SCREEN_RAM_BASE,
    SCREEN_RAM_BASE + 0xFF,
    emu.cpuType.nes
)

-- Monitor decoded_address changes (indy addressing debug)
emu.addMemoryCallback(
    function(address, value)
        local emulatedPC = read16(ZP_PC_LO)
        if emulatedPC >= WATCH_PC_START and emulatedPC <= WATCH_PC_END then
            local decodedAddr = read16(ZP_DECODED_ADDR)
            local f = io.open(outFile, "a")
            f:write(string.format("DecodedAddr: $%04X (emPC=$%04X)\n", decodedAddr, emulatedPC))
            f:close()
        end
    end,
    emu.callbackType.write,
    ZP_DECODED_ADDR,
    ZP_DECODED_ADDR + 1,
    emu.cpuType.nes
)

-- Periodic state dump (every N frames)
local frameCount = 0
local dumpEveryNFrames = 60  -- Once per second at 60fps
emu.addEventCallback(
    function()
        frameCount = frameCount + 1
        if frameCount % dumpEveryNFrames == 0 and logCount < maxLogs then
            dumpState("Frame " .. frameCount)
            -- Dump first 16 bytes of emulated zero page
            dumpMemory(RAM_BASE, 0x30, "Emulated ZP ($00-$2F)")
        end
    end,
    emu.eventType.endFrame
)

-- Initial dump
dumpState("Script Start")
dumpMemory(RAM_BASE, 0x30, "Initial Emulated ZP")
dumpMemory(SCREEN_RAM_BASE, 0x40, "Initial Screen RAM (first 64 bytes)")

emu.displayMessage("Lua", "JIT Debug script loaded. Log: " .. outFile)
