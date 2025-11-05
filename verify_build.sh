#!/bin/bash
# Verification script to check if Layer 10B is compiled in the binary

echo "=== Build Verification Script ==="
echo ""
echo "Current commit:"
git log --oneline -1
echo ""
echo "AudioDSPTools submodule commit:"
git submodule status AudioDSPTools
echo ""
echo "Layer 10B check in ImpulseResponse.cpp:"
grep -n "Layer 10B" AudioDSPTools/dsp/ImpulseResponse.cpp || echo "WARNING: Layer 10B comment not found!"
echo ""
echo "Checking both constructors have initialization:"
grep -A 2 "ImpulseResponse::ImpulseResponse" AudioDSPTools/dsp/ImpulseResponse.cpp | grep "mRawAudioSampleRate(0.0)" || echo "WARNING: Initialization missing!"
echo ""
echo "Expected offset: Should be different from 745664"
echo "Previous offsets:"
echo "  Layers 7-9: 745664 (0xB60C0)"
echo "  Layer 10 (file constructor only): 745680 (0xB60D0)"
echo "  Layer 10B (both constructors): Should be different again"
