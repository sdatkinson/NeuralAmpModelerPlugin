//
//  Colors.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/27/22.
//
// Store the defined colors for the plugin in one place

#ifndef Colors_h
#define Colors_h

#include "IGraphicsStructs.h"

namespace PluginColors
{
// HINT: ARGB
// COLORS!
const iplug::igraphics::IColor OFF_WHITE(255, 243, 246, 249); // Material UI because Heidi said so

// From group photo
// const iplug::igraphics::IColor NAM_5(255, 206, 194, 224);  // Languid
// Lavendar const iplug::igraphics::IColor NAM_4(255, 30, 30, 39);  // Raisin
// Black const iplug::igraphics::IColor NAM_3(255, 48, 43, 96);  // Space Cadet
// const iplug::igraphics::IColor NAM_2(255, 3, 2, 1);  // Black
// const iplug::igraphics::IColor NAM_1(255, 18, 15, 18);  // Smoky Black

// Blue palette "Microsoft"
// const iplug::igraphics::IColor NAM_1(255, 13, 27, 42);  // Black Fogra 29
// const iplug::igraphics::IColor NAM_2(255, 27, 38, 59);  // Oxford Blue
// const iplug::igraphics::IColor NAM_3(255, 65, 90, 119);  // Bedazzled Blue
// const iplug::igraphics::IColor NAM_4(255, 119, 141, 169);  // Shadow Blue
// const iplug::igraphics::IColor NAM_5(224, 225, 221, 224);  // Platinum

// Dark theme
// const iplug::igraphics::IColor NAM_1(255, 26, 20, 35);  // Xiketic
// const iplug::igraphics::IColor NAM_2(255, 55, 37, 73);  // Dark Purple
// const iplug::igraphics::IColor NAM_3(255, 119, 76, 96);  // Twilight Lavendar
// const iplug::igraphics::IColor NAM_4(255, 183, 93, 105);  // Popstar
// const iplug::igraphics::IColor NAM_5(224, 234, 205, 194);  // Unbleached Silk

// const iplug::igraphics::IColor MOUSEOVER = NAM_5.WithOpacity(0.3);

// My 3 colors (purple)
// const iplug::igraphics::IColor NAM_1(255, 18, 17, 19);  // Smoky Black
// const iplug::igraphics::IColor NAM_2(255, 115, 93, 120);  // Old Lavendar
// const iplug::igraphics::IColor NAM_3(255, 189, 185, 196);  // Lavendar Gray
// Alts
// const iplug::igraphics::IColor NAM_2(255, 34, 39, 37);  // Charleston Green
// const iplug::igraphics::IColor NAM_2(255, 114, 161, 229);  // Little Boy Blue
// const iplug::igraphics::IColor NAM_3(255, 247, 247, 242);  // Baby Powder
// const iplug::igraphics::IColor NAM_3(255, 230, 220, 249);  // Pale Purple
// Pantone const iplug::igraphics::IColor NAM_3(255, 218, 203, 246);  //
// Lavender Blue

// Blue mode
const iplug::igraphics::IColor NAM_1(255, 29, 26, 31); // Raisin Black
const iplug::igraphics::IColor NAM_2(255, 80, 133, 232); // Azure
const iplug::igraphics::IColor NAM_3(255, 162, 178, 191); // Cadet Blue Crayola
// Alts
// const iplug::igraphics::IColor NAM_1(255, 18, 17, 19);  // Smoky Black
// const iplug::igraphics::IColor NAM_2(255, 126, 188, 230);  // Camel
// const iplug::igraphics::IColor NAM_2(255, 152, 202, 235);  // Pale Cerulean
// const iplug::igraphics::IColor NAM_2(255, 46, 116, 163);  // French Blue
// const iplug::igraphics::IColor NAM_2(255, 80, 171, 232);  // Blue Jeans
// const iplug::igraphics::IColor NAM_3(255, 189, 185, 196);  // Aero
// const iplug::igraphics::IColor NAM_3(255, 221, 237, 248);  // Alice Blue
// const iplug::igraphics::IColor NAM_3(255, 207, 220, 229);  // Beau Blue
// const iplug::igraphics::IColor NAM_3(255, 187, 199, 208);  // Silver Sand

// Evan Heritage theme colors
const iplug::igraphics::IColor NAM_0(0, 18, 17, 19); // Transparent
const iplug::igraphics::IColor NAM_THEMECOLOR(255, 80, 133, 232); // Azure
// const iplug::igraphics::IColor NAM_THEMECOLOR(255, 23, 190, 187); // Custom :)
const iplug::igraphics::IColor NAM_THEMEFONTCOLOR(255, 242, 242, 242); // Dark White

// Misc
// const iplug::igraphics::IColor MOUSEOVER = NAM_3.WithOpacity(0.3);
const iplug::igraphics::IColor MOUSEOVER = NAM_THEMEFONTCOLOR.WithOpacity(0.1);
const iplug::igraphics::IColor HELP_TEXT = iplug::igraphics::COLOR_WHITE;
const iplug::igraphics::IColor HELP_TEXT_MO = iplug::igraphics::COLOR_WHITE.WithOpacity(0.9);
const iplug::igraphics::IColor HELP_TEXT_CLICKED = iplug::igraphics::COLOR_WHITE.WithOpacity(0.8);

}; // namespace PluginColors

#endif /* Colors_h */
