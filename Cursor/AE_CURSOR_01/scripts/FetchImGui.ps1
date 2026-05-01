$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $root "third_party\imgui"

if (Test-Path (Join-Path $dest "imgui.cpp")) {
    Write-Host "ImGui already present at: $dest"
    exit 0
}

Write-Host "Cloning Dear ImGui into: $dest"
New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null

if (Test-Path $dest) {
    Remove-Item -Recurse -Force $dest
}

git clone --depth 1 https://github.com/ocornut/imgui.git $dest

if (-not (Test-Path (Join-Path $dest "imgui.cpp"))) {
    throw "ImGui clone failed (imgui.cpp missing)."
}

Write-Host "Done."
