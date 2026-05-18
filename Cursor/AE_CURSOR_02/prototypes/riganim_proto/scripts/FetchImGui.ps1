$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $root "third_party\imgui"
if (Test-Path (Join-Path $dest "imgui.cpp")) {
    Write-Host "ImGui already present."
    exit 0
}
$src = "G:\mine\AEDD_AI_EXPERIMENTS\Cursor\AE_CURSOR_01\third_party\imgui"
if (Test-Path (Join-Path $src "imgui.cpp")) {
    Write-Host "Copying ImGui from AE_CURSOR_01..."
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item -Recurse -Force $src $dest
    exit 0
}
Write-Host "Cloning Dear ImGui..."
New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
git clone --depth 1 https://github.com/ocornut/imgui.git $dest
