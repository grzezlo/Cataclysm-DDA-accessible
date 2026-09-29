#pragma once
#ifndef CATA_SRC_MUTATION_UI_H
#define CATA_SRC_MUTATION_UI_H

#include <string>
#include <vector>

#include "type_id.h"

class avatar;

// Test entry points for the mutations screen's screen reader speech.
// `active_column` selects the active or passive column.
std::string mutations_accessibility_tab( avatar &you, bool active_column );
/** Focus speech of each row in the column. */
std::vector<std::string> mutations_accessibility_rows( avatar &you, bool active_column );
/** The Alt+D document: the row with its column position and letter, then the description. */
std::vector<std::string> mutations_accessibility_details( avatar &you, const trait_id &mut );
/** The Alt+S summary: mode, column, position, and letter. */
std::string mutations_accessibility_summary( avatar &you, bool active_column, int cursor,
        bool examining );

#endif // CATA_SRC_MUTATION_UI_H
