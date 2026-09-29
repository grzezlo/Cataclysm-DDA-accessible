#pragma once
#ifndef CATA_SRC_MEDICAL_UI_H
#define CATA_SRC_MEDICAL_UI_H

#include <string>
#include <vector>

class Character;

// Test entry points for the medical menu's screen reader speech.  `column`
// is 0 for health, 1 for effects, and 2 for stats.
/** Opening speech: the menu name, the title bar statuses, and the health column. */
std::string medical_accessibility_open( Character &you );
/** Column speech: its name, row count, and first row. */
std::string medical_accessibility_column( Character &you, int column );
/** Focus speech of each row in the column. */
std::vector<std::string> medical_accessibility_rows( Character &you, int column );
/** The Alt+D document of a row: the row, its position, description, and detail text. */
std::vector<std::string> medical_accessibility_details( Character &you, int column, int row );

#endif // CATA_SRC_MEDICAL_UI_H
