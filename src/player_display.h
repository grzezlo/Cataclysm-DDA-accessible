#pragma once
#ifndef CATA_SRC_PLAYER_DISPLAY_H
#define CATA_SRC_PLAYER_DISPLAY_H

#include <string>
#include <vector>

class Character;

// Test entry points for the character sheet's screen reader speech.  `tab`
// follows the sheet's tab order: stats, encumbrance, speed, skills, traits,
// bionics, effects, proficiencies.  `line` is the visual line, which counts
// skill category headings.  Speed causes from effects are not included.
std::string character_sheet_accessibility_tab( const Character &you, int tab );
/** Focus speech of each focusable row, as when moving down from the previous one. */
std::vector<std::string> character_sheet_accessibility_rows( const Character &you, int tab );
std::vector<std::string> character_sheet_accessibility_details( const Character &you, int tab,
        unsigned line );
std::string character_sheet_accessibility_summary( const Character &you, int tab, unsigned line );

#endif // CATA_SRC_PLAYER_DISPLAY_H
