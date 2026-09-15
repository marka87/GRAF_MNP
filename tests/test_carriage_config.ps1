# =============================================================================
# test_carriage_config.ps1
# Selbstpruefender Test fuer Schlitten-Konfiguration & Flash-Sektor-7-Emulation
# =============================================================================

$ErrorActionPreference = "Stop"

function Assert-Equal($actual, $expected, $message) {
    if ($actual -ne $expected) {
        Write-Error "ASSERTION FAILED: $message (Expected: $expected, Got: $actual)"
    } else {
        Write-Host "[PASS] $message" -ForegroundColor Green
    }
}

Write-Host "Running Carriage Config & Flash Verification..." -ForegroundColor Cyan

# 1. Sektor 7 Geometrie & Rolling Slot Kapazitaet
$sectorSize = 256 * 1024
$slotSize = 256
$maxSlots = $sectorSize / $slotSize

Assert-Equal $slotSize 256 "Slot-Groesse muss exakt 256 Bytes betragen"
Assert-Equal $sectorSize 262144 "Sektor 7 muss 262.144 Bytes (256 KB) gross sein"
Assert-Equal $maxSlots 1024 "Sektor 7 muss Platz fuer genau 1024 Rolling-Slots bieten"

# 2. IEEE 802.3 CRC-32 Testvektor ("123456789" -> 0xCBF43926)
function Compute-CRC32([byte[]]$bytes) {
    $crc = [uint32]"0xFFFFFFFF"
    $poly = [uint32]"0xEDB88320"
    foreach ($b in $bytes) {
        $crc = $crc -bxor [uint32]$b
        for ($i = 0; $i -lt 8; $i++) {
            if (($crc -band 1) -ne 0) {
                $crc = ($crc -shr 1) -bxor $poly
            } else {
                $crc = $crc -shr 1
            }
        }
    }
    return [uint32]((-bnot $crc) -band 0xFFFFFFFFL)
}

$testBytes = [System.Text.Encoding]::ASCII.GetBytes("123456789")
$crcResult = Compute-CRC32 $testBytes
$expectedCrc = [uint32]"0xCBF43926"
Assert-Equal ('0x{0:X8}' -f $crcResult) ('0x{0:X8}' -f $expectedCrc) "IEEE 802.3 CRC-32 Checksumme auf Standard-Testvektor"

# 3. Serial Protocol Parsing Check (CFG: Line)
$sampleLine = "CFG:ID=SCHLITTEN-A;PKP=0.1200;PKI=0.000000;PKD=0.0000;VKP=0.0250;VKI=0.000250;VKD=0.0000;LZ=60;LS=4;BF=80;SNO=3204;TMV=1500;DMOT=3.30;CNT=7"
$payload = $sampleLine.Substring(4).Trim()
$pairs = $payload.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries)
$dict = @{}
foreach ($pair in $pairs) {
    $parts = $pair.Split('=')
    if ($parts.Length -eq 2) {
        $dict[$parts[0].Trim().ToUpperInvariant()] = $parts[1].Trim()
    }
}

Assert-Equal $dict["ID"] "SCHLITTEN-A" "ID muss SCHLITTEN-A sein"
Assert-Equal ([int]$dict["LZ"]) 60 "Landing Zone Margin muss 60 inc sein"
Assert-Equal ([int]$dict["LS"]) 4 "Landing Speed Level muss 4 sein"
Assert-Equal ([int]$dict["CNT"]) 7 "Write Count muss 7 sein"
Assert-Equal ([double]::Parse($dict["PKP"], [System.Globalization.CultureInfo]::InvariantCulture)) 0.12 "Position Kp muss 0.12 sein"

# 4. Schlitten A vs Schlitten B Parametersatz-Konsistenz
$profileA = @{ ID = "SCHLITTEN-A"; LZ = 60; LS = 4 }
$profileB = @{ ID = "SCHLITTEN-B"; LZ = 150; LS = 3 }

Assert-Equal ($profileA.LZ -lt $profileB.LZ) $true "Schlitten A Landebahn muss kuerzer sein als Schlitten B (60 < 150)"
Assert-Equal ($profileA.LS -gt $profileB.LS) $true "Schlitten A Landegeschwindigkeit muss hoeher sein als Schlitten B (4 > 3 gegen Stiction)"

# 5. Variante A: Geschwindigkeitsbewusste Auto-Erkennung Kriterien
function Classify-Carriage($stallMs, $landingTimeMs, $expectedTimeMs = 30) {
    $maxNormal = ($expectedTimeMs * 2) + 50
    if ($stallMs -ge 15 -or $landingTimeMs -gt $maxNormal) {
        return 2 # Schlitten A
    } else {
        return 1 # Schlitten B
    }
}

Assert-Equal (Classify-Carriage 178 215) 2 "Stall 178 ms & Landedauer 215 ms muss als Schlitten A klassifiziert werden"
Assert-Equal (Classify-Carriage 0 14) 1 "Stall 0 ms & Landedauer 14 ms muss als Schlitten B klassifiziert werden"
Assert-Equal (Classify-Carriage 0 38) 1 "Stall 0 ms & Landedauer 38 ms muss als Schlitten B klassifiziert werden"

# 6. TEST_B_SUMMARY Telegramm mit Nadelschlitten-Diagnose-Feldern
$summaryMsg = "TEST_B_SUMMARY:status=OK,cycles=10,done=10,z_ref=495,z_min=495,z_max=499,delta_min=0,delta_max=4,range=4,mean=498.4,baseline_v=0.021,trig_v=1.520,time_ms=1450,speed_level=6,v_max=508.0,a_max=4.7,a_brake=4.3,no_sensor_pos=3204,contact_travel=6,peak_v=4.282,ns_type=2,ns_stall=178,ns_time=215,ns_lag=24,ns_auto=1,ns_id=DEFAULT,ns_lz=60,ns_ls=4"
$sPayload = $summaryMsg.Substring("TEST_B_SUMMARY:".Length).Trim()
$sPairs = $sPayload.Split(',')
$sDict = @{}
foreach ($pair in $sPairs) {
    $p = $pair.Split('=')
    if ($p.Length -eq 2) {
        $sDict[$p[0].Trim()] = $p[1].Trim()
    }
}

Assert-Equal ([int]$sDict["speed_level"]) 6 "Summary muss speed_level=6 enthalten"
Assert-Equal ([int]$sDict["ns_type"]) 2 "Summary muss ns_type=2 enthalten"
Assert-Equal ([int]$sDict["ns_stall"]) 178 "Summary muss ns_stall=178 enthalten"
Assert-Equal ([int]$sDict["ns_auto"]) 1 "Summary muss ns_auto=1 enthalten"
Assert-Equal ([int]$sDict["ns_lz"]) 60 "Summary muss ns_lz=60 enthalten"
Assert-Equal ([int]$sDict["ns_ls"]) 4 "Summary muss ns_ls=4 enthalten"

# 7. Phase 2: Ringpuffer-Chronologie & Wrapping
function Test-RingBufferChronology($maxSamples, $writtenSamples) {
    $buffer = New-Object int[] $maxSamples
    $head = 0
    $isFull = $false

    for ($i = 0; $i -lt $writtenSamples; $i++) {
        $buffer[$head] = 1000 + $i # Monotoner Tick
        $head++
        if ($head -ge $maxSamples) {
            $head = 0
            $isFull = $true
        }
    }

    $total = if ($isFull) { $maxSamples } else { $head }
    $startIdx = if ($isFull) { $head } else { 0 }
    $tZero = $buffer[$startIdx]

    $output = @()
    for ($i = 0; $i -lt $total; $i++) {
        $idx = if ($isFull) { ($startIdx + $i) % $maxSamples } else { $i }
        $output += ($buffer[$idx] - $tZero)
    }
    return $output
}

$ringOut = Test-RingBufferChronology 2000 3500
Assert-Equal $ringOut.Count 2000 "Ringpuffer muss bei 3500 Schreibvorgängen exakt 2000 Samples halten"
Assert-Equal $ringOut[0] 0 "Erster relativer Timestamp muss exakt 0 ms sein"
Assert-Equal $ringOut[1999] 1999 "Letzter relativer Timestamp muss exakt 1999 ms sein"
Assert-Equal ($ringOut[100] -lt $ringOut[200]) $true "Ringpuffer-Ausgabe muss streng monoton steigend sein"

# 8. Linker-Schutz fuer Flash Sektor 7 & Direkt-Boot
$appLd = Get-Content (Join-Path $PSScriptRoot "..\Software\GRAF_MNPA\STM32F746NGHX_FLASH.ld") -Raw
$appOriginMatch = [regex]::Match($appLd, 'FLASH\s*\(rx\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+),\s*LENGTH\s*=\s*(\d+K)')
Assert-Equal $appOriginMatch.Success $true "Applikation Linker-Skript Flash-Definition gefunden"
Assert-Equal ([uint32]$appOriginMatch.Groups[1].Value) 0x08000000 "Applikation Origin muss 0x08000000 (Direkt-Boot) sein"
Assert-Equal $appOriginMatch.Groups[2].Value.ToUpper() "768K" "Applikation Flash-Laenge muss 768 KB betragen (Sektor 7 geschuetzt)"

Write-Host "`nALL CHECKS PASSED SUCCESSFULLY!" -ForegroundColor Green
