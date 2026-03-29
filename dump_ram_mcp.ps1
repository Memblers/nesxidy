# Dump NES RAM via Mesen MCP server
$uri = "http://localhost:51234/mcp"

# NES RAM is typically at addresses $0000-$07FF (2KB)
# Make an MCP request using JSON-RPC 2.0

$body = @{
    jsonrpc = "2.0"
    method  = "tools/call"
    params  = @{
        name      = "get_memory_range"
        arguments = @{
            startAddress = 0x0000
            length       = 0x0800
        }
    }
    id      = 1
} | ConvertTo-Json

Write-Host "Requesting NES RAM from MCP server at $uri"

try {
    $response = Invoke-WebRequest -Uri $uri -Method Post -Body $body -ContentType "application/json" -ErrorAction Stop -UseBasicParsing
    $result = $response.Content | ConvertFrom-Json
    
    if ($result.error) {
        Write-Host "Error: $($result.error.message)"
    } elseif ($result.result) {
        Write-Host "Successfully retrieved memory dump (2KB NES RAM)"
        $content = $result.result.content
        
        # Display as hex dump
        $hex = ""
        for ($i = 0; $i -lt $content.Count; $i++) {
            $hex += "{0:X2} " -f $content[$i]
            if (($i + 1) % 16 -eq 0) {
                Write-Host ("{0:X4}: {1}" -f $i - 15, $hex.Trim())
                $hex = ""
            }
        }
        
        # Save hex dump
        $hexStr = ($content | ForEach-Object { "{0:X2}" -f $_ }) -join " "
        $hexStr | Out-File "nes_ram_dump.txt"
        Write-Host "Saved to nes_ram_dump.txt"
    } else {
        Write-Host "Response: $($response.Content)"
    }
} catch {
    Write-Host "Failed to connect: $_"
    Write-Host "Make sure Mesen is running with the MCP server enabled"
}
