#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_GAMEPLAY_H
#define CATA_SRC_ACCESSIBILITY_GAMEPLAY_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "accessibility_format.h"
#include "coords_fwd.h"

class Character;
class Creature;
class avatar;
class inventory;
class item;
class map;
class map_item_stack;
class recipe;
struct requirement_data;
class uilist;
class vehicle;
struct item_info_data;
struct visibility_variables;
enum class content_newness;
enum class hint_rating;
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
/**
 * Name the most salient thing the viewer can see at a position next to
 * `origin`, with its direction from `origin`.
 */
accessibility::direction_target direction_target( map &here, const avatar &viewer,
        const tripoint_bub_ms &origin, const tripoint_bub_ms &position );
/** Describe `valid` in compass order starting north, with `origin` itself last. */
std::vector<accessibility::direction_target> direction_targets( map &here, const avatar &viewer,
        const tripoint_bub_ms &origin, const std::vector<tripoint_bub_ms> &valid );
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

/** Summarize a local auto-travel route that the player just started. */
void announce_travel_start( map &here, const avatar &viewer,
                            const std::vector<tripoint_bub_ms> &route );
/** Summarize an overmap travel path, stored destination first as in `omt_path`. */
void announce_overmap_travel_start( const avatar &viewer,
                                    const std::vector<tripoint_abs_omt> &path );
/** The player confirmed cancelling auto-travel; the stop needs no travel speech. */
void travel_canceled();
/** Overmap travel reached its destination; the game message reports it. */
void travel_arrived();

accessibility::nearby_list_entry nearby_item_entry( const avatar &viewer,
        const map_item_stack &stack, int page, int result_index, int result_count, int priority,
        std::optional<content_newness> newness );
accessibility::nearby_list_entry nearby_creature_entry( const map &here, const avatar &viewer,
        const Creature &creature, int result_index, int result_count, bool player_knows );
std::string nearby_item_sort_change( list_item_sort_mode mode );

/**
 * Describe what the targeting cursor is on: the visible `critter` or `veh`
 * with its attitude and location, the look-around tile, and, when
 * `line_of_fire` is set, the visible obstacle and creatures along
 * `trajectory`.  The caller adds the hit chance and status problems.
 */
accessibility::target_focus target_focus( map &here, const avatar &viewer,
        const tripoint_bub_ms &origin, const tripoint_bub_ms &cursor,
        const std::vector<tripoint_bub_ms> &trajectory, const Creature *critter, const vehicle *veh,
        bool line_of_fire );
/** Target description paragraphs for the targeting details view. */
std::vector<std::string> target_details( map &here, const avatar &viewer,
        const Creature *critter, const vehicle *veh );

/**
 * Requirement groups of `req` with the state of each alternative, as the
 * crafting requirement list shows them by color: tool qualities and tools,
 * then components when `include_components` is set.
 */
std::vector<accessibility::requirement_group> requirement_groups( const requirement_data &req,
        const inventory &inv, const std::function<bool( const item & )> &component_filter,
        int batch, bool include_components );
/** Disassembly detail lines: returned components, time, and required tools. */
std::vector<std::string> disassembly_details( const recipe &uncraft, const std::string &yield,
        const std::string &time, const inventory &inv );

accessibility::item_action_availability item_action_availability( hint_rating rating );
/**
 * Name the item when its action menu opens and let the details action read the
 * complete item information.  `info` must outlive the menu query.
 */
void describe_item_action_menu( uilist &menu, const std::string &item_name,
                                const item_info_data &info );

} // namespace accessibility_gameplay

#endif // CATA_SRC_ACCESSIBILITY_GAMEPLAY_H
