# Sends this computer's time to the UNIHIKER K10 over the USB cable.
# Run it by double-clicking SET_TIME.bat (the board must be plugged in and switched on).
param([switch]$WifiTest)

$dev = Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.Name -match '\(COM\d+\)' -and $_.PNPDeviceID -match 'VID_303A' } |
  Select-Object -First 1
if (-not $dev) {
  Write-Host "K10 not found. Plug in the USB cable and try again." -ForegroundColor Red
  Start-Sleep -Seconds 6
  exit 1
}
$portName = [regex]::Match($dev.Name, 'COM\d+').Value
Write-Host "K10 found on $portName"

$port = New-Object System.IO.Ports.SerialPort $portName, 115200
$port.ReadTimeout = 1000
$port.WriteTimeout = 1000
$port.NewLine = "`n"
try { $port.Open() }
catch {
  Write-Host "Cannot open $portName. Close the Serial Monitor in Arduino IDE and try again." -ForegroundColor Red
  Start-Sleep -Seconds 8
  exit 1
}

$inv = [System.Globalization.CultureInfo]::InvariantCulture
$ok = $false
try {
  if ($WifiTest) {
    # ask the board to try its WiFi again and print what it sees
    $port.WriteLine('WIFI?')
    $deadline = (Get-Date).AddSeconds(45)
    while ((Get-Date) -lt $deadline) {
      try { $reply = $port.ReadLine().Trim() } catch { $reply = '' }
      if ($reply) { Write-Host "  board: $reply" }
      if ($reply -match '^WIFI RESULT') { $ok = $true; break }
    }
  } else {
    for ($i = 0; $i -lt 40 -and -not $ok; $i++) {
      $line = 'T=' + (Get-Date).ToString('HH:mm:ss,yyyy-MM-dd', $inv)
      try { $port.WriteLine($line) } catch {}
      $deadline = (Get-Date).AddMilliseconds(900)
      while ((Get-Date) -lt $deadline) {
        try { $reply = $port.ReadLine().Trim() } catch { $reply = '' }
        if ($reply -match 'TIME OK') { $ok = $true; break }
        if ($reply) { Write-Host "  board: $reply" }
      }
    }
  }
}
finally { $port.Close() }

if ($WifiTest) { exit 0 }
if ($ok) {
  Write-Host ("Done! The cat's clock is now " + (Get-Date).ToString('HH:mm:ss', $inv)) -ForegroundColor Green
  Start-Sleep -Seconds 3
  exit 0
}
Write-Host "The board did not answer. Upload the new Thor_K10 sketch first, then try again." -ForegroundColor Red
Start-Sleep -Seconds 8
exit 1
