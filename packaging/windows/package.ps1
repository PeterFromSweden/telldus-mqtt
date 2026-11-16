# Puts telldus-mqtt.exe, the DLLs it needs, the config files and README.txt
# in one folder. Run in a Visual Studio developer shell (dumpbin, VCToolsRedistDir).
#   package.ps1 -BuildDir build -Config Release -VcpkgInstalled C:\vcpkg\installed\x64-windows-release -OutDir dist
param(
  [Parameter(Mandatory)] [string] $BuildDir,
  [Parameter(Mandatory)] [string] $Config,
  [Parameter(Mandatory)] [string] $VcpkgInstalled,
  [Parameter(Mandatory)] [string] $OutDir
)
$ErrorActionPreference = 'Stop'
$repo = Resolve-Path "$PSScriptRoot\..\.."

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Copy-Item "$BuildDir\$Config\telldus-mqtt.exe" $OutDir
Copy-Item "$repo\telldus-mqtt.json" $OutDir
Copy-Item "$BuildDir\telldus-mqtt-homeassistant.json" $OutDir
Copy-Item "$PSScriptRoot\README.txt" $OutDir
Copy-Item "$repo\LICENSE" "$OutDir\LICENSE.txt"

# The MSVC runtime DLLs (vcruntime140.dll) go next to the exe ("app-local"),
# so the user does not have to install the Visual C++ Redistributable.
if (-not $env:VCToolsRedistDir) { throw 'VCToolsRedistDir is not set: run in a developer shell' }
$crt = Get-ChildItem "$env:VCToolsRedistDir\x64" -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
if (-not $crt) { throw "No CRT folder in $env:VCToolsRedistDir\x64" }
$vcpkgBin = "$VcpkgInstalled\bin"
$system32 = "$env:SystemRoot\System32"

# Copy each DLL that the exe needs, and the DLLs that those DLLs need.
$queue = New-Object System.Collections.Queue
$queue.Enqueue((Join-Path $OutDir 'telldus-mqtt.exe'))
while ($queue.Count -gt 0) {
  $file = $queue.Dequeue()
  $deps = dumpbin /nologo /dependents $file | ForEach-Object {
    if ($_ -match '^\s+(\S+\.dll)\s*$') { $Matches[1] }
  }
  foreach ($dll in $deps) {
    if (Test-Path (Join-Path $OutDir $dll)) { continue }
    if ($dll -ieq 'TelldusCore.dll') { continue }           # From TelldusCenter
    if ($dll -match '^(api|ext)-ms-') { continue }          # Windows API sets
    $src = $null
    foreach ($dir in @($vcpkgBin, $crt.FullName)) {
      if (Test-Path (Join-Path $dir $dll)) { $src = Join-Path $dir $dll; break }
    }
    if ($src) {
      Write-Host "$dll <- $src"
      Copy-Item $src $OutDir
      $queue.Enqueue((Join-Path $OutDir $dll))
    } elseif (Test-Path (Join-Path $system32 $dll)) {
      Write-Host "$dll (Windows)"
    } else {
      throw "$dll (needed by $file) not found"
    }
  }
}

# License texts of the vcpkg libraries
New-Item -ItemType Directory -Force -Path "$OutDir\licenses" | Out-Null
Get-ChildItem "$VcpkgInstalled\share" -Directory | Where-Object { $_.Name -notlike 'vcpkg-*' } | ForEach-Object {
  $copyright = Join-Path $_.FullName 'copyright'
  if (Test-Path $copyright) { Copy-Item $copyright "$OutDir\licenses\$($_.Name).txt" }
}

Get-ChildItem -Recurse $OutDir | Select-Object FullName, Length | Format-Table -AutoSize
