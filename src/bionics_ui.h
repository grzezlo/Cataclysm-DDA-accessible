#pragma once
#ifndef CATA_SRC_BIONICS_UI_H
#define CATA_SRC_BIONICS_UI_H

#include <string>
#include <vector>

class avatar;

// Test entry points for the bionics screen's screen reader speech.  `active`
// selects the active or passive tab; `cursor` is the focused row in that tab.
std::string bionics_accessibility_tab( avatar &you, bool active );
/** Focus speech of each row in the tab's sort order. */
std::vector<std::string> bionics_accessibility_rows( avatar &you, bool active );
/** The Alt+D document: the row with its position and letter, then the description. */
std::vector<std::string> bionics_accessibility_details( avatar &you, bool active, int cursor );
/** The Alt+S summary: power, fuel, mode, tab, position, and letter. */
std::string bionics_accessibility_summary( avatar &you, bool active, int cursor, bool examining );

#endif // CATA_SRC_BIONICS_UI_H
