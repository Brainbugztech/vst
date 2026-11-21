#!/bin/bash
# BrainBugz Touch Me! auf dem Mac installieren (VST3 + AU)
cd "$(dirname "$0")"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/BrainBugz Touch Me!.vst3" "$AU/BrainBugz Touch Me!.component"
cp -R "BrainBugz Touch Me!.vst3" "$VST3/"
cp -R "BrainBugz Touch Me!.component" "$AU/"
# Download-Sperre von macOS entfernen (Plugin ist nicht bei Apple signiert)
xattr -dr com.apple.quarantine "$VST3/BrainBugz Touch Me!.vst3" "$AU/BrainBugz Touch Me!.component" 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo ""
echo "Fertig! BrainBugz Touch Me! ist installiert:"
echo "  VST3: $VST3"
echo "  AU:   $AU"
echo "Jetzt Ableton neu starten bzw. Plugins neu scannen."
read -n 1 -s -r -p "Taste druecken zum Schliessen ..."
