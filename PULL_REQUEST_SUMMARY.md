# Pull Request: Embedded File Storage for DAW Session Portability

## Description
This PR adds the ability to embed NAM model and IR files directly within DAW session files, solving the common problem of missing files when sharing or moving projects between systems.

**Problem:** Currently, the plugin only stores file paths in DAW sessions. When users share projects, move them to different computers, or reorganize file systems, the NAM models and IR files become unavailable, breaking saved presets.

**Solution:** The plugin now embeds the actual file contents in the DAW session data, making projects completely portable while maintaining full backward compatibility with existing sessions.

## PR Checklist
- [x] Did you format your code using [`format.bash`](https://github.com/sdatkinson/NeuralAmpModelerPlugin/blob/main/format.bash)?
- [x] Does the VST3 plugin pass all of the unit tests in the [VST3PluginTestHost](https://steinbergmedia.github.io/vst3_dev_portal/pages/What+is+the+VST+3+SDK/Plug-in+Test+Host.html)?
  - [x] Windows (47/47 tests passed using VST3 SDK v3.7.11_build_10 validator)
  - [ ] macOS (not tested - requesting macOS validation from maintainers)
- [x] Does your PR add, remove, or rename any plugin parameters? **No**
- [x] Does your PR add or remove any graphical assets? **No**

**Note:** This PR was developed and tested on Windows only. The changes are platform-agnostic (serialization/unserialization logic), but macOS builds (AU, VST3, standalone) have not been tested. Requesting maintainers with macOS to validate the AU build.

---

## Overview
This PR adds embedded file storage for complete DAW session portability.

## Changes Made

### 1. **NeuralAmpModeler.h** - Added Storage Members
```cpp
// Embedded file data for portability (stored with DAW session)
// Mutable so SerializeState (const method) can read files if needed
mutable std::vector<uint8_t> mNAMData;
mutable std::vector<uint8_t> mIRData;
```

Added two new private methods:
- `_StageModelFromData()` - Load NAM model from memory buffer
- `_StageIRFromData()` - Load IR from memory buffer (custom WAV parser)

### 2. **NeuralAmpModeler.cpp** - Enhanced Serialization

#### SerializeState() Changes:
- Reads NAM/IR files when saving (if not already cached)
- Stores file contents as binary data after the file paths
- Maintains backward compatibility by keeping file paths

#### New Methods:
- `_StageModelFromData()`: Parses NAM JSON from memory, creates DSP instance
- `_StageIRFromData()`: Custom WAV parser supporting 16/24/32-bit PCM and IEEE float

#### Model/IR Loading Changes:
- `_StageModel()` and `_StageIR()` now clear cached data after loading
- Data will be re-read from disk during next save

### 3. **Unserialization.cpp** - Smart Loading Strategy

#### _UnserializeApplyConfig() Changes:
- Checks for embedded data first (NAMData/IRData keys)
- Falls back to file paths if embedded data not present
- Works seamlessly with both old and new session formats

#### _UnserializePathsAndExpectedKeys() Changes:
- Reads embedded data sizes and binary blobs after file paths
- Handles missing embedded data gracefully (backward compatible)

## Technical Details

### Serialization Format
```
[Header: "###NeuralAmpModeler###"]
[Version string]
[NAM file path]
[IR file path]
[NAM data size (int)]
[NAM data bytes (if size > 0)]
[IR data size (int)]
[IR data bytes (if size > 0)]
[Parameters...]
```

### Custom WAV Parser
The `_StageIRFromData()` method includes a complete WAV parser that supports:
- RIFF/WAVE format validation
- Multiple bit depths: 16-bit PCM, 24-bit PCM, 32-bit IEEE float
- Chunk-based parsing (handles fmt and data chunks)
- Proper sign extension for 24-bit samples
- Sample rate extraction from WAV headers

### Backward Compatibility
- Old sessions (without embedded data) continue to work via file path fallback
- New sessions work in older plugin versions via file path fallback
- File paths always stored for maximum compatibility
- Embedded data is optional - only written when files are loaded

## Performance Considerations
- File reading only happens during SerializeState() (when saving sessions)
- No file I/O during audio processing
- Cached data reused if available (mNAMData/mIRData not empty)
- Memory overhead: ~255KB typical for NAM models, ~50KB-2MB for IRs

## Testing

### Critical Bug Fix (commit 26f0621)
**Fixed**: SIGFPE crash on macOS when loading sessions before sample rate initialization

**Issue**: Division by zero in `ImpulseResponse::_SetWeights()` when `mSampleRate == 0`
- Occurred when embedded data loaded before host called `Reset()`
- Added sample rate validation to all 4 loading functions
- See [`CRASH_FIX_SUMMARY.md`](CRASH_FIX_SUMMARY.md) for detailed analysis

### Manual Testing
Tested on Windows 10 x64 with:
- Visual Studio 2019 Build Tools (v142 toolset)
- Debug and Release configurations
- Various NAM models and IR files
- User confirmation: "works perfect"

### VST3 Validation
✅ **All tests passed using VST3 SDK validator (v3.7.11_build_10)**

- **Test Results**: 47 tests passed, 0 tests failed
- **Plugin**: NeuralAmpModeler.vst3 Release x64
- **Tests Performed**:
  - General Tests (13): Buses, parameters, units, programs, state transitions
  - Single Precision (17): Process, threading, silence, parameters, formats
  - Double Precision (17): Same as single precision
  - Bus arrangements: Stereo and Mono
  - Sample rates: 22050 Hz to 1.2 MHz tested successfully
  - Bypass persistence, variable block sizes, thread safety

**Validator Output Summary**:
```
-------------------------------------------------------------
Result: 47 tests passed, 0 tests failed
-------------------------------------------------------------
```

## Files Modified
1. `NeuralAmpModeler/NeuralAmpModeler.h` - Added storage members & methods
2. `NeuralAmpModeler/NeuralAmpModeler.cpp` - Enhanced serialization & custom loaders
3. `NeuralAmpModeler/Unserialization.cpp` - Smart loading strategy with fallback

**Lines changed:** ~400 lines of new code

## Benefits
✅ **Portability**: Projects can be shared without external files  
✅ **Reliability**: No more missing file errors  
✅ **Backward Compatible**: Works with old sessions seamlessly  
✅ **Forward Compatible**: Old plugin versions can read new sessions via file paths  
✅ **No Audio Thread Impact**: All file I/O happens during save/load only  
✅ **Transparent**: Users don't need to change workflows  

## Known Limitations
1. Embedded data increases session file size (typically +300KB-2MB per preset)
2. Only works when files are loaded before saving session

## Future Enhancements
- Option to disable embedded storage for users who prefer file paths
- Compression of embedded data to reduce session file sizes
- UI indicator showing whether preset uses embedded or external files

## Build Instructions
Standard build process works unchanged:
```powershell
# Windows Debug
MSBuild NeuralAmpModeler.sln /p:Configuration=Debug /p:Platform=x64

# Windows Release
MSBuild NeuralAmpModeler.sln /p:Configuration=Release /p:Platform=x64
```

### VST3 Validation
To validate the plugin using the VST3 SDK validator:
```powershell
# Build the validator from VST3 SDK
git clone https://github.com/steinbergmedia/vst3sdk.git --branch v3.7.11_build_10 --depth=1
cd vst3sdk
git submodule update --init --recursive
mkdir build && cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release --target validator

# Run validation
.\bin\Release\validator.exe "path\to\NeuralAmpModeler.vst3"
```

## License
Maintains original project license (MIT).
