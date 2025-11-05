#!/bin/bash
# Script to clear ALL NeuralAmpModeler caches on macOS
# Run this before testing a new build to ensure clean state

echo "=== Clearing NeuralAmpModeler Mac Caches ==="
echo ""

# 1. Kill audio processes
echo "1. Killing audio processes..."
sudo killall -9 AudioComponentRegistrar 2>/dev/null || true
sudo killall -9 coreaudiod 2>/dev/null || true
sudo killall -9 NeuralAmpModeler 2>/dev/null || true
echo "   Done"
echo ""

# 2. Remove installed plugins and apps
echo "2. Removing installed plugins and apps..."
sudo rm -rf ~/Library/Audio/Plug-Ins/Components/NeuralAmpModeler.component
sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/NeuralAmpModeler.vst3
sudo rm -rf ~/Library/Audio/Plug-Ins/VST/NeuralAmpModeler.vst
sudo rm -rf /Library/Audio/Plug-Ins/Components/NeuralAmpModeler.component
sudo rm -rf /Library/Audio/Plug-Ins/VST3/NeuralAmpModeler.vst3
sudo rm -rf /Library/Audio/Plug-Ins/VST/NeuralAmpModeler.vst
sudo rm -rf ~/Applications/NeuralAmpModeler.app
sudo rm -rf /Applications/NeuralAmpModeler.app
sudo rm -rf ~/Downloads/NeuralAmpModeler.app
echo "   Done"
echo ""

# 3. Clear application state and preferences
echo "3. Clearing application state and preferences..."
rm -rf ~/Library/Saved\ Application\ State/com.StevenAtkinson.app.NeuralAmpModeler*
rm -rf ~/Library/Preferences/com.StevenAtkinson.app.NeuralAmpModeler.plist
defaults delete com.StevenAtkinson.app.NeuralAmpModeler 2>/dev/null || true
echo "   Done"
echo ""

# 4. Clear system caches
echo "4. Clearing system caches..."
sudo rm -rf /Library/Caches/com.StevenAtkinson.app.NeuralAmpModeler*
rm -rf ~/Library/Caches/com.StevenAtkinson.app.NeuralAmpModeler*
sudo rm -rf /System/Library/Caches/com.apple.audio.InfoCache.plist
echo "   Done"
echo ""

# 5. Clear audio unit cache
echo "5. Clearing Audio Unit cache..."
sudo rm -rf ~/Library/Caches/AudioUnitCache
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "   Done"
echo ""

# 6. Restart Core Audio
echo "6. Restarting Core Audio daemon..."
sudo launchctl kickstart -kp system/com.apple.audio.coreaudiod
echo "   Done"
echo ""

echo "=== Cache Clear Complete ==="
echo ""
echo "IMPORTANT: Reboot your Mac now to ensure completely clean state:"
echo "  sudo reboot"
echo ""
echo "After reboot, install the NEW binary from artifact 4461379434"
