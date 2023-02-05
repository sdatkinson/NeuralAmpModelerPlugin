
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vTemplateProject
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vTemplateProject
#import <TemplateProjectAU/IPlugAUViewController.h>
#import <TemplateProjectAU/IPlugAUAudioUnit.h>

//! Project version number for TemplateProjectAU.
FOUNDATION_EXPORT double TemplateProjectAUVersionNumber;

//! Project version string for TemplateProjectAU.
FOUNDATION_EXPORT const unsigned char TemplateProjectAUVersionString[];

@class IPlugAUViewController_vTemplateProject;
