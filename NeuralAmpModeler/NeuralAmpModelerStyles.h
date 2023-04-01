#pragma once

#include "IGraphicsStructs.h"

using namespace iplug;
using namespace igraphics;

namespace PluginColors
{
// COLORS!
const IColor OFF_WHITE(255, 243, 246, 249); // Material UI because Heidi said so

// From group photo
// const IColor NAM_5(255, 206, 194, 224);  // Languid
// Lavendar const IColor NAM_4(255, 30, 30, 39);  // Raisin
// Black const IColor NAM_3(255, 48, 43, 96);  // Space Cadet
// const IColor NAM_2(255, 3, 2, 1);  // Black
// const IColor NAM_1(255, 18, 15, 18);  // Smoky Black

// Blue palette "Microsoft"
// const IColor NAM_1(255, 13, 27, 42);  // Black Fogra 29
// const IColor NAM_2(255, 27, 38, 59);  // Oxford Blue
// const IColor NAM_3(255, 65, 90, 119);  // Bedazzled Blue
// const IColor NAM_4(255, 119, 141, 169);  // Shadow Blue
// const IColor NAM_5(224, 225, 221, 224);  // Platinum

// Dark theme
// const IColor NAM_1(255, 26, 20, 35);  // Xiketic
// const IColor NAM_2(255, 55, 37, 73);  // Dark Purple
// const IColor NAM_3(255, 119, 76, 96);  // Twilight Lavendar
// const IColor NAM_4(255, 183, 93, 105);  // Popstar
// const IColor NAM_5(224, 234, 205, 194);  // Unbleached Silk

// const IColor MOUSEOVER = NAM_5.WithOpacity(0.3);

// My 3 colors (purple)
// const IColor NAM_1(255, 18, 17, 19);  // Smoky Black
// const IColor NAM_2(255, 115, 93, 120);  // Old Lavendar
// const IColor NAM_3(255, 189, 185, 196);  // Lavendar Gray
// Alts
// const IColor NAM_2(255, 34, 39, 37);  // Charleston Green
// const IColor NAM_2(255, 114, 161, 229);  // Little Boy Blue
// const IColor NAM_3(255, 247, 247, 242);  // Baby Powder
// const IColor NAM_3(255, 230, 220, 249);  // Pale Purple
// Pantone const IColor NAM_3(255, 218, 203, 246);  //
// Lavender Blue

// Blue mode
const IColor NAM_1(255, 29, 26, 31); // Raisin Black
const IColor NAM_2(255, 80, 133, 232); // Azure
const IColor NAM_3(255, 162, 178, 191); // Cadet Blue Crayola
// Alts
// const IColor NAM_1(255, 18, 17, 19);  // Smoky Black
// const IColor NAM_2(255, 126, 188, 230);  // Camel
// const IColor NAM_2(255, 152, 202, 235);  // Pale Cerulean
// const IColor NAM_2(255, 46, 116, 163);  // French Blue
// const IColor NAM_2(255, 80, 171, 232);  // Blue Jeans
// const IColor NAM_3(255, 189, 185, 196);  // Aero
// const IColor NAM_3(255, 221, 237, 248);  // Alice Blue
// const IColor NAM_3(255, 207, 220, 229);  // Beau Blue
// const IColor NAM_3(255, 187, 199, 208);  // Silver Sand

// Misc
const IColor MOUSEOVER = NAM_3.WithOpacity(0.3);
const IColor HELP_TEXT = COLOR_WHITE;
}; // namespace PluginColors


const auto activeColorSpec = IVColorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_1, // Foreground
  PluginColors::NAM_2.WithOpacity(0.4f), // Pressed
  PluginColors::NAM_3, // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_2, // Extra 1
  COLOR_RED, // Extra 2
  DEFAULT_X3COLOR // Extra 3
};

const auto style = IVStyle{true, // Show label
                           true, // Show value
                           activeColorSpec,
                           {DEFAULT_TEXT_SIZE + 5.f, EVAlign::Middle, PluginColors::NAM_3}, // Knob label text
                           {DEFAULT_TEXT_SIZE + 5.f, EVAlign::Bottom, PluginColors::NAM_3}, // Knob value text
                           DEFAULT_HIDE_CURSOR,
                           DEFAULT_DRAW_FRAME,
                           false,
                           DEFAULT_EMBOSS,
                           0.1f,
                           2.f,
                           DEFAULT_SHADOW_OFFSET,
                           DEFAULT_WIDGET_FRAC,
                           DEFAULT_WIDGET_ANGLE};
