# Merges GRAF_Bootloader (0x08000000) and GRAF_MNPA (0x08008000) into a single factory image

$bootloaderBin = "A:\GRAF_MNP\Software\GRAF_Bootloader\GRAF_Bootloader.bin"
$appBin        = "A:\GRAF_MNP\Software\GRAF_MNPA\Debug\GRAF_MNPA.bin"
$factoryBin    = "A:\GRAF_MNP\Software\GRAF_FACTORY_ALL.bin"

if (-not (Test-Path $bootloaderBin)) {
    Write-Error "Bootloader bin not found: $bootloaderBin"
    exit 1
}
if (-not (Test-Path $appBin)) {
    Write-Error "App bin not found: $appBin"
    exit 1
}

$bootBytes = [System.IO.File]::ReadAllBytes($bootloaderBin)
$appBytes  = [System.IO.File]::ReadAllBytes($appBin)

$sector0Size = 32768 # 32 KB
$totalSize = $sector0Size + $appBytes.Length
$factoryBytes = New-Object byte[] $totalSize

# Initialize Sector 0 with 0xFF (erased flash state)
for ($i = 0; $i -lt $sector0Size; $i++) {
    $factoryBytes[$i] = 0xFF
}

# Copy Bootloader into Sector 0
[System.Buffer]::BlockCopy($bootBytes, 0, $factoryBytes, 0, $bootBytes.Length)

# Copy Application starting at 0x8000 (Sector 1)
[System.Buffer]::BlockCopy($appBytes, 0, $factoryBytes, $sector0Size, $appBytes.Length)

[System.IO.File]::WriteAllBytes($factoryBin, $factoryBytes)

Write-Host "Created $factoryBin successfully! Total size: $($factoryBytes.Length) bytes (Bootloader: $($bootBytes.Length) B, App: $($appBytes.Length) B)"
