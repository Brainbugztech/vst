# Brain Bugz Cloud - legt die Ordnerstruktur im Google Drive an
# Laeuft nur Ordner anlegen, loescht oder verschiebt nichts.
$ErrorActionPreference = 'Stop'

$struktur = @(
  '_Inbox (zum Einsortieren)',
  '01_Projekte',
  '01_Projekte\_Vorlagen',
  '01_Projekte\_Archiv',
  '02_Presets',
  '02_Presets\Ableton Racks',
  '02_Presets\Synth Presets',
  '02_Presets\Brain Bugz Plugin Presets',
  '02_Presets\Presetpacks',
  '03_Samples',
  '03_Samples\Drums\Kicks',
  '03_Samples\Drums\Snares und Claps',
  '03_Samples\Drums\HiHats und Cymbals',
  '03_Samples\Drums\Percussion',
  '03_Samples\Drums\Drum Loops',
  '03_Samples\Bass',
  '03_Samples\Melodie und Chords',
  '03_Samples\Vocals',
  '03_Samples\FX',
  '03_Samples\Eigene Aufnahmen',
  '03_Samples\Samplepacks',
  '04_Plugins',
  '04_Plugins\Brain Bugz Builds',
  '04_Plugins\Brain Bugz Builds\Brainbugz Haas',
  '04_Plugins\Brain Bugz Builds\Phazer',
  '04_Plugins\Brain Bugz Builds\Bleep Designer',
  '04_Plugins\Brain Bugz Builds\Bugz Align',
  '04_Plugins\Installer (Fremd-Plugins)',
  '05_Tracks',
  '05_Tracks\Fertig (Master)',
  '05_Tracks\Stems',
  '05_Tracks\Demos und Previews',
  '05_Tracks\Releases (Artwork, Infos)'
)

Write-Host ''
Write-Host '=== Brain Bugz Cloud einrichten ===' -ForegroundColor Cyan
Write-Host ''

# Google Drive fuer Desktop suchen ("Meine Ablage" / "My Drive" auf irgendeinem Laufwerk)
$ziel = $null
foreach ($lw in (Get-PSDrive -PSProvider FileSystem)) {
  foreach ($name in 'Meine Ablage','My Drive') {
    $p = Join-Path $lw.Root $name
    if (Test-Path $p) { $ziel = $p; break }
  }
  if ($ziel) { break }
}

if ($ziel) {
  Write-Host "Google Drive gefunden: $ziel" -ForegroundColor Green
  $ok = Read-Host 'Dort anlegen? (j/n)'
  if ($ok -notmatch '^[jJyY]') { $ziel = $null }
} else {
  Write-Host 'Google Drive fuer Desktop wurde nicht gefunden.' -ForegroundColor Yellow
  Write-Host 'Installieren: https://www.google.com/drive/download/  (danach mit deinem Google-Konto anmelden)'
}

if (-not $ziel) {
  $ziel = Read-Host 'Pfad eingeben, in dem "Brain Bugz Cloud" angelegt werden soll (z.B. H:\Meine Ablage)'
  $ziel = $ziel.Trim('"',' ')
  if (-not (Test-Path $ziel)) { Write-Host "Pfad gibt es nicht: $ziel" -ForegroundColor Red; Read-Host 'Enter zum Beenden'; exit 1 }
}

$root = Join-Path $ziel 'Brain Bugz Cloud'
$neu = 0
foreach ($s in $struktur) {
  $p = Join-Path $root $s
  if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null; $neu++ }
}

$liesMich = Join-Path $PSScriptRoot 'LIES_MICH.txt'
if (Test-Path $liesMich) { Copy-Item $liesMich (Join-Path $root 'LIES_MICH.txt') -Force }

Write-Host ''
Write-Host "Fertig: $neu Ordner neu angelegt in $root" -ForegroundColor Green
Write-Host 'Naechster Schritt: auf drive.google.com den Ordner "Brain Bugz Cloud" mit Dexter teilen (Rechtsklick > Teilen > Bearbeiter).'
Start-Process explorer.exe $root
Read-Host 'Enter zum Beenden'
