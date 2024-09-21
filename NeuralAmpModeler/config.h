#define PLUG_NAME "NeuralAmpModeler"
#define PLUG_MFR "Steven Atkinson"
#define PLUG_VERSION_HEX 0x0000070a
#define PLUG_VERSION_STR "0.7.10"
#define PLUG_UNIQUE_ID '1YEo'
#define PLUG_MFR_ID 'SDAa'
#define PLUG_URL_STR "https://github.com/sdatkinson/NeuralAmpModelerPlugin"
#define PLUG_EMAIL_STR "spam@me.com"
#define PLUG_COPYRIGHT_STR "Copyright 2022 Steven Atkinson"
#define PLUG_CLASS_NAME NeuralAmpModeler
#define BUNDLE_NAME "NeuralAmpModeler"
#define BUNDLE_MFR "StevenAtkinson"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "NeuralAmpModeler"

#ifdef APP_API
  #define PLUG_CHANNEL_IO "1-2"
#else
  #define PLUG_CHANNEL_IO "1-1 1-2 2-2"
#endif

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 600
#define PLUG_HEIGHT 400
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0
#define PLUG_MAX_WIDTH PLUG_WIDTH * 4
#define PLUG_MAX_HEIGHT PLUG_HEIGHT * 4

#define AUV2_ENTRY NeuralAmpModeler_Entry
#define AUV2_ENTRY_STR "NeuralAmpModeler_Entry"
#define AUV2_FACTORY NeuralAmpModeler_Factory
#define AUV2_VIEW_CLASS NeuralAmpModeler_View
#define AUV2_VIEW_CLASS_STR "NeuralAmpModeler_View"

#define AAX_TYPE_IDS 'ITP1'
#define AAX_TYPE_IDS_AUDIOSUITE 'ITA1'
#define AAX_PLUG_MFR_STR "Acme"
#define AAX_PLUG_NAME_STR "NeuralAmpModeler\nIPEF"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
#define MICHROMA_FN "Michroma-Regular.ttf"

#define GEAR_FN "Gear.svg"
#define FILE_FN "File.svg"
#define CLOSE_BUTTON_FN "Cross.svg"
#define LEFT_ARROW_FN "ArrowLeft.svg"
#define RIGHT_ARROW_FN "ArrowRight.svg"
#define MODEL_ICON_FN "ModelIcon.svg"
#define IR_ICON_ON_FN "IRIconOn.svg"
#define IR_ICON_OFF_FN "IRIconOff.svg"

#define BACKGROUND_FN "Background.jpg"
#define BACKGROUND2X_FN "Background@2x.jpg"
#define BACKGROUND3X_FN "Background@3x.jpg"
#define KNOBBACKGROUND_FN "KnobBackground.png"
#define KNOBBACKGROUND2X_FN "KnobBackground@2x.png"
#define KNOBBACKGROUND3X_FN "KnobBackground@3x.png"
#define FILEBACKGROUND_FN "FileBackground.png"
#define FILEBACKGROUND2X_FN "FileBackground@2x.png"
#define FILEBACKGROUND3X_FN "FileBackground@3x.png"
#define LINES_FN "Lines.png"
#define LINES2X_FN "Lines@2x.png"
#define LINES3X_FN "Lines@3x.png"
#define SLIDESWITCHHANDLE_FN "SlideSwitchHandle.png"
#define SLIDESWITCHHANDLE2X_FN "SlideSwitchHandle@2x.png"
#define SLIDESWITCHHANDLE3X_FN "SlideSwitchHandle@3x.png"

#define METERBACKGROUND_FN "MeterBackground.png"
#define METERBACKGROUND2X_FN "MeterBackground@2x.png"
#define METERBACKGROUND3X_FN "MeterBackground@3x.png"

// Issue 291
// On the macOS standalone, we might not have permissions to traverse the file directory, so we have the app ask the
// user to pick a directory instead of the file in the directory.
// Everyone else is fine though.
#if defined(APP_API) && defined(__APPLE__)
  #define NAM_PICK_DIRECTORY
#endif
