#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_FORMAT_H
#define CATA_SRC_ACCESSIBILITY_FORMAT_H

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "accessibility_dialogue_model.h"
#include "accessibility_document_cursor.h"
#include "point.h"

// Renderer-independent semantic models and speech formatters.  Everything here
// is a pure function of its inputs so it can be tested without a backend.
namespace accessibility
{

struct semantic_list_entry {
    int distance;
    /** Offset from the player. */
    tripoint offset;
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
/** "2 levels above", "1 level below", or empty for the same level. */
std::string format_level_difference( int dz );
/** Join the non-empty phrases of one sentence with commas. */
std::string join_phrases( const std::vector<std::string> &parts );
/** format_semantic_sequence() over views, for callers that do not own the parts. */
std::string format_semantic_sequence_of_views( const std::vector<std::string_view> &parts );

/**
 * "Top" when a move toward the end wrapped to the start, "Bottom" when a move
 * toward the start wrapped to the end, spoken before the focused row.  Empty
 * without a wrap, for a zero `direction`, or when either index is negative.
 */
std::string format_list_wrap( int previous, int current, int direction );
/** The wrap, if any, then the focused row after a list move. */
std::string format_list_move( int previous, int current, int direction, const std::string &row );
/** Keys that move list focus besides UP and DOWN; screens handle different sets. */
enum list_move_keys : unsigned {
    list_move_arrows = 0,
    list_move_scroll = 1,
    list_move_pages = 2,
    list_move_ends = 4,
    list_move_all = list_move_scroll | list_move_pages | list_move_ends
};
/** -1 for list moves toward the start, 1 toward the end, 0 for other actions. */
int list_move_direction( std::string_view action, unsigned keys = list_move_all );
/** Item count spoken once when a list opens or a pane is switched to. */
std::string format_list_count( size_t count );
/** One-based focus position, spoken only on request. */
std::string format_list_position( size_t position, size_t count );
/**
 * Describe a world by its character count rather than visual parentheses:
 * "Survival, 3 characters" or "Survival, no characters".
 */
std::string format_world_row( const std::string &name, size_t character_count );
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
/** As above with a spoken count such as "3 spells". */
std::string format_uilist_open( const std::string &context, const std::string &count,
                                const std::string &focus, const std::string &details_binding,
                                bool has_details );
/** Whether an item action is possible, which the visual menu shows only by color. */
enum class item_action_availability : int {
    available,
    not_now,
    not_applicable
};
std::string format_item_action_menu_context( const std::string &item_name );
/** Empty for an available action. */
std::string format_item_action_availability( item_action_availability availability );
std::string format_auto_pickup_rule_state( bool has_rule );
/** Speak a visual hint whose fields are separated by line breaks or box-drawing lines. */
std::string format_visual_hint( std::string_view hint );
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
bool uilist_details_need_document( std::string_view details );
/** Split a long list description into spoken sentences, independent of visual wrapping. */
std::vector<std::string> split_uilist_detail_sentences( std::string_view details );
/** Split formatted item details into clean semantic lines and sentences for document reading. */
std::vector<std::string> format_item_info_paragraphs( const std::string &type_name,
        const std::string &raw_info );
/** Split formatted text into source lines and sentences, independent of visual wrapping. */
std::vector<std::string> format_document_paragraphs( const std::string &text );
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
/** A row of a trade pane: the inventory row with its unit price and marked count. */
struct trade_row {
    inventory_focus_entry item;
    /** Formatted price of one unit; empty when the row cannot be traded. */
    std::string unit_price;
    size_t marked = 0;
    size_t available = 0;
};
/** "Marked" or "3 marked"; empty when nothing of the row is marked. */
std::string format_trade_marked( size_t marked, size_t available );
/** Name, unit price, marks, and refusal; details add location, state, and owner. */
std::string format_trade_row( const trade_row &row, bool include_context, bool include_details );
/** The row after marking: its name and the new marked count, or "Unmarked". */
std::string format_trade_mark_change( const std::string &caption, size_t marked,
                                      size_t available );
/** Running balance of a trade and whether the trader accepts it. */
struct trade_balance {
    /** The trader exchanges items freely, without a balance. */
    bool exchange = false;
    /** The player owes the trader the amount. */
    bool debt = false;
    std::string amount;
    bool acceptable = true;
};
/** "Balance: you owe $5.00, trader refuses" or "Balance: credit $3.00, acceptable". */
std::string format_trade_balance( const trade_balance &balance );
/** Name of a trade pane: the player's goods or the trader's goods. */
std::string format_trade_pane_label( bool yours );
/** Trade purpose, trader, and balance, spoken when the trade opens. */
std::string format_trade_opening( std::string_view title, const std::string &trader,
                                  const trade_balance &balance );
/** The whole trade state spoken on request. */
struct trade_summary {
    std::string title;
    std::string trader;
    trade_balance balance;
    /** Value of the marked goods of each side; empty to omit. */
    std::string buying;
    std::string selling;
    /** Bank balance; empty when banking is unavailable. */
    std::string bank;
};
std::string format_trade_summary( const trade_summary &summary );
/** Balance after moving it to or from the bank account. */
std::string format_trade_bank_change( const trade_balance &balance, const std::string &bank );
/** Spoken when the trade window closes. */
std::string format_trade_result( bool traded );
/** Tabs of the missions screen. */
enum class mission_category : int {
    active,
    completed,
    failed
};
/** "Active", "Completed", or "Failed". */
std::string format_mission_category( mission_category category );
/** "3 missions" or "no missions". */
std::string format_mission_count( size_t count );
/** A mission row as the missions screen shows it. */
struct mission_row {
    std::string title;
    mission_category category = mission_category::active;
    /** The mission is the current objective. */
    bool tracked = false;
    /**
     * Remaining time of an active mission, or the time since an inactive one
     * ended; empty when the screen shows none.
     */
    std::string time;
    /** An active mission whose deadline has passed. */
    bool deadline_passed = false;
};
/**
 * "Reach Refugee Center, current objective, deadline in 2 days." or
 * "Find a book, failed 3 hours ago."
 */
std::string format_mission_row( const mission_row &row );
/** "Active, 3 missions. <row>" or "Failed, no missions." */
std::string format_mission_tab( mission_category category, size_t count,
                                const std::string &focus );
/** Screen name followed by the tab, spoken when the missions screen opens. */
std::string format_mission_screen_opening( const std::string &tab );
/**
 * A known map location: "Target: 12 map tiles northeast, about 288 m." or
 * "Target: here."  `label` is the translated kind of location.
 */
std::string format_mission_location( const std::string &label, int omt_distance,
                                     const std::string &direction, const std::string &length );
/** Spoken when a mission shows no target location. */
std::string format_mission_no_location();
/** Everything the mission description pane shows. */
struct mission_details {
    mission_row row;
    size_t position = 0;
    size_t count = 0;
    /** Empty when the giver is unknown or not nearby. */
    std::string giver;
    std::string faction;
    /** Formatted giver location; empty when the giver is unknown. */
    std::string giver_location;
    /** Description with tags expanded; it states the objective and reward. */
    std::string description;
    /** Deadline or end time lines as the screen shows them. */
    std::vector<std::string> time_lines;
    /** Formatted target location; empty when the mission has no target. */
    std::string location;
};
/** Paragraphs for the document view: status, giver, objective, time, and location. */
std::vector<std::string> format_mission_details( const mission_details &details );
/** Tab, position, and the current objective, spoken on request. */
std::string format_mission_summary( mission_category category, size_t position, size_t count,
                                    const std::string &current_objective );
/** Spoken after CONFIRM sets the current objective. */
std::string format_mission_tracked( const std::string &title );
/** Spoken when CONFIRM cannot set the current objective. */
std::string format_mission_track_unavailable( mission_category category, size_t count );
/** A scenario, profession, background, or point pool row in character creation. */
struct character_choice_row {
    std::string name;
    /** Point cost; a negative cost earns points. */
    int points = 0;
    /** False when the point pool does not show costs. */
    bool show_points = true;
    /** The choice is the character's current one. */
    bool selected = false;
    /** Why the choice cannot be picked; empty when it can. */
    std::string unavailable;
    /** The visual list marks the cost as unaffordable. */
    bool short_of_points = false;
};
/** "Evacuee, 0 points, selected." or "Lab patient, 2 points. Unavailable: <reason>." */
std::string format_character_choice_row( const character_choice_row &row );
/** Point pools as the character creation header shows them. */
enum class character_point_pool : int {
    /** Survivor: no point limits. */
    freeform,
    one_pool,
    multi_pool,
    transfer
};
struct character_point_balance {
    character_point_pool pool = character_point_pool::freeform;
    /** Unspent points of each pool, without borrowing. */
    int stat_points = 0;
    int trait_points = 0;
    int skill_points = 0;
    /** Points left in total. */
    int total = 0;
    /** No pool is overspent. */
    bool valid = true;
};
/** "4 points left", the multi-pool breakdown, or empty for a pool without limits. */
std::string format_character_point_balance( const character_point_balance &balance );
/**
 * A character creation tab: "Profession, 42 items. <row>".  `opening` adds the
 * screen name when character creation starts.
 */
std::string format_character_creation_tab( const std::string &tab, size_t count,
        const std::string &focus,
        bool opening );
/** Spoken after CONFIRM picks or toggles a choice, followed by the point balance. */
std::string format_character_choice_selected( std::string_view name, bool added,
        const std::string &balance );
/** Spoken after the filter changes: "Filter doc, 3 items. <row>" or "Filter cleared, ...". */
std::string format_character_choice_filter( const std::string &filter, size_t count,
        const std::string &focus );
/** Spoken after the sort order changes. */
std::string format_character_choice_sort( bool by_name, const std::string &focus );
/** Tab, count, position, current choices, point balance, and difficulty on request. */
std::string format_character_tab_summary( const std::string &tab, size_t position, size_t count,
        const std::vector<std::string> &current, const std::string &balance,
        const std::string &difficulty );
/**
 * A character creation tab with panes, or a pane switch when `tab` is empty:
 * "Traits. Negative traits, 30 items. <row>".
 */
std::string format_character_creation_pane( const std::string &tab, const std::string &pane,
        size_t count, const std::string &focus, bool opening );
/** "Strength 8, average human." followed by a limit or cost note such as "Increasing ... costs 2 points". */
std::string format_character_stat_row( const std::string &stat, int value,
                                       const std::string &level, const std::string &note );
/**
 * Spoken after a stat or skill changes: "Strength 10, 4 points left." or
 * "Level 2, 3 points over budget.", followed by an optional note.
 */
std::string format_character_value_change( const std::string &value, const std::string &balance,
        const std::string &note );
/** A trait row in character creation. */
struct character_trait_row {
    std::string name;
    /** Point cost; a negative cost earns points. */
    int points = 0;
    /** False when the point pool does not show costs. */
    bool show_points = true;
    bool selected = false;
    /** What prevents removing the trait: "scenario", "profession", or "background". */
    std::string locked_by;
    /** Chosen traits that prevent taking this one. */
    std::vector<std::string> conflicts;
    /** What prevents taking the trait: "scenario" or "profession". */
    std::string forbidden_by;
};
/** "Fast Learner, 3 points, selected, locked by profession" or "..., conflicts with Slow Learner". */
std::string format_character_trait_row( const character_trait_row &row );
/** "Melee, level 2" or "Melee, level 3, 1 from profession". */
std::string format_character_skill_row( const std::string &skill, int level,
                                        int from_profession );
/** A Description tab field: "Name: Ada". */
std::string format_character_description_field( const std::string &label,
        const std::string &value );
/** "O positive" or "AB negative". */
std::string format_blood_type( const std::string &type, bool rh_positive );
/** "Shelter, 3 variants" or "Random location, 5 variants"; one variant omits the count. */
std::string format_character_start_location( const std::string &name, int variants );
/**
 * A list, pane, or screen that just opened: "World selection, 4 items. <row>".
 * An empty list speaks "No items" without a row.
 */
std::string format_list_open( const std::string &name, size_t count, const std::string &focus );
/** A labeled setting: "World name: Survival" or "Population density: High". */
std::string format_world_setting( const std::string &label, const std::string &value );
/** Create World summary: position, name, settings, and the load order size. */
std::string format_world_creation_summary( size_t position, size_t count,
        const std::vector<std::string> &settings, bool custom_options, size_t mod_count );
/** A mod row in the mod manager or the active world mods list. */
struct world_mod_row {
    /** Display name, or the mod id when the mod is not installed. */
    std::string name;
    /** Mod category, such as "Content"; the list groups rows under it. */
    std::string category;
    /** The mod is in the world's load order; spoken only in the mod list. */
    bool active = false;
    /** The mod is not installed. */
    bool missing = false;
    bool obsolete = false;
    /** A mod in the load order conflicts with this mod. */
    bool incompatible = false;
    /** Why the mod cannot be activated, such as a missing dependency; empty when it can. */
    std::string unavailable;
};
/** "Magiclysm, active, Content." or "Old mod, obsolete, Obsolete mods." */
std::string format_world_mod_row( const world_mod_row &row );
/** The mod list pane with its category tab: "Mod List: Default". */
std::string format_mod_list_pane( const std::string &pane, const std::string &tab );
/** "Added Magiclysm, position 5 of 5.  Also added: Aftershock." */
std::string format_mod_added( const std::string &name, size_t position, size_t count,
                              const std::vector<std::string> &also_added,
                              const std::vector<std::string> &also_removed );
/** "Removed Magiclysm, 4 mods in load order.  Also removed: <dependents>." */
std::string format_mod_removed( const std::string &name, size_t count,
                                const std::vector<std::string> &also_removed );
/** "Cannot add Magiclysm: <reason>." */
std::string format_mod_not_added( const std::string &name, const std::string &reason );
/** "Position 3 of 5." after a mod, rule, or panel moves. */
std::string format_mod_moved( size_t position, size_t count );
/** "Cannot move up: <reason>." */
std::string format_mod_not_moved( bool up, const std::string &reason );
/** Mod manager pane, position, load order size, and filter on request. */
std::string format_mod_manager_summary( const std::string &pane, size_t position, size_t count,
                                        size_t active_count, const std::string &filter );
/** A screen and its page, tab, or column: "Options: General". */
std::string format_screen_page( const std::string &screen, const std::string &page );
/** A list change and the new item count: "Rule added, 3 items" or "Rule removed, No items". */
std::string format_list_change( const std::string &change, size_t count );
/** Rule managers: the change with the new rule count, then the focused rule. */
std::string format_rule_list_change( const std::string &change, size_t count,
                                     const std::string &focused_rule );
/** Rule managers: the new one-based position, or why the rule did not move. */
std::string format_rule_move( bool moved, bool up, size_t position, size_t count );
/** Rule managers: "Enabled" or "Disabled". */
std::string format_rule_enabled( bool enabled );
/** Rule managers on closing: "Changes saved" or "Changes discarded". */
std::string format_rule_changes( bool saved );
/** A shortcut-marked label without its markup: "[<Global>]" becomes "Global". */
std::string format_shortcut_label( std::string_view label );
/**
 * An option row: "Use tiles: True".  `unavailable` names the unmet
 * prerequisite shown only in gray: "Font blending: False, Unavailable: requires Use tiles".
 */
std::string format_option_row( const std::string &name, const std::string &value,
                               const std::string &unavailable );
/** An option group header: "Graphics, collapsed, 12 options" or "Graphics, expanded, 12 options". */
std::string format_option_group_row( const std::string &name, bool expanded, size_t option_count );
/** After a group header is toggled: "Expanded, 12 options" or "Collapsed". */
std::string format_option_group_toggled( bool expanded, size_t option_count );
/**
 * The option tooltip requested with Alt+D.  The visual tooltip separates the
 * description from its default with " #"; the parts become sentences, and
 * `note` (such as the world options warning) follows them.
 */
std::string format_option_tooltip( const std::string &tooltip, const std::string &note );
/** Options screen summary: "Options: General, 3 of 20.  Page 1 of 8." */
std::string format_options_summary( const std::string &page, size_t position, size_t count,
                                    size_t page_number, size_t page_count );
/** A safe mode rule with its columns as words rather than positions. */
struct safemode_rule_row {
    std::string rule;
    bool active = true;
    bool whitelist = false;
    /** Sound rule; otherwise a hostile-spotted rule. */
    bool sound = false;
    /** Attitude name; only hostile-spotted rules use it. */
    std::string attitude;
    int proximity = 0;
    /** Walking, driving, or both. */
    std::string mode;
};
/** Whether a safe mode rule uses its attitude and distance columns. */
bool safemode_rule_uses_attitude( const safemode_rule_row &row );
bool safemode_rule_uses_distance( const safemode_rule_row &row );
/**
 * The six safe mode column values in visual order, spoken after a change:
 * "zombie", "Hostile", "5", "Blacklist", "Hostile", "Both".  An unused
 * column says "not applicable".
 */
std::vector<std::string> format_safemode_rule_values( const safemode_rule_row &row );
/**
 * The six safe mode columns in visual order, spoken as the focused column:
 * "Rule: zombie", "Attitude: Hostile", "Distance: 5", "Blacklist",
 * "Category: Hostile", "Mode: Both".  An unused column says "not applicable".
 */
std::vector<std::string> format_safemode_rule_fields( const safemode_rule_row &row );
/** "zombie, Blacklist, Category: Hostile, Attitude: Hostile, Distance: 5, Mode: Both, disabled." */
std::string format_safemode_rule_row( const safemode_rule_row &row );
/** "wooden arrow, Include." or "*, Exclude, disabled." */
std::string format_autopickup_rule_row( const std::string &rule, bool exclude, bool active );
/** Rule manager summary: "Safe mode manager: Global, 2 of 5.  Safe mode enabled.  Column: Rule." */
std::string format_rule_manager_summary( const std::string &screen, size_t position, size_t count,
        const std::string &state, const std::string &column );
/** A sidebar panel row: "Stats, shown" or "Location, hidden". */
std::string format_panel_row( const std::string &name, bool shown );
/** A sidebar layout row: "Classic, active" or "Labels". */
std::string format_layout_row( const std::string &name, bool active );
/** Moving a sidebar panel: "Moving Stats, position 3 of 20" at the start. */
std::string format_panel_move_start( const std::string &name, size_t position, size_t count );
/** "Moved Stats to position 5 of 20." */
std::string format_panel_moved( const std::string &name, size_t position, size_t count );
/**
 * Offset of a map tile from the player: "3 north 2 east", "5 west, 1 level
 * below", or "here".  `y` grows southwards and `z` upwards.
 */
std::string format_overmap_offset( const tripoint &offset );
/** "Level -1" for an overmap z-level. */
std::string format_overmap_level( int z );
/** One overmap tile as the map cursor speaks it. */
struct overmap_tile {
    /** Known terrain name, or empty when the tile is unexplored. */
    std::string terrain;
    /** Note text without its glyph and color prefix; empty without a note. */
    std::string note;
    /** Danger radius of the note, or -1 when it is not marked dangerous. */
    int danger_radius = -1;
    /** Inside the radius of another dangerous note. */
    bool in_dangerous_area = false;
    /** Titles of active missions that target this tile. */
    std::vector<std::string> missions;
    bool route_destination = false;
    bool on_route = false;
    /** Offset from the player in map tiles. */
    tripoint offset = tripoint::zero;
};
/**
 * "forest, note: Base, mission target: Find a book, 3 north 2 east." or, when
 * `terrain_repeated`, the terrain and markers without the offset.
 */
std::string format_overmap_tile( const overmap_tile &tile, bool terrain_repeated );
/** Spoken when the overmap opens: its title (or prompt) and the cursor tile. */
std::string format_overmap_opening( std::string_view prompt, const std::string &tile );
/** Spoken after a z-level change: "Level -1. <tile>" */
std::string format_overmap_level_change( int z, const std::string &tile );
/** A known place in the known-places list. */
struct known_place {
    std::string name;
    int distance = 0;
    /** Compass direction from the player; ignored when `distance` is 0. */
    std::string direction;
    /** Level difference from the player; positive is above. */
    int dz = 0;
    bool city = false;
    bool camp = false;
    /** Note text; empty when the place has no note. */
    std::string note;
    int danger_radius = -1;
    bool in_dangerous_area = false;
    std::vector<std::string> missions;
    bool current_objective = false;
    bool route_destination = false;
};
/** "evac shelter, 12 map tiles south, note: supplies" without a final period. */
std::string format_known_place_row( const known_place &place );
/** "12 places" or "no places". */
std::string format_known_place_count( size_t count );
/** Route preview after the first CHOOSE_DESTINATION press. */
std::string format_overmap_route_preview( const std::string &destination,
        const std::string &location, int length, const std::string &confirm_key );
/** Spoken when no route to the cursor can be found. */
std::string format_overmap_no_route( const std::string &destination, bool different_level );
/** Spoken when a route command needs a route and none is selected. */
std::string format_overmap_no_route_selected();
/** Current route for the summary: "Route: <destination>, <location>." or "No route." */
std::string format_overmap_route_state( const std::string &destination,
                                        const std::string &location );
/** Overmap summary: prompt, cursor tile, level, and route. */
std::string format_overmap_summary( std::string_view prompt, const std::string &tile, int z,
                                    const std::string &route );
/** "Search results, 5 results. <focus>" */
std::string format_overmap_search_results( size_t count, const std::string &focus );
/** "Result 2 of 5. <focus>" */
std::string format_overmap_search_position( size_t position, size_t count,
        const std::string &focus );
/** Spoken after TOGGLE_EXPLORED: "Marked explored" or "Marked unexplored". */
std::string format_overmap_explored( bool explored );
/** A zone as the zones manager lists it. */
struct zone_row {
    std::string name;
    /** Zone type name; omitted from speech when it equals `name`. */
    std::string type;
    bool enabled = true;
    bool personal = false;
    bool vehicle = false;
    /** Distance in tiles from the player to the zone center. */
    int distance = 0;
    /** Compass direction from the player; ignored when `distance` is 0. */
    std::string direction;
    /** Level difference from the player; positive is above. */
    int dz = 0;
};
/** "Loot: food, enabled, 5 tiles north." */
std::string format_zone_row( const zone_row &row );
/** "3 zones" or "no zones". */
std::string format_zone_count( size_t count );
/** "Zones manager, 3 zones. <row>" spoken when the zones manager opens. */
std::string format_zones_manager_opening( size_t count, const std::string &focus );
/** "Zone 4 by 3." with ", 2 levels" when the zone spans several levels. */
std::string format_zone_size( int width, int height, int levels );
/** "<name> enabled." or "<name> disabled." */
std::string format_zone_enabled( std::string_view name, bool enabled );
/** "<name> shown on map." or "<name> hidden from map." */
std::string format_zone_displayed( std::string_view name, bool displayed );
/** "<name>, 2 of 5." after reordering. */
std::string format_zone_moved( std::string_view name, size_t position, size_t count );
/** "Removed <name>. <focus>" or "Removed <name>. No zones." */
std::string format_zone_removed( std::string_view name, const std::string &focus );
/** "2 personal zones enabled." or "No personal zones changed." */
std::string format_personal_zones_changed( bool enabled, size_t changed );
/**
 * "Showing all zones, 3 zones. <row>" after the zone filter or faction
 * changed; `filter` names the new filter.
 */
std::string format_zone_list_change( const std::string &filter, size_t count,
                                     const std::string &focus );
/** "Showing all zones" or "Hiding distant zones". */
std::string format_zone_filter( bool show_all );
/** Everything the zones manager shows about one zone, spoken on request. */
struct zone_details {
    zone_row row;
    size_t position = 0;
    size_t count = 0;
    bool displayed = false;
    int width = 1;
    int height = 1;
    int levels = 1;
    /** Option label and value pairs as the options pane shows them. */
    std::vector<std::pair<std::string, std::string>> options;
};
std::string format_zone_details( const zone_details &details );
/** "Options: Filter: food." or empty when the zone has no options. */
std::string format_zone_options( const std::vector<std::pair<std::string, std::string>> &options );
/** "Zones manager. Showing all zones. Faction: x. 2 of 5. <row>" */
std::string format_zones_manager_summary( const std::string &filter, const std::string &faction,
        size_t position, size_t count, const std::string &focus );

/** Engine strain beyond the safe speed, shown by the speed gauge color. */
enum class vehicle_strain {
    none,
    light,
    heavy,
    severe
};
/** One fuel or power gauge of a driven vehicle. */
struct vehicle_fuel_gauge {
    std::string fuel;
    int percent = 0;
    /** Time until the tank is empty, or full when `filling`; empty when steady. */
    std::string estimate;
    bool filling = false;
};
/** Driving state of a vehicle, as the sidebar and gauges show it. */
struct vehicle_status {
    std::string name;
    /** Speed unit such as "mph". */
    std::string units;
    /** Actual and cruise speed in `units`; negative when reversing. */
    int speed = 0;
    int cruise = 0;
    vehicle_strain strain = vehicle_strain::none;
    /** Facing in degrees from north, with its compass direction. */
    int heading = 0;
    std::string compass;
    /** Steering target when it differs from the facing. */
    bool turning = false;
    int steering = 0;
    std::string steering_compass;
    bool engine_on = false;
    bool skidding = false;
    std::vector<vehicle_fuel_gauge> fuels;
};
/** "20 mph" or "5 mph reverse". */
std::string format_vehicle_speed( int speed, const std::string &units );
/** "Heading northeast, 45 degrees." */
std::string format_vehicle_heading( const std::string &compass, int degrees );
/** "Cruise 60 mph, above safe speed." or "Cruise 80 mph, maximum." */
std::string format_vehicle_cruise( int cruise, const std::string &units, bool above_safe,
                                   bool at_limit );
/** "heavy engine strain", or empty without strain. */
std::string format_vehicle_strain( vehicle_strain strain );
/** "gasoline 45%, 2 hours until empty." */
std::string format_vehicle_fuel( const vehicle_fuel_gauge &gauge );
/**
 * "Car. Speed 20 mph, cruise 30 mph. Heading north, 0 degrees. Engine on.
 * gasoline 45%, 2 hours until empty."
 */
std::string format_vehicle_status( const vehicle_status &status );
/** "wall ahead, 4 tiles." or "tree behind, 1 tile." */
std::string format_vehicle_obstacle( const std::string &name, int distance, bool reversing );
/** Engine selection row: "V8 engine, gasoline, on". */
std::string format_vehicle_engine_row( const std::string &engine, const std::string &fuel,
                                       bool active );

/**
 * A tile of the vehicle examination grid in vehicle coordinates: x grows
 * toward the front and y toward the right.  The bounds are the vehicle's.
 */
struct vehicle_grid_position {
    point tile = point::zero;
    /** Smallest x and y of the vehicle's parts. */
    point min = point::zero;
    /** Largest x and y of the vehicle's parts. */
    point max = point::zero;
};
/** "front left", "rear", "center"; "outside, front" beyond the vehicle. */
std::string format_vehicle_location( const vehicle_grid_position &position );
/**
 * Exact position: "Row 2 of 5 from the front, column 1 of 3 from the left."
 * Beyond the vehicle: "1 tile in front of the vehicle, column 2 of 3 from the left."
 */
std::string format_vehicle_position_details( const vehicle_grid_position &position );
/** "broken", "damaged 40%", or empty for an undamaged part. */
std::string format_vehicle_part_condition( bool broken, int damage_percent );
/** One vehicle part in the examination part list. */
struct vehicle_part_row {
    std::string name;
    std::string location;
    bool broken = false;
    int damage_percent = 0;
    /** Words for states shown by symbols or color, such as "leaking". */
    std::vector<std::string> states;
};
/** "headlight, front left, damaged 40%" */
std::string format_vehicle_part_row( const vehicle_part_row &row );
/** "headlight, damaged 40%": the row without its location, for a tile summary. */
std::string format_vehicle_tile_part( const vehicle_part_row &row );
/** Grid cursor: "front left: frame; door, damaged 10%." or "outside, front: no parts, obstructed." */
std::string format_vehicle_tile( const std::string &location, const std::vector<std::string> &parts,
                                 bool obstructed );
/** "12 parts" or "No parts" */
std::string format_vehicle_part_count( size_t count );
/** "5 installable parts" or "No installable parts", for the install list. */
std::string format_vehicle_install_count( size_t count );
/** Task menu or part chooser row: "Repair" or "Remove. Unavailable: something is attached." */
std::string format_vehicle_task( const std::string &task, const std::string &unavailable_reason );
/** A chosen task that cannot start: "Repair headlight. Unavailable: 2 requirements missing." */
std::string format_vehicle_task_blocked( const std::string &task, const std::string &part,
        const std::string &reason );
/** "2 requirements missing" or "requirements not met" when none can be named. */
std::string format_vehicle_missing_requirements( size_t missing );
/** A skill level a vehicle task requires. */
struct vehicle_skill_requirement {
    std::string skill;
    int level = 0;
    bool met = false;
};
/** "Skills required: mechanics 2, have; electronics 1, missing." or "Skills required: none." */
std::string format_vehicle_skills( const std::vector<vehicle_skill_requirement> &skills );
/** "mechanics 5 for extra engines, missing." */
std::string format_vehicle_extra_skill( const std::string &skill, int level, bool met,
                                        const std::string &purpose );
/** "Time required: 30 minutes." */
std::string format_vehicle_task_time( const std::string &time );
/** "Lifting: 1 tool with jacking 3, have; or strength 12, missing." */
std::string format_vehicle_lift_requirement( const std::string &tool, bool tool_met,
        const std::string &strength, bool strength_met );
/** Speed figures of one medium in the examination statistics. */
struct vehicle_speed_stats {
    /** Empty for ground, or "On water" or "In the air". */
    std::string medium;
    int safe = 0;
    int top = 0;
    int acceleration = 0;
};
/** Vehicle statistics shown at the bottom of the examination screen. */
struct vehicle_workshop_stats {
    std::string name;
    std::string owner;
    /** Speed unit such as "mph". */
    std::string units;
    std::vector<vehicle_speed_stats> speeds;
    std::string mass;
    std::string cargo_used;
    std::string cargo_total;
    std::string status;
    std::string wheels;
    std::string most_damaged;
    bool most_damaged_repairable = true;
    std::string needs_repair;
    std::vector<vehicle_fuel_gauge> fuels;
    double air_drag = 0.0;
    bool has_water_drag = false;
    double water_drag = 0.0;
    bool has_rolling_drag = false;
    double rolling_drag = 0.0;
    int static_drag = 0;
    int offroad = 0;
    bool has_draft = false;
    double draft = 0.0;
    double clearance = 0.0;
};
std::string format_vehicle_workshop_stats( const vehicle_workshop_stats &stats );

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
std::string format_direction_prompt( std::string_view prompt,
                                     const std::vector<direction_target> &targets, std::string_view hint );
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

/** Construction catalog focus and category summary. */
std::string format_construction_row( const std::string &name, bool available,
                                     const std::string &unavailable_reason, size_t variants );
std::string format_construction_list( const std::string &category, size_t count );

/** Character sheet rows; the sheet shows several of these states only by color. */
/** A stat with its base value, e.g. "Strength 10, base 8." */
std::string format_character_stat( const std::string &name, int current, int base );
/** A labeled sheet value, e.g. "Weight, Normal." */
std::string format_character_value( const std::string &label, const std::string &value );
/** Body part warmth band, shown on the sheet only by the color of its temperature. */
enum class body_warmth {
    /** The part does not count towards warmth. */
    none,
    freezing,
    very_cold,
    chilly,
    comfortable,
    warm,
    very_hot,
    scorching
};
std::string format_body_warmth( body_warmth warmth );
struct encumbrance_row {
    std::string part;
    /** Encumbrance without the layering penalty, as the sheet's first number. */
    int encumbrance = 0;
    int layering = 0;
    /** The sheet's temperature number. */
    int warmth = 0;
    body_warmth warmth_state = body_warmth::none;
    /** Sort armor highlights parts covered by the focused item only by color. */
    bool covered = false;
};
/**
 * Concise: "Torso, encumbrance 12, layering 3, warmth comfortable."  Detailed
 * adds the total and the temperature number.
 */
std::string format_encumbrance_row( const encumbrance_row &row, bool detailed );
/** A cause of changed speed or move cost: "Pain, speed -10." */
std::string format_speed_row( std::string_view cause, bool speed, int value, bool percent );
struct skill_row {
    std::string name;
    int level = 0;
    int exercise = 0;
    /** Effective dodge for the dodge skill; empty otherwise. */
    std::string effective;
    /** Knowledge exceeds practical ability; the sheet shows it by color. */
    bool rusty = false;
    bool training = true;
    /** An active bionic sets the level; the sheet shows it by color. */
    bool set_by_bionic = false;
};
/** "Melee 3, 45%, training." */
std::string format_skill_row( const skill_row &row );
/** Trait kinds the sheet distinguishes only by color. */
enum class trait_kind {
    positive,
    negative,
    neutral,
    mixed,
    threshold,
    profession,
    attunement,
    debug
};
std::string format_trait_row( std::string_view name, trait_kind kind );
std::string format_bionic_row( const std::string &name, int count );
/** Known, learning with progress, or not known when it cannot be learned by practice. */
std::string format_proficiency_row( std::string_view name, bool known, bool learning,
                                    int progress );
/** Character sheet tab: its name, values it shows above the list, its count, and the focus. */
std::string format_character_tab( const std::string &tab, const std::vector<std::string> &header,
                                  size_t count, const std::string &focus );

/** Bionics and mutations screens: what selecting a row does. */
enum class power_menu_mode {
    activating,
    examining,
    reassigning,
    hiding
};
/** "Activating", "Examining"; reassigning and hiding say what selecting does. */
std::string format_power_menu_mode( power_menu_mode mode );
/** "Key a", "Key Shift+a", or "No key" for an unassigned letter. */
std::string format_assigned_key( const std::string &key );
struct bionic_power_row {
    std::string name;
    /** Toggled or charging bionics say on or off; others say on only while powered. */
    bool toggled = false;
    bool powered = false;
    /** The screen shows power sources only by color. */
    bool power_source = false;
    /** Displayed energies; empty when the bionic has no such cost. */
    std::string activate;
    std::string deactivate;
    std::string trigger;
    std::string over_time;
    /** Turns per `over_time` drain. */
    int charge_turns = 1;
    bool incapacitated = false;
    bool sprite_hidden = false;
    /** Fuel saving threshold in percent; negative when fuel saving is off. */
    int fuel_saving = -1;
    /** Fuel saving keeps the powered bionic from burning fuel now. */
    bool fuel_saving_inactive = false;
    /** Off and the current power is below the activation cost. */
    bool not_enough_power = false;
};
/** "Night vision, on, 5 J per turn." or "Laser, off, 50 kJ to activate. Unavailable: not enough power." */
std::string format_bionic_power_row( const bionic_power_row &row );
/** "Bionic power 50.5 kJ of 100 kJ. Available fuel: battery 5." */
std::string format_bionic_power( const std::string &current, const std::string &maximum,
                                 const std::vector<std::string> &fuels );
struct mutation_power_row {
    std::string name;
    bool activated = false;
    bool powered = false;
    /** A starting trait rather than a mutation; the screen shows it only by color. */
    bool starting_trait = false;
    bool sprite_hidden = false;
    int cost = 0;
    /** Resources the cost is paid in, such as "kcal" or "mana". */
    std::vector<std::string> resources;
    /** Displayed cooldown; empty when there is none. */
    std::string cooldown;
    /** Off and one of its resources is too low to activate it. */
    bool insufficient = false;
};
/** "Night vision, on, 10 kcal, cooldown 5 minutes." */
std::string format_mutation_power_row( const mutation_power_row &row );

/** Spell menu row. */
struct spell_row {
    std::string name;
    /** Translated spell class name, or "Classless" as the menu shows; omitted when empty. */
    std::string spell_class;
    /** Empty when the spell can be cast; otherwise why the menu disables it. */
    std::string unavailable_reason;
    /** Energy cost such as "10 mana" or "no cost". */
    std::string cost;
    /** Encumbrance raises the cost or the casting time; the menu shows it by label or color. */
    bool cost_impeded = false;
    std::string casting_time;
    bool time_impeded = false;
    /** The menu marks favorites only with a star. */
    bool favorite = false;
};
/**
 * "Magic missile, Magus, castable, 10 mana, 100 moves, favorite." or
 * "Magic missile, Magus, 10 mana, 100 moves. Unavailable: not enough mana."
 */
std::string format_spell_row( const spell_row &row );
/** "3 spells" or "No spells". */
std::string format_spell_count( size_t count );
/** Martial arts style row: name, "current" for the style in use, and its first sentence. */
std::string format_martial_style_row( std::string_view name, bool current,
                                      std::string_view description );

/**
 * A morale window line: "Total morale: +12." or "Ate a good meal, 25%."  A
 * label ending with a colon is followed by the value without a comma.
 */
std::string format_morale_line( std::string_view label, std::string_view value );
/**
 * A five-character health bar in words at the bar's own precision, where each
 * '|' is two tenths and '\' one: "health 7 of 10".  A bar of dashes is zero and
 * ':' is under one tenth.
 */
std::string format_health_bar( std::string_view bar );
/** A splinted limb's mending bar in fifths: "mending 2 of 5". */
std::string format_mending_bar( int filled );
/** Body status part: "Head, health 10 of 10, has details." or "Left arm, missing." */
std::string format_body_part_row( const std::string &name, const std::string &health,
                                  bool missing, bool has_details );
/** Medical health row: part, health in words, then conditions such as "broken". */
std::string format_medical_health_row( std::string_view part, std::string_view health,
                                       const std::vector<std::string> &conditions );
/** Proficiencies window row: "Knapping, known." or "Tailoring, learning, 40%." */
std::string format_proficiency_window_row( std::string_view name, bool known, int progress );
/** Worn item in the sort armor list; the list shows conflicts only by color. */
struct armor_row {
    std::string name;
    /** "left" or "right" for an item worn on one side; empty otherwise. */
    std::string side;
    /** Hidden from the character sprite. */
    bool hidden = false;
    /** Several items share a layer on a body part. */
    bool stacking_conflict = false;
    /** Worn over items that normally go outside it. */
    bool out_of_order = false;
    /** Encumbrance on the filtered body part; negative to omit. */
    int encumbrance = -1;
};
/** "Hoodie, left, hidden, layering conflict, out of order." */
std::string format_armor_row( const armor_row &row );
/**
 * The moving item's neighbours in the list, which runs from innermost to
 * outermost: "above hoodie, below T-shirt", "innermost, above hoodie",
 * "outermost, below T-shirt", or "only item".
 */
std::string format_armor_position( std::string_view inner, std::string_view outer );
/** Starting to move a worn item: "Moving hoodie." */
std::string format_armor_moving( std::string_view name );
/** Finishing a move: "Placed." and the row at its new position. */
std::string format_armor_placed( const std::string &row );
/** Toggling the sprite: "Sprite hidden." or "Sprite shown." and the row. */
std::string format_armor_sprite( bool hidden, const std::string &row );
/**
 * Sort armor summary: the filter with its count, each item with encumbrance and
 * conflicts in layer order, then the total when `total` is not negative.
 */
std::string format_armor_summary( const std::string &filter, const std::vector<armor_row> &rows,
                                  int total, int layering );

/** Dialogue history element: "Rubik: Hello." or the text alone without a speaker. */
std::string format_dialogue_utterance( std::string_view speaker, std::string_view text );
/** What a response does, which the dialogue window shows only by color. */
enum class dialogue_response_hint {
    none,
    /** Performs an action rather than saying something. */
    action,
    /** Leaves the player helpless. */
    helpless,
    /** May make the other side attack. */
    hostile
};
struct dialogue_response_row {
    /** Displayed hotkey, such as "a" or "L". */
    std::string key;
    /** Response text including any trial, such as "[LIE 50%] …". */
    std::string text;
    dialogue_response_hint hint = dialogue_response_hint::none;
    /** Shown but its condition is not met. */
    bool unavailable = false;
    std::string reason;
};
/** "a, Ask about the job." or "c, [LIE 50%] Lie. Unavailable: requires charm." */
std::string format_dialogue_response( const dialogue_response_row &row );
/** Final conversation element: "Your response, 5 options." */
std::string format_dialogue_prompt( const std::string &label, size_t count );

/** Activity lifecycle speech, independent of handlers and game state. */
std::string format_activity_start( std::string_view label, const std::string &duration,
                                   bool resuming );
std::string format_activity_result( std::string_view label, bool finished, bool canceled,
                                    std::string_view reason, const std::vector<std::string> &messages );
std::string format_activity_progress( std::string_view label, std::string_view progress,
                                      const std::string &remaining );

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_FORMAT_H
