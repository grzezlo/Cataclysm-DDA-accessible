#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_GAMEPLAY_H
#define CATA_SRC_ACCESSIBILITY_GAMEPLAY_H

#include <optional>
#include <string>
#include <vector>

#include "accessibility_format.h"
#include "coords_fwd.h"

class Character;
class Creature;
class avatar;
class map;
class map_item_stack;
struct visibility_variables;
enum class content_newness;
enum class list_item_sort_mode : int;

// Gameplay-facing accessibility views and semantic descriptions.  game.cpp
// keeps only narrow calls into this module to limit upstream merge conflicts.
namespace accessibility_gameplay
{

/** Describe a target position relative to an origin, e.g. "3 tiles north". */
std::string situation_location( const tripoint_bub_ms &origin, const tripoint_bub_ms &target );
/** Semantic description of one look-around cursor position. */
std::vector<std::string> look_parts( map &here, const avatar &viewer,
                                     const tripoint_bub_ms &origin, const tripoint_bub_ms &position,
                                     const visibility_variables &cache );
std::string gameplay_focus_description( map &here, const avatar &viewer );
std::string arrival_announcement( map &here, const avatar &viewer, bool loaded );

/** Open the on-demand situation review list. */
void situation_review( map &here, const avatar &viewer );
/** Open the on-demand status review list; `safe_mode` is already formatted for speech. */
void status_review( const avatar &viewer, const std::string &safe_mode );

/**
 * Announce visible-threat and urgent condition transitions for this turn as one
 * utterance.  Nothing is formatted unless the accessible interface is active.
 */
void announce_turn_alerts( const map &here, const Character &viewer,
                           const std::vector<const Creature *> &threats );
/**
 * Speak queued gameplay speech before the game waits for a key.  After
 * auto-move, where per-step speech is skipped, also report the position.
 */
void before_player_input( map &here, const avatar &viewer );
/** Queue the destination tile after the avatar moves, except during auto-move. */
void announce_avatar_moved( const tripoint_abs_ms &old_abs_pos, const avatar &viewer,
                            const map &here );

accessibility::nearby_list_entry nearby_item_entry( const avatar &viewer,
        const map_item_stack &stack, int page, int result_index, int result_count, int priority,
        std::optional<content_newness> newness );
accessibility::nearby_list_entry nearby_creature_entry( const map &here, const avatar &viewer,
        const Creature &creature, int result_index, int result_count, bool player_knows );
std::string nearby_item_sort_change( list_item_sort_mode mode );

} // namespace accessibility_gameplay

#endif // CATA_SRC_ACCESSIBILITY_GAMEPLAY_H
