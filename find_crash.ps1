$f = "c:\proj\c\NES\nesxidy-co\nesxidy\tracelog.txt"
$outFile = "c:\proj\c\NES\nesxidy-co\nesxidy\crash_context.txt"
$sz = (Get-Item $f).Length
Write-Output "File size: $sz"

$found = $false
foreach ($offsetMB in @(150, 140, 130, 120, 110, 100, 90, 80, 70, 60)) {
    $offset = $sz - [int64]($offsetMB * 1000000)
    if ($offset -lt 0) { $offset = 0 }
    Write-Output "Trying offset $offset (sz - ${offsetMB}MB)..."

    $fs = $null
    $sr = $null
    try {
        $fs = [System.IO.FileStream]::new($f, 'Open', 'Read', 'Read')
        [void]$fs.Seek($offset, 'Begin')
        $sr = [System.IO.StreamReader]::new($fs)
        [void]$sr.ReadLine()  # skip partial line

        $lineNum = 0
        $ringBuf = New-Object string[] 50
        $ringIdx = 0
        $ringCount = 0

        while ($lineNum -lt 500000) {
            $line = $sr.ReadLine()
            if ($line -eq $null) { break }
            $lineNum++

            if ($line.Contains('4B63')) {
                Write-Output "FOUND at line $lineNum from this offset!"
                $results = @()

                # Collect ring buffer (lines before)
                $startIdx = if ($ringCount -lt 50) { 0 } else { $ringIdx }
                $count = [Math]::Min($ringCount, 50)
                for ($i = 0; $i -lt $count; $i++) {
                    $idx = ($startIdx + $i) % 50
                    $results += $ringBuf[$idx]
                }

                # The match line
                $results += ">>>CRASH>>> $line"

                # 5 lines after
                for ($j = 0; $j -lt 5; $j++) {
                    $after = $sr.ReadLine()
                    if ($after -ne $null) { $results += $after }
                }

                $results | Out-File -FilePath $outFile -Encoding utf8
                Write-Output "Wrote $($results.Count) lines to $outFile"
                $found = $true
                break
            }

            $ringBuf[$ringIdx] = $line
            $ringIdx = ($ringIdx + 1) % 50
            $ringCount++
        }

        if (-not $found) {
            Write-Output "  Not found in $lineNum lines"
        }
    }
    finally {
        if ($sr -ne $null) { $sr.Dispose() }
        elseif ($fs -ne $null) { $fs.Dispose() }
    }

    if ($found) { break }
}

if (-not $found) {
    Write-Output "4B63 NOT FOUND in searched range"
}
