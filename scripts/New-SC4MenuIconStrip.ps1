param(
    [Parameter(Mandatory = $true)]
    [string]$ColorImage,

    [Parameter(Mandatory = $true)]
    [string]$OutputImage,

    [string]$MonochromeImage = "",

    [int]$FrameSize = 44,

    [int]$IconSize = 28
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

function New-Color([string]$hex) {
    return [System.Drawing.ColorTranslator]::FromHtml($hex)
}

function Draw-Frame(
    [System.Drawing.Graphics]$Graphics,
    [int]$X,
    [string]$Background,
    [string]$Outer,
    [string]$InnerDark,
    [string]$InnerLight,
    [bool]$Pressed,
    [bool]$Hover,
    [int]$FrameSize)
{
    $bg = New-Object System.Drawing.SolidBrush((New-Color $Background))
    $Graphics.FillRectangle($bg, $X + 1, 1, $FrameSize - 2, $FrameSize - 2)
    $bg.Dispose()

    $outerPen = New-Object System.Drawing.Pen((New-Color $Outer), 1)
    $Graphics.DrawRectangle($outerPen, $X, 0, $FrameSize - 1, $FrameSize - 1)
    $outerPen.Dispose()

    $darkPen = New-Object System.Drawing.Pen((New-Color $InnerDark), 1)
    $lightPen = New-Object System.Drawing.Pen((New-Color $InnerLight), 1)

    if ($Pressed) {
        $Graphics.DrawLine($darkPen, $X + 1, 1, $X + $FrameSize - 2, 1)
        $Graphics.DrawLine($darkPen, $X + 1, 1, $X + 1, $FrameSize - 2)
        $Graphics.DrawLine($lightPen, $X + 1, $FrameSize - 2, $X + $FrameSize - 2, $FrameSize - 2)
        $Graphics.DrawLine($lightPen, $X + $FrameSize - 2, 1, $X + $FrameSize - 2, $FrameSize - 2)
        $Graphics.DrawRectangle($darkPen, $X + 2, 2, $FrameSize - 5, $FrameSize - 5)
    }
    else {
        $Graphics.DrawLine($lightPen, $X + 1, 1, $X + $FrameSize - 2, 1)
        $Graphics.DrawLine($lightPen, $X + 1, 1, $X + 1, $FrameSize - 2)
        $Graphics.DrawLine($darkPen, $X + 1, $FrameSize - 2, $X + $FrameSize - 2, $FrameSize - 2)
        $Graphics.DrawLine($darkPen, $X + $FrameSize - 2, 1, $X + $FrameSize - 2, $FrameSize - 2)
        $Graphics.DrawRectangle($darkPen, $X + 2, 2, $FrameSize - 5, $FrameSize - 5)
    }

    $darkPen.Dispose()
    $lightPen.Dispose()

    if ($Hover) {
        $hoverPen = New-Object System.Drawing.Pen((New-Color '#FFF7CF'), 1)
        $Graphics.DrawRectangle($hoverPen, $X, 0, $FrameSize - 1, $FrameSize - 1)
        $Graphics.DrawRectangle($hoverPen, $X + 3, 3, $FrameSize - 7, $FrameSize - 7)
        $hoverPen.Dispose()
    }
}

$resolvedColor = (Resolve-Path $ColorImage).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputImage)
$resolvedMono = if ($MonochromeImage) { (Resolve-Path $MonochromeImage).Path } else { "" }

$color = [System.Drawing.Bitmap]::FromFile($resolvedColor)
$mono = if ($resolvedMono) { [System.Drawing.Bitmap]::FromFile($resolvedMono) } else { $null }

$pixelFormat = [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
$canvas = New-Object System.Drawing.Bitmap(($FrameSize * 4), $FrameSize, $pixelFormat)
$graphics = [System.Drawing.Graphics]::FromImage($canvas)
$graphics.Clear([System.Drawing.Color]::Transparent)

$states = @(
    @{
        Background = '#D3D3CE'
        Outer = '#B9B9B2'
        InnerDark = '#A0A097'
        InnerLight = '#F1F1ED'
        UseMono = $true
        Hover = $false
        Pressed = $false
        Alpha = 0.42
    },
    @{
        Background = '#D4C18E'
        Outer = '#887349'
        InnerDark = '#7B6841'
        InnerLight = '#F8EFCB'
        UseMono = $false
        Hover = $false
        Pressed = $false
        Alpha = 1.0
    },
    @{
        Background = '#D8C695'
        Outer = '#CDB777'
        InnerDark = '#8B774A'
        InnerLight = '#FFF8D8'
        UseMono = $false
        Hover = $true
        Pressed = $false
        Alpha = 1.0
    },
    @{
        Background = '#C4AE75'
        Outer = '#786339'
        InnerDark = '#6A572F'
        InnerLight = '#E4D09D'
        UseMono = $false
        Hover = $false
        Pressed = $true
        Alpha = 1.0
    }
)

try {
    for ($i = 0; $i -lt $states.Count; $i++) {
        $state = $states[$i]
        $x = $i * $FrameSize

        Draw-Frame `
            -Graphics $graphics `
            -X $x `
            -Background $state.Background `
            -Outer $state.Outer `
            -InnerDark $state.InnerDark `
            -InnerLight $state.InnerLight `
            -Pressed $state.Pressed `
            -Hover $state.Hover `
            -FrameSize $FrameSize

        $source = if ($state.UseMono -and $mono) { $mono } else { $color }

        $frame = New-Object System.Drawing.Bitmap($IconSize, $IconSize, $pixelFormat)
        $frameGraphics = [System.Drawing.Graphics]::FromImage($frame)
        $frameGraphics.Clear([System.Drawing.Color]::Transparent)

        $attrs = New-Object System.Drawing.Imaging.ImageAttributes
        $matrix = New-Object System.Drawing.Imaging.ColorMatrix
        $matrix.Matrix00 = 1.0
        $matrix.Matrix11 = 1.0
        $matrix.Matrix22 = 1.0
        $matrix.Matrix33 = [single]$state.Alpha
        $matrix.Matrix44 = 1.0

        if ($state.UseMono) {
            $matrix.Matrix00 = 0.82
            $matrix.Matrix11 = 0.82
            $matrix.Matrix22 = 0.82
        }

        $attrs.SetColorMatrix($matrix)
        $destRect = [System.Drawing.Rectangle]::new(0, 0, $IconSize, $IconSize)
        $frameGraphics.DrawImage(
            $source,
            $destRect,
            0,
            0,
            $source.Width,
            $source.Height,
            [System.Drawing.GraphicsUnit]::Pixel,
            $attrs)

        $attrs.Dispose()
        $frameGraphics.Dispose()

        $iconInset = [Math]::Max(0, [int](($FrameSize - $IconSize) / 2))
        $graphics.DrawImage($frame, $x + $iconInset, $iconInset - 1, $IconSize, $IconSize)
        $frame.Dispose()

        if ($state.UseMono) {
            $veil = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(72, 255, 255, 255))
            $graphics.FillRectangle($veil, $x + $iconInset, $iconInset - 1, $IconSize, $IconSize)
            $veil.Dispose()
        }
    }

    $outDir = Split-Path -Parent $resolvedOutput
    if ($outDir) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }

    $canvas.Save($resolvedOutput, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $resolvedOutput
}
finally {
    $graphics.Dispose()
    $canvas.Dispose()
    $color.Dispose()
    if ($mono) {
        $mono.Dispose()
    }
}
