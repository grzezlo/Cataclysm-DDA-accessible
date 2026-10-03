#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_GAMEPLAY_H
#define CATA_SRC_ACCESSIBILITY_GAMEPLAY_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "coords_fwd.h"
#include "point.h"
#include "type_id.h"

class Character;
class Creature;
class avatar;
class input_context;
class inventory;
class item;
class map;
class map_item_stack;
class mod_ui;
class player_activity;
class profession;
class recipe;
class scenario;
struct requirement_data;
class uilist;
class vehicle;
struct vehicle_part;
class zone_data;
struct item_info_data;
struct visibility_variables;
namespace accessibility
{
class game_message_speech_capture;
enum class item_action_availability : int;
struct character_choice_row;
struct character_trait_row;
struct direction_target;
struct known_place;
struct nearby_list_entry;
struct overmap_tile;
struct requirement_group;
struct target_focus;
struct vehicle_grid_position;
struct vehicle_part_row;
struct vehicle_status;
struct world_mod_row;
struct zone_details;
struct zone_row;
} // namespace accessibility
enum class content_newness;
enum class hint_rating;
enum class list_item_sort_mode : int;

// Gameplay-facing accessibility views and semantic descriptions.  game.cpp
// keeps only narrow calls into this module to limit upstream merge conflicts.
namespace accessibility_gameplay
{

void activity_started( const player_activity &act, const avatar &viewer, bool resuming );
void activity_canceled( const avatar &viewer );
/** Actors may use cancel_activity for cleanup after successful completion. */
void activity_completed( const Character &viewer );
void activity_interruption_reason( const std::string &reason );
void sleep_started( const avatar &viewer );
void sleep_finished( const avatar &viewer );
void update_activity_progress( const avatar &viewer );
/** Whether the current activity or sleep has its own progress speech. */
bool has_activity_progress();
void reset_activity_speech();

/** Capture one handler turn; report its result only when the activity ends. */
class activity_turn_speech
{
    public:
        explicit activity_turn_speech( Character &who );
        ~activity_turn_speech();
        void finish_cycle() {
            finished = true;
        }
        /** Call before clearing the old activity and resuming its backlog. */
        void conclude();
    private:
        avatar *viewer = nullptr;
        std::unique_ptr<accessibility::game_message_speech_capture> capture;
        unsigned generation = 0;
        bool finished = false;
        bool concluded = false;
        activity_turn_speech *previous = nullptr;
};

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
/** An overmap tile as the viewer knows it, with its offset from the viewer. */
accessibility::overmap_tile overmap_tile( const avatar &viewer, const tripoint_abs_omt &position );
/**
 * Detail sentences for the overmap cursor: description with the nearest known
 * city, vision level, note, danger, missions, current objective distance,
 * `weather` when visible, explored state, level, and offset.
 */
std::vector<std::string> overmap_tile_details( const avatar &viewer,
        const tripoint_abs_omt &position, const std::string &weather );
/** "Route: <destination>, <location>." or "No route selected." */
std::string overmap_route_state( const avatar &viewer );

/**
 * Places the viewer knows about, nearest first: seen cities and camps, map
 * notes, active mission targets, and the route destination.  Places at one
 * position are merged into one row.
 */
std::vector<std::pair<tripoint_abs_omt, accessibility::known_place>> known_overmap_places(
            const avatar &viewer );
/** Open the known-places list; return the chosen place, or nothing on cancel. */
std::optional<tripoint_abs_omt> choose_known_overmap_place( const avatar &viewer );
/**
 * Speak the route after CHOOSE_DESTINATION previewed a new route to
 * `destination`, or why there is none.
 */
void announce_overmap_route( const avatar &viewer, const tripoint_abs_omt &destination,
                             const std::string &confirm_key );
/** Row speech for a map note in the notes list. */
std::string overmap_note_row( const avatar &viewer, const tripoint_abs_omt &position );

/** Speech state of one overmap session. */
class overmap_speech
{
    public:
        /** Speak the prompt or "Overmap" and the cursor tile. */
        void open( const avatar &viewer, const std::string &prompt, const tripoint_abs_omt &cursor );
        /**
         * Speak the cursor when it moved.  One step onto the same terrain speaks
         * only the terrain and markers; a level change speaks the level first.
         */
        void update( const avatar &viewer, const tripoint_abs_omt &cursor );
        /** Speak the cursor tile in full, for example after its note changed. */
        void announce( const avatar &viewer, const tripoint_abs_omt &cursor );
        /** Remember the cursor without speaking; it was already spoken elsewhere. */
        void sync( const avatar &viewer, const tripoint_abs_omt &cursor );
        /** Speak the prompt, cursor tile, level, and route. */
        void summary( const avatar &viewer, const tripoint_abs_omt &cursor ) const;
    private:
        std::string prompt;
        tripoint spoken = tripoint::invalid;
        std::string terrain;
};

/** Row speech model for one zone in the zones manager. */
accessibility::zone_row zone_row( const avatar &viewer, const zone_data &zone );
/** Details of one zone at one-based `position` of `count`. */
accessibility::zone_details zone_details( const avatar &viewer, const zone_data &zone,
        size_t position, size_t count );

/**
 * Speech for one zones manager session.  It keeps references to the manager's
 * list, focus, and filter so game.cpp needs only a call before and after each
 * action; it compares the two to speak the result of the action.
 */
class zones_manager_speech
{
    public:
        zones_manager_speech( const avatar &viewer,
                              const std::vector<std::reference_wrapper<zone_data>> &zones, const int &index,
                              const bool &show_all, const faction_id &faction );
        /** Speak the screen name, zone count, and focused zone. */
        void open();
        /** Remember the state before the action is handled. */
        void before();
        /** Speak the result of `action`; `closing` is set when the manager closes. */
        void after( const std::string &action, bool closing ) const;
        /** Screen summary for ACCESSIBILITY_STATUS. */
        std::string summary() const;
        /** Focused zone details for ACCESSIBILITY_DETAILS. */
        std::string details() const;
        /**
         * Speech for the result of `action` since before(), or empty.  Actions
         * that return focus from a dialog speak the focused zone.
         */
        std::string result( const std::string &action ) const;
    private:
        std::string focus_row() const;
        const zone_data *focused() const;
        size_t enabled_personal_zones() const;

        const avatar &viewer;
        const std::vector<std::reference_wrapper<zone_data>> &zones;
        const int &index;
        const bool &show_all;
        const faction_id &faction;

        // State before the current action.
        int previous_index = 0;
        size_t previous_count = 0;
        size_t previous_personal_enabled = 0;
        bool had_focus = false;
        std::string previous_name;
        tripoint previous_start;
        tripoint previous_end;
        std::vector<std::pair<std::string, std::string>> previous_options;
};

/** Driving state of `veh` for the driving summary. */
accessibility::vehicle_status vehicle_status( map &here, const vehicle &veh );
/**
 * When the viewer controls a vehicle, speak its driving summary and return
 * true; otherwise return false so the caller opens the status review.
 */
bool announce_driving_status( map &here, const avatar &viewer );
/**
 * The nearest newly visible obstacle on the path of the vehicle the viewer
 * controls, as speech.  Each obstacle is spoken once until it leaves the path.
 */
std::optional<std::string> vehicle_obstacle_transition( map &here, const avatar &viewer );

/** Speaks the cruise speed and steering heading changed by one driving input. */
class vehicle_control_speech
{
    public:
        vehicle_control_speech( map &here, const vehicle &veh );
        /** Queue the changes; `acceleration` is the requested change, negative to speed up. */
        void finish( int acceleration ) const;
    private:
        map &here;
        const vehicle &veh;
        int cruise = 0;
        double turn_degrees = 0.0;
};

/** The examination grid position of `mount` within the bounds of `veh`. */
accessibility::vehicle_grid_position vehicle_grid_position( const vehicle &veh,
        const point_rel_ms &mount );
/** Part list row of `part`, a part of `veh`, with its location. */
accessibility::vehicle_part_row vehicle_part_row( const vehicle &veh, const vehicle_part &part );
/**
 * Part list row with the bounds of `grid`, a vehicle_grid_position() of any tile
 * of `veh`; lists compute it once instead of once per part.
 */
accessibility::vehicle_part_row vehicle_part_row( const vehicle &veh, const vehicle_part &part,
        const accessibility::vehicle_grid_position &grid );
/**
 * Indices of the parts of `veh` in part list order: front to rear, left to
 * right, and as stacked on each tile.
 */
std::vector<int> vehicle_part_list( const vehicle &veh );
/** Grid cursor speech for the tile at `mount`: location, parts, and label. */
std::string vehicle_tile_description( const vehicle &veh, const point_rel_ms &mount,
                                      bool obstructed );
/** Grid tile details: exact position, then each part with its contents. */
std::vector<std::string> vehicle_tile_details( const vehicle &veh, const point_rel_ms &mount );
/** Part details: row, exact position, contents, faults, label, and description. */
std::vector<std::string> vehicle_part_details( const vehicle &veh, const vehicle_part &part );

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
/**
 * The number of requirement groups of `req`, with components, that
 * requirement_groups() reports as unmet, without building their text.
 */
size_t missing_requirement_groups( const requirement_data &req, const inventory &inv,
                                   const std::function<bool( const item & )> &component_filter, int batch );
/** Disassembly detail lines: returned components, time, and required tools. */
std::vector<std::string> disassembly_details( const recipe &uncraft, const std::string &yield,
        const std::string &time, const inventory &inv );

accessibility::item_action_availability item_action_availability( hint_rating rating );
/**
 * Name the item when its action menu opens and let the details action read the
 * complete item information.  `info` must outlive the menu query.
 */
void describe_item_action_menu( uilist &menu, std::string_view item_name,
                                const item_info_data &info );

/**
 * Character creation rows as the list shows them by color: the current
 * choice, unavailability, and unaffordable costs.  `points_left` is the point
 * balance the header uses for affordability; `show_points` is false for a
 * pool without limits.
 */
accessibility::character_choice_row character_scenario_row( const scenario &scen,
        const avatar &you, bool cities_enabled, bool show_points, int points_left );
accessibility::character_choice_row character_profession_row( const profession &prof,
        const avatar &you, bool show_points, int points_left );
/** A background is unavailable when its traits conflict with a chosen background's. */
accessibility::character_choice_row character_background_row( const profession &hobby,
        const avatar &you, bool show_points, int points_left );
/**
 * A trait row with the locks, conflicts, and prohibitions that the trait list
 * shows by color or reports only after Enter.
 */
accessibility::character_trait_row character_trait_row( const trait_id &trait, const avatar &you,
        bool show_points );

/**
 * A mod row as the mod manager shows it: category header, name color, and the
 * incompatibility mark.  `in_mod_list` adds whether the mod is active; the
 * load order itself holds only active mods.
 */
accessibility::world_mod_row world_mod_row( mod_ui &mods, const mod_id &mod,
        const std::vector<mod_id> &active, bool in_mod_list );
/** Why a mod cannot be activated (missing dependencies); empty when it can. */
std::string mod_unavailable_reason( mod_ui &mods, const mod_id &mod );
/** Why Enter cannot add a mod the incompatibility popup does not report; empty when it can. */
std::string mod_add_refusal( mod_ui &mods, const mod_id &mod, const std::vector<mod_id> &active );
/** Why a load order entry cannot move one step; empty when it can. */
std::string mod_move_refusal( mod_ui &mods, size_t selection, const std::vector<mod_id> &active,
                              bool up );
/** Spoken mod name: the display name, or the id of a mod that is not installed. */
std::string mod_display_name( const mod_id &mod );

/** -1 for list moves toward the start, 1 toward the end, 0 for other actions. */
int list_move_direction( const std::string &action );
/**
 * Speech for a read-only match list such as a rule test result.  Call once
 * with an empty action after creating `ctxt`: it registers the shared keys and
 * speaks the title with the first match.  Then call after every action with
 * the line before it; moves speak the match, Alt+S the position, and Alt+D
 * the note under the list.
 */
void match_list_speech( input_context &ctxt, const std::string &title, const std::string &note,
                        const std::vector<std::string> &matches, int line, const std::string &action,
                        int previous_line );

} // namespace accessibility_gameplay

#endif // CATA_SRC_ACCESSIBILITY_GAMEPLAY_H
