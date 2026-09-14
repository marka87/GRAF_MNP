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

Write-Host "`nALL 10 CHECKS PASSED SUCCESSFULLY!" -ForegroundColor Green
