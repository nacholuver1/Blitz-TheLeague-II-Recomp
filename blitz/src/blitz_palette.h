// blitz - ReXGlue Recompiled Project
//
// Blitz: The League II's menu look (see blitz_look.h): the floodlit fence from
// its own menus, chrome BLITZ over a blood-red THE LEAGUE II. Read only by
// blitz_look.cpp.

#pragma once

#include "blitz_look.h"

namespace blitz::look {

inline const Palette kGamePalette{Scene::kFloodlights,
                                  IM_COL32(204, 26, 22, 255),    // accent: the logo's blood red
                                  IM_COL32(245, 245, 240, 255),  // text
                                  IM_COL32(176, 182, 192, 255),  // dim
                                  IM_COL32(130, 138, 150, 255),  // hint
                                  "BLITZ",
                                  "THE LEAGUE II"};

}  // namespace blitz::look
