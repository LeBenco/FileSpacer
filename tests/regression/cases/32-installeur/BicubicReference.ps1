# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
param([string]$SourcePath,[int]$Width,[int]$Height,[string]$OutputPath)
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'The reference must use x86 GDI+, matching the NSIS process.'}
Add-Type -AssemblyName System.Drawing
$source=New-Object Drawing.Bitmap($SourcePath)
$target=New-Object Drawing.Bitmap($Width,$Height,[Drawing.Imaging.PixelFormat]::Format32bppPArgb)
$graphics=[Drawing.Graphics]::FromImage($target)
try {
    $graphics.Clear([Drawing.Color]::FromArgb(241,242,246))
    $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.DrawImage($source,(New-Object Drawing.Rectangle(0,0,$Width,$Height)),0,0,$source.Width,$source.Height,[Drawing.GraphicsUnit]::Pixel)
    $target.Save($OutputPath,[Drawing.Imaging.ImageFormat]::Png)
} finally { $graphics.Dispose();$target.Dispose();$source.Dispose() }
