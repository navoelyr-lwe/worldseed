# Worldseed Engine — convert a binary PPM (P6) frame to PNG.
#
# Developer/release tooling only; it never ships inside the product. Used by the
# Windows release and GUI workflows to turn captured frames into PNG previews
# that can be attached to a release and viewed by the project owner.
param(
    [Parameter(Mandatory = $true)][string]$In,
    [Parameter(Mandatory = $true)][string]$Out
)

$data = [System.IO.File]::ReadAllBytes($In)
$script:i = 0

function Read-PpmToken {
    while ($script:i -lt $data.Length) {
        $c = $data[$script:i]
        if ($c -eq 35) { while ($script:i -lt $data.Length -and $data[$script:i] -ne 10) { $script:i += 1 }; continue }
        if ($c -eq 32 -or $c -eq 9 -or $c -eq 13 -or $c -eq 10) { $script:i += 1; continue }
        break
    }
    $sb = New-Object System.Text.StringBuilder
    while ($script:i -lt $data.Length) {
        $c = $data[$script:i]
        if ($c -eq 32 -or $c -eq 9 -or $c -eq 13 -or $c -eq 10) { break }
        [void]$sb.Append([char]$c)
        $script:i += 1
    }
    return $sb.ToString()
}

$magic = Read-PpmToken
if ($magic -ne "P6") { throw "not a binary PPM (P6): $magic" }
$w = [int](Read-PpmToken)
$h = [int](Read-PpmToken)
$maxval = [int](Read-PpmToken)
if ($w -le 0 -or $h -le 0 -or $maxval -ne 255) { throw "unsupported PPM header" }
# Skip exactly one whitespace delimiter after maxval (handles CRLF).
if ($data[$script:i] -eq 13) { $script:i += 1 }
if ($data[$script:i] -eq 10) { $script:i += 1 } elseif ($data[$script:i] -eq 32 -or $data[$script:i] -eq 9) { $script:i += 1 }
$raster = $script:i

Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
$locked = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $bmp.PixelFormat)
$stride = $locked.Stride
$buf = New-Object byte[] ($stride * $h)
for ($y = 0; $y -lt $h; $y++) {
    $src = $raster + $y * $w * 3
    $dst = $y * $stride
    for ($x = 0; $x -lt $w; $x++) {
        $buf[$dst + $x * 3 + 0] = $data[$src + $x * 3 + 2] # blue
        $buf[$dst + $x * 3 + 1] = $data[$src + $x * 3 + 1] # green
        $buf[$dst + $x * 3 + 2] = $data[$src + $x * 3 + 0] # red
    }
}
[System.Runtime.InteropServices.Marshal]::Copy($buf, 0, $locked.Scan0, $buf.Length)
$bmp.UnlockBits($locked)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "ppm2png $In -> $Out"
