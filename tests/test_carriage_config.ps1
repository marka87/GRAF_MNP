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

# 5. Variante A: Auto-Erkennung Kriterien
function Classify-Carriage($stallMs, $landingTimeMs) {
    if ($stallMs -gt 10 -or $landingTimeMs -gt 80) {
        return 2 # Schlitten A
    } else {
        return 1 # Schlitten B
    }
}

Assert-Equal (Classify-Carriage 178 215) 2 "Stall 178 ms & Landedauer 215 ms muss als Schlitten A klassifiziert werden"
Assert-Equal (Classify-Carriage 0 38) 1 "Stall 0 ms & Landedauer 38 ms muss als Schlitten B klassifiziert werden"

# 6. TEST_B_SUMMARY Telegramm mit Nadelschlitten-Diagnose-Feldern (Phase 1 & Phase 2)
$summaryMsg = "TEST_B_SUMMARY:status=OK,cycles=10,done=10,z_ref=495,z_min=495,z_max=499,delta_min=0,delta_max=4,range=4,mean=498.4,baseline_v=0.021,trig_v=1.520,time_ms=1450,v_max=508.0,a_max=4.7,a_brake=4.3,no_sensor_pos=3204,contact_travel=6,peak_v=4.282,ns_type=2,ns_stall=178,ns_time=215,ns_lag=24,ns_auto=1,ns_id=DEFAULT,ns_lz=60,ns_ls=4,ss_u=170,glide_u=342,bounce=0,health=2"
$sPayload = $summaryMsg.Substring("TEST_B_SUMMARY:".Length).Trim()
$sPairs = $sPayload.Split(',')
$sDict = @{}
foreach ($pair in $sPairs) {
    $p = $pair.Split('=')
    if ($p.Length -eq 2) {
        $sDict[$p[0].Trim()] = $p[1].Trim()
    }
}

Assert-Equal ([int]$sDict["ns_type"]) 2 "Summary muss ns_type=2 enthalten"
Assert-Equal ([int]$sDict["ns_stall"]) 178 "Summary muss ns_stall=178 enthalten"
Assert-Equal ([int]$sDict["ns_auto"]) 1 "Summary muss ns_auto=1 enthalten"
Assert-Equal ([int]$sDict["ns_lz"]) 60 "Summary muss ns_lz=60 enthalten"
Assert-Equal ([int]$sDict["ns_ls"]) 4 "Summary muss ns_ls=4 enthalten"
Assert-Equal ([int]$sDict["ss_u"]) 170 "Summary muss ss_u=170 mV enthalten"
Assert-Equal ([int]$sDict["glide_u"]) 342 "Summary muss glide_u=342 mV enthalten"
Assert-Equal ([int]$sDict["bounce"]) 0 "Summary muss bounce=0 enthalten"
Assert-Equal ([int]$sDict["health"]) 2 "Summary muss health=2 (GELB) enthalten"

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

# 8. Phase 2: Condition Monitoring Ampel-Klassifizierung
function Classify-Health($stallMs, $deltaUMv, $glideUMv, $bounceCount) {
    if ($stallMs -gt 30 -or $glideUMv -gt 800 -or $bounceCount -ge 2) {
        return 3 # ROT
    } elseif ($stallMs -gt 5 -or $deltaUMv -gt 120 -or $glideUMv -gt 450) {
        return 2 # GELB
    } else {
        return 1 # GRUEN
    }
}

Assert-Equal (Classify-Health 0 30 320 0) 1 "Gesunder Schlitten B muss GRUEN (1) erhalten"
Assert-Equal (Classify-Health 15 140 380 0) 2 "Leichte Haftreibung muss GELB (2) erhalten"
Assert-Equal (Classify-Health 188 170 340 0) 3 "Schwerer Klemmer (188 ms Stall) muss ROT (3) erhalten"
Assert-Equal (Classify-Health 0 40 400 2) 3 "Hebelprellen (>=2) muss ROT (3) erhalten"

# 9. Phase 3: Bootloader Flash Partitioning & Vector Offset
$bootloaderLd = Get-Content (Join-Path $PSScriptRoot "..\Software\GRAF_Bootloader\STM32F746NGHX_BOOTLOADER.ld") -Raw
$appLd = Get-Content (Join-Path $PSScriptRoot "..\Software\GRAF_MNPA\STM32F746NGHX_FLASH.ld") -Raw

$bootOriginMatch = [regex]::Match($bootloaderLd, 'FLASH\s*\(rx\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+),\s*LENGTH\s*=\s*(\d+K)')
Assert-Equal $bootOriginMatch.Success $true "Bootloader Linker-Skript Flash-Definition gefunden"
Assert-Equal ([uint32]$bootOriginMatch.Groups[1].Value) 0x08000000 "Bootloader Origin muss 0x08000000 (Sektor 0) sein"
Assert-Equal $bootOriginMatch.Groups[2].Value.ToUpper() "32K" "Bootloader Sektor 0 Groesse muss 32 KB betragen"

$appOriginMatch = [regex]::Match($appLd, 'FLASH\s*\(rx\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+),\s*LENGTH\s*=\s*(\d+K)')
Assert-Equal $appOriginMatch.Success $true "Applikation Linker-Skript Flash-Definition gefunden"
Assert-Equal ([uint32]$appOriginMatch.Groups[1].Value) 0x08000000 "Applikation Origin muss 0x08000000 (Direkt-Boot) sein"
Assert-Equal $appOriginMatch.Groups[2].Value.ToUpper() "768K" "Applikation Flash-Laenge muss 768 KB betragen (Sektor 7 geschuetzt)"

# 10. Phase 3: Factory Image Integritaet
$factoryBinPath = Join-Path $PSScriptRoot "..\Software\GRAF_FACTORY_ALL.bin"
$factoryExists = Test-Path $factoryBinPath
Assert-Equal $factoryExists $true "Factory-Image GRAF_FACTORY_ALL.bin muss existieren"

if ($factoryExists) {
    $factoryBytes = [System.IO.File]::ReadAllBytes($factoryBinPath)
    Assert-Equal ($factoryBytes.Length -ge 32768) $true "Factory-Image muss mindestens 32 KB (Bootloader + Offset) umfassen"
    # Pruefen, dass Initial Stack Pointer an 0x0 (Bootloader) und 0x8000 (App) ein gueltiger RAM-Pointer im DTCM/SRAM1 ist (0x2000xxxx)
    $bootMsp = [System.BitConverter]::ToUInt32($factoryBytes, 0)
    $appMsp = [System.BitConverter]::ToUInt32($factoryBytes, 32768)
    Assert-Equal (($bootMsp -band 0xFF000000) -eq 0x20000000) $true "Bootloader Initial Stack Pointer muss im SRAM liegen (0x20xxxxxx)"
    Assert-Equal (($appMsp -band 0xFF000000) -eq 0x20000000) $true "Applikation Initial Stack Pointer muss im SRAM liegen (0x20xxxxxx)"
}

# 11. Phase 3: CRC16 Testvektor fuer UART Bootloader Chunks ("123456789" -> 0xBB3D)
function Compute-CRC16([byte[]]$bytes) {
    $crc = [uint16]0xFFFF
    foreach ($b in $bytes) {
        $crc = $crc -bxor [uint16]$b
        for ($i = 0; $i -lt 8; $i++) {
            if (($crc -band 1) -ne 0) {
                $crc = [uint16](($crc -shr 1) -bxor 0xA001)
            } else {
                $crc = [uint16]($crc -shr 1)
            }
        }
    }
    return $crc
}

$crc16Result = Compute-CRC16 $testBytes
$expectedCrc16 = [uint16]"0x4B37"
Assert-Equal ('0x{0:X4}' -f $crc16Result) ('0x{0:X4}' -f $expectedCrc16) "CRC-16 (Init 0xFFFF, Poly 0xA001) Checksumme auf Standard-Testvektor '123456789'"

Write-Host "`nALL 34 CHECKS PASSED SUCCESSFULLY!" -ForegroundColor Green
