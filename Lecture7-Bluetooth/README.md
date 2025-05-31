# Lecture4-UDP

PowerShell Snippet to listen to the given port:
```powershell
Function listenToUDP {
    # Configure this
    $port = 15651
    $timeout = 1000 * 10 # 10 seconds

    # Do not touch this
    Write-Host "Start listening to UDP..."

    $endpoint = New-Object System.Net.IPEndPoint ([IPAddress]::Any, $port)
    $udpclient = New-Object System.Net.Sockets.UdpClient $port
    $udpclient.Client.ReceiveTimeout = $timeout

    $script:stopListening = $false
    $handler = {
        $script:stopListening = $true
        Write-Host "`nStopping listener..."
    }
    Register-EngineEvent PowerShell.Exiting -Action $handler > $null

    try {
        while (-not $script:stopListening) {
            try {
                $content = $udpclient.Receive([ref]$endpoint)
                $receivetimestamp = Get-Date -Format HH:mm:ss:ff
                Write-Host "`nPacket received at $($receivetimestamp)"
                Write-Host "Sender: $($endpoint.tostring())"
                Write-Host "---- DATA ------"

                # [string[]]$parsedContent = [System.Text.Encoding]::ASCII.GetString($content)
                # Write-Host $parsedContent

                printRawConent $content
                printParsedConent $content

            } catch {
                if (
                    ($_.Exception -is [System.TimeoutException]) -or
                    ($_.Exception -is [System.Management.Automation.MethodInvocationException])
                ) {
                    # Will be printed if no message comes in within the given timeout
                    Write-Host "Waiting for data..."
                } else {
                    Write-Error "An error occurred: $($_.Exception)"
                }
            }
        }
    } finally {
        Write-Host "Closing UDP connection..."
        $udpclient.Close()
        Unregister-Event -SourceIdentifier PowerShell.Exiting
    }
}

Function printRawConent {
    param (
        [byte[]]$content
    )

    Write-Host "RAW"
    foreach ($byte in $content) {
        Write-Host ("    0x{0:X2}" -f $byte)
    }
    Write-Host ""
}

Function printParsedConent {
    param (
        [byte[]]$content
    )

    Write-Host "PARSED"
    Write-Host "    Button:    $(if ($content[0] -eq 9) { 'Left' } elseif ($content[0] -eq 2) { 'Right' } else { $content[0] })"
    Write-Host "    Event:     $($content[1] -eq 0 ? 'pressed' : 'released')"

    $timestampBytes = [byte[]](0..7 | ForEach-Object { 0 })
    $availableBytes = $content[2..($content.Length - 1)]
    [Array]::Copy($availableBytes, 0, $timestampBytes, 0, $availableBytes.Length)
    [Array]::Reverse($timestampBytes) #ToDo: This does not work
    $timestamp = [BitConverter]::ToUInt64($timestampBytes, 0)

    Write-Host "    Timestamp: $timestamp"
    Write-Host ""
}

listenToUDP
 ```