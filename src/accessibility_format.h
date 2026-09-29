#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_FORMAT_H
#define CATA_SRC_ACCESSIBILITY_FORMAT_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Renderer-independent semantic models and speech formatters.  Everything here
// is a pure function of its inputs so it can be tested without a backend.
namespace accessibility
{

struct semantic_list_entry {
    int distance;
    int z;
    int y;
    int x;
    int kind;
    std::string label;
    std::string context;
};

struct connected_semantic_list_group {
    semantic_list_entry nearest;
    size_t count;
};

/** Renderer-independent semantic row for the nearby item and creature lists. */
struct nearby_list_entry {
    std::string name;
    int count = 0;
    int position_index = 0;
    int position_count = 0;
    std::string location;
    std::vector<std::string> details;
    int result_index = 0;
    int result_count = 0;
};

/** Source messages in visual order; wrapped screen lines are never separate entries. */
struct message_log_entry {
    size_t source_index = 0;
    std::string text;
    std::string type;
    std::string spoken_type;
    std::string elapsed;
};

class message_log_review
{
    public:
        void set_entries( std::vector<message_log_entry> entries, bool newest_first );
        /** Invalid nonempty type prefixes leave the current results unchanged. */
        bool set_filter( const std::string &query, const std::vector<std::string> &types );
        /** Source indices of the matching messages, in visual order. */
        const std::vector<size_t> &matches() const;
        const message_log_entry *focused() const;
        size_t focused_position() const;
        bool focus_source( size_t source_index );
        void focus_newest();
        bool move( int delta );
        bool page( int direction, size_t page_size );

    private:
        std::vector<message_log_entry> entries_;
        /** Indices into entries_ for the matching messages. */
        std::vector<size_t> matched_entries_;
        std::vector<size_t> matches_;
        size_t focused_ = 0;
        bool newest_first_ = true;
};

std::string format_message_log_focus( const message_log_entry &entry );
std::string format_message_log_detail( const message_log_entry &entry, size_t position,
                                       size_t count );
std::string format_message_log_open( size_t count, const message_log_entry *entry );
std::string format_message_log_filter( const std::string &query, size_t count,
                                       const message_log_entry *entry );
std::string format_message_log_filter_field( const std::string &query,
        const std::vector<std::string> &types );
std::string format_message_log_filter_value( const std::string &query );
std::string format_message_log_invalid_type( const std::string &type );

void sort_semantic_list_entries( std::vector<semantic_list_entry> &entries );
std::vector<connected_semantic_list_group> group_connected_semantic_list_entries(
    std::vector<semantic_list_entry> entries );

/** Join parts into sentences, skipping empty parts and adding missing sentence ends. */
std::string format_semantic_sequence( const std::vector<std::string> &parts );

/**
 * "Top" when a move toward the end wrapped to the start, "Bottom" when a move
 * toward the start wrapped to the end, spoken before the focused row.  Empty
 * without a wrap, for a zero `direction`, or when either index is negative.
 */
std::string format_list_wrap( int previous, int current, int direction );
/** Item count spoken once when a list opens or a pane is switched to. */
std::string format_list_count( size_t count );
/** One-based focus position, spoken only on request. */
std::string format_list_position( size_t position, size_t count );
/** Describe a main-menu world using its save count rather than visual parentheses. */
std::string format_main_menu_world( const std::string &name, size_t save_count );
std::string format_main_menu_focus( const std::string &menu_name, const std::string &tab,
                                    const std::string &focused, const std::string &title = {},
                                    const std::string &version = {}, const std::string &tip = {} );
/** Tab and submenu positions on request; a zero `item_position` omits the submenu. */
std::string format_main_menu_position( const std::string &tab, int tab_position, int tab_count,
                                       int item_position, int item_count );
struct semantic_document_line {
    std::string text;
    size_t source_index;
};
/** Keep meaningful source lines in reading order without splitting long lines. */
std::vector<semantic_document_line> select_document_lines(
    const std::vector<std::string> &source_lines );
int move_document_cursor( int current, int line_count, int delta );
/** Retain independent reading positions when switching between documents. */
class document_cursor_memory
{
    public:
        explicit document_cursor_memory( size_t document_count );
        int position( size_t document ) const;
        int move( size_t document, int line_count, int delta );
        void reset();

    private:
        std::vector<int> positions_;
};
/** Format a document title and the first line or paragraph for opening announcement. */
std::string format_document_focus( const std::string &title, const std::string &text );
/** Format opening announcement for the help topic menu. */
std::string format_help_menu_open( size_t topic_count, const std::string &initial_hotkey,
                                   const std::string &initial_topic, const std::string &up_binding,
                                   const std::string &down_binding, const std::string &confirm_binding,
                                   const std::string &quit_binding );
/** Topic count and the focused topic's position, spoken on request. */
std::string format_help_menu_summary( size_t topic_count, size_t position );
/** Format a menu topic with its hotkey and title. */
std::string format_help_topic_focus( const std::string &hotkey, const std::string &topic_name );
/** Format compass directions and bound keys semantically. */
std::string format_help_directions(
    const std::vector<std::pair<std::string, std::vector<std::string>>> &direction_bindings );
std::string format_creature_attitude( const std::string &attitude );
/** Describe creature health with the same injury tiers as the monster description. */
std::string format_creature_health( int current, int maximum );
std::string format_nearby_list_entry( const nearby_list_entry &entry );
std::string format_nearby_list_context( const std::string &mode, int result_count,
                                        const std::string &change = {} );
/** Mode, result count, and the focused row's result and location positions. */
std::string format_nearby_list_summary( const std::string &mode, int result_count,
                                        const nearby_list_entry &entry );
/** Build concise list speech without reading the focused entry's description. */
std::string format_uilist_focus( const std::string &label, const std::string &column,
                                 bool unavailable, const std::string &state = {} );
/** Name, item count, navigation hint, and focused row, spoken once on opening. */
std::string format_uilist_open( const std::string &context, size_t count,
                                const std::string &focus, const std::string &details_binding,
                                bool has_details );
/** Whether an item action is possible, which the visual menu shows only by color. */
enum class item_action_availability {
    available,
    not_now,
    not_applicable
};
std::string format_item_action_menu_context( const std::string &item_name );
/** Empty for an available action. */
std::string format_item_action_availability( item_action_availability availability );
std::string format_auto_pickup_rule_state( bool has_rule );
/** Speak a visual hint whose fields are separated by line breaks or box-drawing lines. */
std::string format_visual_hint( const std::string &hint );
/** Describe the focused button in an option-bearing query popup. */
std::string format_popup_option_focus( const std::string &option );
/** Describe a button query; lists choices on opening when there are at least three. */
std::string format_popup_prompt( const std::string &question, const std::string &selected,
                                 const std::vector<std::string> &choices,
                                 const std::string &left_binding,
                                 const std::string &right_binding,
                                 const std::string &confirm_binding, bool include_hint );
/** Describe a text input's prompt and complete current value. */
std::string format_text_input( const std::string &prompt, const std::string &description,
                               const std::string &value );
/** Name a single character for speech, including spaces. */
std::string format_text_character( const std::string &character );
/** Read the Unicode character after an insertion cursor, or the end of a field. */
std::string format_text_cursor( const std::string &value, size_t cursor );
/** Format a key combination for screen reader speech, making uppercase letters explicit (e.g. "Shift+y", "Ctrl+Shift+y"). */
std::string format_spoken_key( const std::string &raw_key );
/** Describe a focused keybinding action without reading every assigned key. */
std::string format_keybinding_focus( const std::string &name, const std::string &primary_key,
                                     bool local );
/** Describe every assigned key and the binding state on demand. */
std::string format_keybinding_details( const std::string &name,
                                       const std::vector<std::string> &keys, bool local, bool customized );
/** Announce opening of the keybinding screen with basic navigation hints. */
std::string format_keybinding_open( const std::string &up_binding, const std::string &down_binding,
                                    const std::string &tab_binding, const std::string &confirm_binding,
                                    const std::string &details_binding, size_t action_count,
                                    const std::string &focus );
/** Announce keybinding filter state and search results count. */
std::string format_keybinding_filter( const std::string &query, int results_count,
                                      const std::string &tab_binding, bool include_hint );
/** Use the document reader when spoken list details exceed a concise announcement. */
bool uilist_details_need_document( const std::string &details );
/** Split a long list description into spoken sentences, independent of visual wrapping. */
std::vector<std::string> split_uilist_detail_sentences( const std::string &details );
/** Split formatted item details into clean semantic lines and sentences for document reading. */
std::vector<std::string> format_item_info_paragraphs( const std::string &type_name,
        const std::string &raw_info );
/** Semantic inventory row, independent of item and renderer types. */
struct inventory_focus_entry {
    std::string caption;
    std::string category;
    std::string location;
    std::vector<std::pair<std::string, std::string>> cells;
    std::string denial;
    std::string selection;
    bool favorite = false;
    bool has_contents = false;
    bool contents_collapsed = false;
    bool not_owned = false;
    /** Owner name; spoken only with all cells. */
    std::string owner;
    /** Predicted destination of a transfer; spoken only with all cells. */
    std::string destination;
};
std::string format_inventory_focus( const inventory_focus_entry &entry, bool include_context,
                                    bool include_all_cells );
std::string format_inventory_filter( const std::string &query, size_t result_count );
/** Selection state of a multiselect row; empty when the row cannot be selected. */
std::string format_inventory_selection_state( size_t chosen, size_t available, bool selectable );
std::string format_inventory_selection_total( size_t selected_entries );
std::string format_inventory_selection_change( const std::string &caption,
        const std::string &state, size_t selected_entries );
std::string format_inventory_comparison_choice( const std::string &first,
        const std::string &second );
/** Weight and volume against their limits, as shown by a selector's statistics. */
struct inventory_capacity {
    std::string weight;
    std::string weight_limit;
    std::string weight_unit;
    std::string volume;
    std::string volume_limit;
    std::string volume_unit;
    bool over_weight = false;
    bool over_volume = false;
};
/** Values and warnings, or only the warnings; empty when only warnings are asked and none apply. */
std::string format_inventory_capacity( const inventory_capacity &capacity, bool include_values );
/** An item that a pickup stored, with the container it went into, if known. */
struct pickup_result_item {
    std::string item;
    std::string destination;
};
/** An item that a pickup left behind, with the reason. */
struct pickup_failure {
    std::string item;
    std::string reason;
};
std::string format_pickup_result( const std::vector<pickup_result_item> &picked,
                                  const std::vector<pickup_failure> &failed );
/** Result of a deliberate drop; `onto` is false when the items went into furniture or a vehicle. */
std::string format_drop_result( const std::string &destination,
                                const std::vector<std::string> &items, bool onto );
/** Semantic row of a two-pane item transfer screen. */
struct transfer_row {
    std::string name;
    int count = 1;
    std::string weight;
    std::string volume;
    std::string category;
    std::string source_area;
    bool not_owned = false;
    bool auto_pickup = false;
    bool open_in_other_pane = false;
};
std::string format_transfer_row( const transfer_row &row, bool include_category, bool detailed );
/** Semantic pane of a two-pane item transfer screen. */
struct transfer_pane {
    std::string area;
    size_t item_count = 0;
    std::string capacity;
    std::string filter;
    std::string sort;
};
std::string format_transfer_pane( const transfer_pane &pane, bool source, bool include_capacity );
std::string format_transfer_status( const transfer_pane &source, const transfer_pane &destination );
/** An empty item name describes a bulk transfer counted in stacks. */
std::string format_transfer_result( const std::string &item_name, int moved, int requested,
                                    const std::string &destination );
enum class inventory_comparison_relation {
    first_better,
    second_better,
    equal,
    unknown
};
inventory_comparison_relation compare_inventory_values( double first, double second,
        bool lower_is_better );
std::string format_inventory_comparison_stat( const std::string &label,
        const std::string &first_name, const std::string &first_value,
        const std::string &second_name, const std::string &second_value,
        inventory_comparison_relation relation );
/** Collapse whitespace, trim, and optionally strip progress counters and percentages. */
std::string normalize_status_announcement( const std::string &message, bool progress );

/** Discrete condition state tracked between turns to detect urgent transitions. */
struct condition_tracker {
    std::string hunger_tier;
    std::string thirst_tier;
    std::string sleepiness_tier;
    std::string pain_tier;
    std::string weariness_tier;
    bool stamina_critical = false;
    std::vector<std::string> bleeding_limbs;
    std::vector<std::string> broken_limbs;
    bool initialized = false;
};

/** Evaluate condition transitions between previous and current states.
 *  Returns transition announcements (if any) and updates `tracker`. */
std::vector<std::string> evaluate_condition_transitions( condition_tracker &tracker,
        const condition_tracker &current );

enum class safe_mode_state {
    off,
    on,
    stopped
};

/** Semantic safe-mode state, including information otherwise conveyed only by color. */
std::string format_safe_mode_status( safe_mode_state state, bool auto_safe_mode,
                                     int turns_until_auto_safe );

struct status_snapshot {
    std::string health;
    std::string pain;
    bool bleeding = false;
    bool broken_limb = false;
    int stamina_percent = 0;
    int focus = 0;
    std::string weariness;
    std::string hunger;
    std::string thirst;
    std::string sleepiness;
    std::string body_temperature;
    std::string environment_temperature;
    size_t active_effect_count = 0;
    std::string wielded_weapon;
    std::string movement_mode;
    int speed = 0;
    std::string safe_mode;
    std::string time;
    std::string date;
    std::string weather;
};

struct status_summary_entry {
    std::string label;
    std::string value;
};

/** Format the seven concise status-review categories from a fixed semantic snapshot. */
std::vector<status_summary_entry> format_status_summary( const status_snapshot &snapshot );

/** Describe the nearest visible threats; `others` counts threats not described. */
std::string format_threat_announcement( const std::vector<std::string> &descriptions,
                                        size_t others );
/** Describe a threat by name, distance, and compass direction. */
std::string format_threat_description( const std::string &name, int distance,
                                       const std::string &direction );
std::string format_game_arrival( bool loaded, const std::string &name,
                                 const std::string &location );

/** One valid target of a direction prompt; `direction` is already spoken text. */
struct direction_target {
    std::string target;
    std::string direction;
};
/**
 * Spoken direction prompt: the prompt, the valid targets grouped by identical
 * target in the given order, then the input hint.  Empty `targets` says that
 * there is no valid target.
 */
std::string format_direction_prompt( const std::string &prompt,
                                     const std::vector<direction_target> &targets, const std::string &hint );
std::string format_direction_auto_selected( const direction_target &target );
std::string format_direction_invalid( const direction_target &target );

/** A run of consecutive route tiles with the same spoken name. */
struct travel_segment {
    std::string name;
    int count = 0;
};
/** Merge consecutive identical tile names of a route into segments. */
std::vector<travel_segment> merge_travel_segments( const std::vector<std::string> &names );
/**
 * Spoken travel start: destination, straight-line location such as "12 tiles
 * northeast", the step count (omitted when zero), and at most four segments.
 */
std::string format_travel_start( const std::string &destination, const std::string &location,
                                 int steps, const std::vector<travel_segment> &segments );
std::string format_travel_arrival( const std::string &tile );
std::string format_travel_stopped( const std::string &destination, const std::string &location );

/** Predicted outcome of one aim level shown by the targeting panel. */
struct target_aim_level {
    std::string name;
    /** Spoken key that fires with this aim level; empty when unbound. */
    std::string key;
    bool selected = false;
    int moves = 0;
    int hit_chance = 0;
    /** Translated outcome labels and chances in panel order, ending with the miss chance. */
    std::vector<std::pair<std::string, int>> outcomes;
};
std::string format_target_hit_chance( int chance );
/** One aim level for the targeting details. */
std::string format_target_aim_level( const target_aim_level &level );

/** What the targeting cursor is on, with the problems the panel shows only by color. */
struct target_focus {
    /** Visible creature or vehicle; empty when the cursor is on a tile without one. */
    std::string target;
    std::string attitude;
    /** Distance and direction, with the level difference when there is one. */
    std::string location;
    /** Look-around description of the tile, ending with its location. */
    std::vector<std::string> tile;
    /** Hit chance of the selected aim level; empty when the panel shows none. */
    std::string hit;
    bool invalid = false;
    bool out_of_range = false;
    bool out_of_ammo = false;
    bool no_turret_in_range = false;
    /** Visible obstacle on the line of fire. */
    std::string blocked_by;
    /** Visible creatures on the line of fire, nearest first. */
    std::vector<std::string> in_line_of_fire;
    bool in_area_of_effect = false;
};
/** Problems that forbid or endanger the attack; empty when there are none. */
std::string format_target_problems( const target_focus &focus );
/**
 * Concise focus: the target with its attitude, location, and hit chance, or
 * the look-around tile when `as_tile` is set or there is no target, then the
 * problems.
 */
std::string format_target_focus( const target_focus &focus, bool as_tile );
std::string format_target_count( size_t count );
/** Ammunition as the panel shows it; `remaining` of zero omits the count. */
std::string format_target_ammo( const std::string &ammo, int remaining, int capacity );
/** Entering target mode: title, weapon state, target count, and focus. */
std::string format_target_open( const std::string &title, const std::string &weapon,
                                size_t target_count, const std::string &focus );
/** Weapon state spoken on request in target mode. */
struct target_status {
    std::string firing_mode;
    std::string aim_mode;
    std::string ammo;
    std::string recoil;
    /** Percent steadiness of the fire key's aim; negative when the panel shows none. */
    int steadiness = -1;
    /** Zero when the cursor is on the player. */
    int distance = 0;
    int range = 0;
    /** Further panel lines such as spell cost or turrets in range, already formatted. */
    std::vector<std::string> extra;
};
std::string format_target_status( const target_status &status );

/** Whether one alternative of a recipe requirement group is available, shown only by color. */
enum class requirement_state {
    have,
    /** Present, but not enough to serve as both a tool and a component. */
    not_enough,
    missing,
    uses_favorite,
    uses_filled_container,
    uses_hallucinogen_or_poison
};
enum class requirement_kind {
    tools,
    components
};
struct requirement_option {
    std::string name;
    requirement_state state = requirement_state::missing;
};
/** One requirement group; any one of its alternatives satisfies it. */
struct requirement_group {
    requirement_kind kind = requirement_kind::tools;
    std::vector<requirement_option> options;
};
std::string format_requirement_state( requirement_state state );
bool requirement_group_met( const requirement_group &group );
size_t count_missing_requirement_groups( const std::vector<requirement_group> &groups );
/** One line per group, e.g. "Tools: hammer, have; or rock, missing." */
std::string format_requirement_group( const requirement_group &group );
/** "Craftable", the number of unmet requirements, or "Not craftable" when none can be named. */
std::string format_recipe_availability( bool craftable, size_t missing );
/** Semantic row of the crafting recipe list. */
struct recipe_row {
    std::string name;
    bool craftable = false;
    size_t missing = 0;
    /** A nested recipe group that expands in place. */
    bool group = false;
    bool expanded = false;
    bool favorite = false;
    bool unread = false;
    /** Words for states the list shows only by color, such as "uses rotten ingredients". */
    std::vector<std::string> warnings;
};
std::string format_recipe_row( const recipe_row &row );
std::string format_recipe_count( size_t count );
/** List name with its recipe count, hidden recipes, and whether it has new recipes. */
std::string format_recipe_list( const std::string &name, size_t count, size_t hidden,
                                bool unread );
/** A batch size row: "Batch size 3, 30 minutes, craftable." */
std::string format_recipe_batch( int batch_size, const std::string &time,
                                 const std::string &availability );

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_FORMAT_H
