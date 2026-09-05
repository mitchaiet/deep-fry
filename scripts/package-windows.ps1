# SPDX-License-Identifier: AGPL-3.0-only
# Package an existing Windows x64 Release build. Never builds or installs it.
[CmdletBinding()]
param(
    [string] $Artifacts = "",
    [string] $Output = "",
    [string] $SourceArchive = "",
    [switch] $AllowDirty
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if ($env:OS -ne "Windows_NT") { throw "Windows packaging requires Windows and Visual Studio C++ tools." }

$projectDir = Split-Path -Parent $PSScriptRoot
if (-not $Artifacts) { $Artifacts = Join-Path $projectDir "build/DeepFry_artefacts/Release" }
if (-not $Output) { $Output = Join-Path $projectDir "dist" }
$artifactsDir = (Resolve-Path -LiteralPath $Artifacts).Path
$cmake = Get-Content -LiteralPath (Join-Path $projectDir "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match($cmake, 'project\(DeepFry VERSION (\d+\.\d+\.\d+) ')
if (-not $versionMatch.Success) { throw "Cannot read the project version from CMakeLists.txt." }
$version = $versionMatch.Groups[1].Value
$juceCommit = [regex]::Match($cmake, 'codeload.github.com/juce-framework/JUCE/tar.gz/([a-f0-9]{40})').Groups[1].Value
$juceSha = [regex]::Match($cmake, 'URL_HASH SHA256=([a-f0-9]{64})').Groups[1].Value
if (-not $juceCommit -or -not $juceSha) { throw "Cannot read the immutable JUCE source pin." }

function Invoke-ProjectGit {
    param([string[]] $GitArguments)
    $result = & git -C $projectDir @GitArguments
    if ($LASTEXITCODE -ne 0) { throw "Git command failed: $($GitArguments -join ' ')" }
    return ($result -join "`n").Trim()
}

$sourceCommit = Invoke-ProjectGit @("rev-parse", "HEAD")
$sourceDirty = [bool](Invoke-ProjectGit @("status", "--porcelain"))
if ($sourceCommit -notmatch '^[a-f0-9]{40}$') { throw "Cannot read a complete Git commit ID." }
if ($sourceDirty -and -not $AllowDirty) {
    throw "Commit source changes before packaging a release. -AllowDirty is for local checks only."
}

# dumpbin is part of the Visual Studio C++ toolchain used to build this package.
$dumpbinCommand = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if ($dumpbinCommand) {
    $dumpbin = $dumpbinCommand.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw "Cannot locate Visual Studio's dumpbin.exe." }
    $dumpbinCandidates = @(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe')
    if ($LASTEXITCODE -ne 0 -or $dumpbinCandidates.Count -eq 0) { throw "Visual Studio C++ x64 tools are required." }
    $dumpbin = $dumpbinCandidates[0]
}

function Get-VerifiedBinary {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing binary: $Path" }
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5A4D) { throw "Not a DOS/PE executable: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadUInt32()
        if ($peOffset -gt $stream.Length - 26) { throw "Invalid PE header offset: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Missing PE signature: $Path" }
        if ($reader.ReadUInt16() -ne 0x8664) { throw "Expected an x64 (AMD64) binary: $Path" }
        $stream.Position = $peOffset + 24
        if ($reader.ReadUInt16() -ne 0x20B) { throw "Expected a PE32+ executable: $Path" }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
    $binaryVersion = (Get-Item -LiteralPath $Path).VersionInfo.ProductVersion
    if ($binaryVersion -notmatch ('^' + [regex]::Escape($version) + '(?:\.0)?$')) {
        throw "Binary version '$binaryVersion' does not match project version $version`: $Path"
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($signature.Status -ne "NotSigned") {
        throw "Expected an unsigned development binary. Update signing metadata before packaging another signature type: $Path"
    }
    $dependencyOutput = & $dumpbin /NOLOGO /DEPENDENTS $Path
    if ($LASTEXITCODE -ne 0) { throw "Could not inspect DLL dependencies: $Path" }
    $imports = @($dependencyOutput | ForEach-Object {
        if ($_ -match '^\s*([A-Za-z0-9_.-]+\.dll)\s*$') { $Matches[1].ToLowerInvariant() }
    } | Sort-Object -Unique)
    if ($imports.Count -eq 0) { throw "No Windows DLL imports found; dependency inspection failed: $Path" }
    $runtimeDlls = @($imports | Where-Object { $_ -match '^(msvcp\d|msvcr\d|vcruntime\d|concrt\d|ucrtbased)' })
    if ($runtimeDlls.Count -ne 0) {
        throw "This release requires a static MSVC runtime; unexpected redistributable DLL imports: $($runtimeDlls -join ', ')"
    }
    return [ordered]@{
        architecture = "x64"
        pe_format = "PE32+"
        version = $binaryVersion
        authenticode = "unsigned"
        imported_dlls = $imports
    }
}

$vstBundle = Join-Path $artifactsDir "VST3/Deep Fry.vst3"
$vstRelative = "VST3/Deep Fry.vst3/Contents/x86_64-win/Deep Fry.vst3"
$standaloneRelative = "Standalone/Deep Fry.exe"
$binaryInfo = [ordered]@{}
foreach ($relative in @($vstRelative, $standaloneRelative)) {
    $binaryInfo[$relative] = Get-VerifiedBinary (Join-Path $artifactsDir $relative)
}
$vstManifest = Join-Path $vstBundle "Contents/Resources/moduleinfo.json"
if (-not (Test-Path -LiteralPath $vstManifest -PathType Leaf)) { throw "VST3 moduleinfo.json is missing." }
$moduleInfo = Get-Content -LiteralPath $vstManifest -Raw | ConvertFrom-Json
if ($moduleInfo.Version -ne $version) { throw "VST3 manifest version does not match the project." }

foreach ($required in @("LICENSE", "COPYRIGHT", "CHANGELOG.md", "THIRD_PARTY_NOTICES.md", "packaging/README-Windows.txt")) {
    if (-not (Test-Path -LiteralPath (Join-Path $projectDir $required) -PathType Leaf)) { throw "Missing $required" }
}
$licenseDir = Join-Path $projectDir "packaging/LICENSES"
if (@(Get-ChildItem -LiteralPath $licenseDir -File).Count -eq 0) { throw "Missing dependency license notices." }
$sourceName = "Deep-Fry-$version-source.tar.gz"
$correspondingSource = [ordered]@{
    file = $sourceName
    release_url = "https://github.com/mitchaiet/deep-fry/releases/tag/v$version"
    download_url = "https://github.com/mitchaiet/deep-fry/releases/download/v$version/$sourceName"
}
if ($SourceArchive) {
    $sourceFile = Get-Item -LiteralPath $SourceArchive
    if ($sourceFile.Name -ne $sourceName) { throw "Expected corresponding-source archive named $sourceName." }
    $correspondingSource["sha256"] = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}

New-Item -ItemType Directory -Path $Output -Force | Out-Null
$outputDir = (Resolve-Path -LiteralPath $Output).Path
$packageName = "Deep-Fry-$version-Windows-x64"
$stage = Join-Path $outputDir (".deep-fry-package-" + [guid]::NewGuid().ToString("N"))
$packageDir = Join-Path $stage $packageName
try {
    New-Item -ItemType Directory -Path (Join-Path $packageDir "VST3"), (Join-Path $packageDir "Standalone") -Force | Out-Null
    Copy-Item -LiteralPath $vstBundle -Destination (Join-Path $packageDir "VST3") -Recurse
    Copy-Item -LiteralPath (Join-Path $artifactsDir $standaloneRelative) -Destination (Join-Path $packageDir $standaloneRelative)
    Copy-Item -LiteralPath $licenseDir -Destination (Join-Path $packageDir "LICENSES") -Recurse
    foreach ($name in @("LICENSE", "CHANGELOG.md")) {
        Copy-Item -LiteralPath (Join-Path $projectDir $name) -Destination (Join-Path $packageDir $name)
    }
    foreach ($name in @("COPYRIGHT", "THIRD_PARTY_NOTICES.md")) {
        $notice = (Get-Content -LiteralPath (Join-Path $projectDir $name) -Raw).Replace("packaging/LICENSES/", "LICENSES/")
        [IO.File]::WriteAllText((Join-Path $packageDir $name), $notice, [Text.UTF8Encoding]::new($false))
    }
    $readme = (Get-Content -LiteralPath (Join-Path $projectDir "packaging/README-Windows.txt") -Raw).Replace("@VERSION@", $version)
    [IO.File]::WriteAllText((Join-Path $packageDir "README.txt"), $readme, [Text.UTF8Encoding]::new($false))
    Get-ChildItem -LiteralPath $packageDir -Recurse -File | Where-Object { $_.Extension -in @(".pdb", ".ilk", ".exp", ".lib") -or $_.Name -eq ".DS_Store" } | Remove-Item -Force

    $fileHashes = [ordered]@{}
    foreach ($file in @(Get-ChildItem -LiteralPath $packageDir -Recurse -File | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($packageDir.Length + 1).Replace('\', '/')
        $fileHashes[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $manifest = [ordered]@{
        project = "Deep Fry"
        version = $version
        license = "AGPL-3.0-only"
        source_repository = "https://github.com/mitchaiet/deep-fry"
        source_commit = $sourceCommit
        source_tree_dirty = $sourceDirty
        platform = "Windows"
        architectures = @("x64")
        minimum_windows = "10"
        authenticode = "unsigned"
        msvc_runtime = "static"
        corresponding_source = $correspondingSource
        juce = [ordered]@{ commit = $juceCommit; archive_sha256 = $juceSha }
        binaries = $binaryInfo
        files_sha256 = $fileHashes
    }
    [IO.File]::WriteAllText((Join-Path $packageDir "RELEASE-MANIFEST.json"), ($manifest | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
    $zipPath = Join-Path $stage "$packageName.zip"
    Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -CompressionLevel Optimal
    $zipSha = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText("$zipPath.sha256", "$zipSha  $packageName.zip`n", [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $zipPath -Destination (Join-Path $outputDir "$packageName.zip") -Force
    Move-Item -LiteralPath "$zipPath.sha256" -Destination (Join-Path $outputDir "$packageName.zip.sha256") -Force
    Write-Host "Created $(Join-Path $outputDir "$packageName.zip")"
    Write-Host "Created $(Join-Path $outputDir "$packageName.zip.sha256")"
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
