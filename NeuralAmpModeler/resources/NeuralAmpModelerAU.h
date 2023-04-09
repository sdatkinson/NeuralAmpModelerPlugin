
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1
  #import <UIKit/UIKit.h>
#else
  #import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vNeuralAmpModeler
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vNeuralAmpModeler
#import <NeuralAmpModelerAU/IPlugAUAudioUnit.h>
#import <NeuralAmpModelerAU/IPlugAUViewController.h>

//! Project version number for NeuralAmpModelerAU.
FOUNDATION_EXPORT double NeuralAmpModelerAUVersionNumber;

//! Project version string for NeuralAmpModelerAU.
FOUNDATION_EXPORT const unsigned char NeuralAmpModelerAUVersionString[];

@class IPlugAUViewController_vNeuralAmpModeler;
