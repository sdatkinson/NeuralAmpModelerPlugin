PROJECT_ROOT = $(PWD)/..
DEPS_PATH = $(IPLUG2_ROOT)/Dependencies
WAM_SDK_PATH = $(DEPS_PATH)/IPlug/WAM_SDK/wamsdk
WDL_PATH = $(IPLUG2_ROOT)/WDL
IPLUG_PATH = $(IPLUG2_ROOT)/IPlug
SWELL_PATH = $(WDL_PATH)/swell
IGRAPHICS_PATH = $(IPLUG2_ROOT)/IGraphics
CONTROLS_PATH = $(IGRAPHICS_PATH)/Controls
PLATFORMS_PATH = $(IGRAPHICS_PATH)/Platforms
DRAWING_PATH = $(IGRAPHICS_PATH)/Drawing
IGRAPHICS_EXTRAS_PATH = $(IGRAPHICS_PATH)/Extras
IPLUG_EXTRAS_PATH = $(IPLUG_PATH)/Extras
IPLUG_SYNTH_PATH = $(IPLUG_EXTRAS_PATH)/Synth
IPLUG_FAUST_PATH = $(IPLUG_EXTRAS_PATH)/Faust
IPLUG_WEB_PATH = $(IPLUG_PATH)/WEB
NANOVG_PATH = $(DEPS_PATH)/IGraphics/NanoVG/src
NANOSVG_PATH = $(DEPS_PATH)/IGraphics/NanoSVG/src
IMGUI_PATH = $(DEPS_PATH)/IGraphics/imgui
YOGA_PATH = $(DEPS_PATH)/IGraphics/yoga
STB_PATH = $(DEPS_PATH)/IGraphics/STB

IPLUG_SRC = $(IPLUG_PATH)/IPlugAPIBase.cpp \
	$(IPLUG_PATH)/IPlugParameter.cpp \
	$(IPLUG_PATH)/IPlugPluginBase.cpp \
	$(IPLUG_PATH)/IPlugPaths.cpp \
	$(IPLUG_PATH)/IPlugTimer.cpp

IGRAPHICS_SRC = $(IGRAPHICS_PATH)/IGraphics.cpp \
	$(IGRAPHICS_PATH)/IControl.cpp \
	$(CONTROLS_PATH)/*.cpp \
	$(PLATFORMS_PATH)/IGraphicsWeb.cpp

IMGUI_SRC = $(IGRAPHICS_PATH)/IGraphicsImGui.cpp

INCLUDE_PATHS = -I$(PROJECT_ROOT) \
-I$(WAM_SDK_PATH) \
-I$(WDL_PATH) \
-I$(SWELL_PATH) \
-I$(IPLUG_PATH) \
-I$(IPLUG_EXTRAS_PATH) \
-I$(IPLUG_SYNTH_PATH) \
-I$(IPLUG_FAUST_PATH) \
-I$(IPLUG_WEB_PATH) \
-I$(IGRAPHICS_PATH) \
-I$(DRAWING_PATH) \
-I$(CONTROLS_PATH) \
-I$(PLATFORMS_PATH) \
-I$(IGRAPHICS_EXTRAS_PATH) \
-I$(NANOVG_PATH) \
-I$(NANOSVG_PATH) \
-I$(STB_PATH) \
-I$(IMGUI_PATH) \
-I$(IMGUI_PATH)/backends \
-I$(YOGA_PATH) \
-I$(YOGA_PATH)/yoga

#every cpp file that is needed for both WASM modules
SRC = $(IPLUG_SRC)

#every cpp file that is needed for the WAM audio processor WASM module running in the audio worklet
WAM_SRC = $(IPLUG_WEB_PATH)/IPlugWAM.cpp \
	$(WAM_SDK_PATH)/processor.cpp \
	$(IPLUG_PATH)/IPlugProcessor.cpp

#every cpp file that is needed for the "WEB" graphics WASM module
WEB_SRC = $(IGRAPHICS_SRC) \
$(IPLUG_WEB_PATH)/IPlugWeb.cpp \
$(IGRAPHICS_PATH)/IGraphicsEditorDelegate.cpp

NANOVG_LDFLAGS = -s USE_WEBGL2=0 -s FULL_ES3=1

IMGUI_LDFLAGS = -s BINARYEN_TRAP_MODE=clamp 

# CFLAGS for both WAM and WEB targets
CFLAGS = $(INCLUDE_PATHS) \
-std=c++17  \
-Wno-bitwise-op-parentheses \
-DWDL_NO_DEFINE_MINMAX \
-DNDEBUG=1

WAM_CFLAGS = -DWAM_API \
-DIPLUG_DSP=1 \
-DNO_IGRAPHICS \
-DSAMPLE_TYPE_FLOAT

WEB_CFLAGS = -DWEB_API \
-DIPLUG_EDITOR=1

WAM_EXPORTS = "[\
  '_createModule','_wam_init','_wam_terminate','_wam_resize', \
  '_wam_onprocess', '_wam_onmidi', '_wam_onsysex', '_wam_onparam', \
  '_wam_onmessageN', '_wam_onmessageS', '_wam_onmessageA', '_wam_onpatch' \
  ]"

WEB_EXPORTS = "['_main', '_iplug_fsready', '_iplug_syncfs']"

# LDFLAGS for both WAM and WEB targets
LDFLAGS = -s ALLOW_MEMORY_GROWTH=1 --bind

# We can't compile the WASM module synchronously on main thread (.wasm over 4k in size requires async compile on chrome) https://developers.google.com/web/updates/2018/04/loading-wasm
# and you can't compile asynchronously in AudioWorklet scope
# The following settings mean the WASM is delivered as BASE64 and included in the MyPluginName-wam.js file.
WAM_LDFLAGS = -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap', 'setValue', 'UTF8ToString']" \
-s BINARYEN_ASYNC_COMPILATION=0 \
-s SINGLE_FILE=1
#-s ENVIRONMENT=worker

WEB_LDFLAGS = -s EXPORTED_FUNCTIONS=$(WEB_EXPORTS) \
-s EXPORTED_RUNTIME_METHODS="['UTF8ToString']" \
-s BINARYEN_ASYNC_COMPILATION=1 \
-s FORCE_FILESYSTEM=1 \
-s ENVIRONMENT=web \
-lidbfs.js

