#include <algorithm>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "accessibility.h"
#include "accessibility_format.h"
#include "accessibility_test_helpers.h"
#include "cata_catch.h"
#include "help.h"
#include "point.h"

// Expected speech strings mirror the spoken output, which uses single spaces after sentences.
// NOLINTBEGIN(cata-text-style)

TEST_CASE( "activity_lifecycle_formatters_preserve_outcomes_and_reasons",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_activity_start( "Crafting knife", "2 hours", false ) ==
           "Started: Crafting knife. About 2 hours." );
    CHECK( accessibility::format_activity_start( "Reading book", "", true ) ==
           "Resumed: Reading book." );
    CHECK( accessibility::format_activity_result( "Waiting", true, false, "", {} ) ==
           "Finished: Waiting." );
    CHECK( accessibility::format_activity_result( "Reading book", false, true,
            "Zombie spotted!", {} ) == "Stopped: Reading book. Zombie spotted!" );
    CHECK( accessibility::format_activity_result( "Reading book", true, false, "",
    { "It's too dark to read!" } ) == "It's too dark to read!" );
    CHECK( accessibility::format_activity_result( "Crafting knife", true, false, "",
    { "You craft a knife.", "Fabrication increased to 2." } ) ==
    "You craft a knife. Fabrication increased to 2." );
    CHECK( accessibility::format_activity_progress( "Waiting", "Waiting 20%", "1 hour" ) ==
           "Waiting 20%. About 1 hour remaining." );
    CHECK( accessibility::format_activity_start( "<color_red>Reading book</color>", "", false ) ==
           "Started: Reading book." );
    CHECK( accessibility::format_activity_result( "Reading book", false, true,
            "<color_red>Zombie spotted!</color>", {} ) == "Stopped: Reading book. Zombie spotted!" );
    CHECK( accessibility::format_activity_progress( "Waiting", "<color_white>Waiting 20%</color>",
            "" ) == "Waiting 20%." );
}

TEST_CASE( "semantic_accessibility_lists_have_stable_distance_ordering",
           "[accessibility][nogame]" )
{
    std::vector<accessibility::semantic_list_entry> entries = {
        { 2, tripoint( 2, 0, 0 ), 0, "far east", "2 tiles east" },
        { 1, tripoint::south, 1, "south item", "1 tile south" },
        { 1, tripoint::north, 2, "north obstacle", "1 tile north" },
        { 0, tripoint::zero, 0, "current position", "here" },
        { 1, tripoint::north, 1, "north hazard", "1 tile north" },
    };

    accessibility::sort_semantic_list_entries( entries );

    REQUIRE( entries.size() == 5 );
    CHECK( entries[0].label == "current position" );
    CHECK( entries[1].label == "north hazard" );
    CHECK( entries[2].label == "north obstacle" );
    CHECK( entries[3].label == "south item" );
    CHECK( entries[4].label == "far east" );
}

TEST_CASE( "semantic_accessibility_sequences_omit_empty_parts_and_preserve_order",
           "[accessibility][nogame]" )
{
    const std::vector<std::string> parts = {
        "Terrain: wooden floor",
        "",
        "  Creature: zombie.  ",
        "Z-level: one level above",
        "5 tiles northeast"
    };

    CHECK( accessibility::format_semantic_sequence( parts ) ==
           "Terrain: wooden floor. Creature: zombie. Z-level: one level above. 5 tiles northeast." );
}

TEST_CASE( "keybinding_focus_and_details_expose_scope_and_all_keys",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_keybinding_focus( "Accessibility details", "Alt+d", false ) ==
           "Accessibility details. Key: Alt+d. Global binding." );
    CHECK( accessibility::format_keybinding_focus( "Disassemble", "D", false ) ==
           "Disassemble. Key: Shift+d. Global binding." );
    CHECK( accessibility::format_keybinding_focus( "Accessibility details", "", true ) ==
           "Accessibility details. Unbound. Local binding." );
    CHECK( accessibility::format_keybinding_details( "Accessibility details",
    { "Alt+d", "F2" }, false, true ) ==
    "Accessibility details. Keys: Alt+d, F2. Global binding. Customized." );
    CHECK( accessibility::format_keybinding_details( "Craft",
    { "C", "&" }, false, false ) ==
    "Craft. Keys: Shift+c, &. Global binding. Default." );
}

TEST_CASE( "keybinding_open_and_filter_formatting", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_keybinding_open( "Up", "Down", "Tab", "Enter", "Alt+d", 42,
            "Move north. Key: Up. Global binding." ) ==
           "Keybindings. 42 actions. Up and Down choose an action. Tab switches to the filter. Enter opens actions. Alt+d reads all keys. Move north. Key: Up. Global binding." );
    CHECK( accessibility::format_keybinding_filter( "access", 2, "Tab", true ) ==
           "Filter. Type to search. Tab returns to the action list. Text: access. Results: 2." );
    CHECK( accessibility::format_keybinding_filter( "access", 2, "Tab", false ) ==
           "Text: access. Results: 2." );
    CHECK( accessibility::format_keybinding_filter( "", 150, "Tab", false ) ==
           "Text: empty. Results: 150." );
}

TEST_CASE( "format_spoken_key_distinguishes_uppercase_and_modifiers", "[accessibility][nogame]" )
{
    // Single uppercase letters become Shift+<lower>
    CHECK( accessibility::format_spoken_key( "Y" ) == "Shift+y" );
    CHECK( accessibility::format_spoken_key( "A" ) == "Shift+a" );
    CHECK( accessibility::format_spoken_key( "Z" ) == "Shift+z" );

    // Single lowercase letters remain unchanged
    CHECK( accessibility::format_spoken_key( "y" ) == "y" );
    CHECK( accessibility::format_spoken_key( "a" ) == "a" );

    // Modifiers combined with uppercase letters include Shift
    CHECK( accessibility::format_spoken_key( "ctrl+Y" ) == "Ctrl+Shift+y" );
    CHECK( accessibility::format_spoken_key( "CTRL+Y" ) == "Ctrl+Shift+y" );
    CHECK( accessibility::format_spoken_key( "CTRL-Y" ) == "Ctrl+Shift+y" );
    CHECK( accessibility::format_spoken_key( "alt+Y" ) == "Alt+Shift+y" );
    CHECK( accessibility::format_spoken_key( "ALT-Y" ) == "Alt+Shift+y" );
    CHECK( accessibility::format_spoken_key( "ctrl+alt+Y" ) == "Ctrl+Alt+Shift+y" );
    CHECK( accessibility::format_spoken_key( "CTRL-ALT-Y" ) == "Ctrl+Alt+Shift+y" );

    // Modifiers with lowercase letters do not add Shift
    CHECK( accessibility::format_spoken_key( "ctrl+y" ) == "Ctrl+y" );
    CHECK( accessibility::format_spoken_key( "CTRL-y" ) == "Ctrl+y" );
    CHECK( accessibility::format_spoken_key( "alt+d" ) == "Alt+d" );
    CHECK( accessibility::format_spoken_key( "ALT-d" ) == "Alt+d" );

    // Shift modifier normalization
    CHECK( accessibility::format_spoken_key( "SHIFT-y" ) == "Shift+y" );
    CHECK( accessibility::format_spoken_key( "Shift+y" ) == "Shift+y" );
    CHECK( accessibility::format_spoken_key( "SHIFT-Y" ) == "Shift+y" );
    CHECK( accessibility::format_spoken_key( "Shift+Y" ) == "Shift+y" );
    CHECK( accessibility::format_spoken_key( "CTRL-SHIFT-y" ) == "Ctrl+Shift+y" );
    CHECK( accessibility::format_spoken_key( "ctrl+shift+Y" ) == "Ctrl+Shift+y" );

    // Named keys with all-caps words remain intact without spurious Shift
    CHECK( accessibility::format_spoken_key( "UP" ) == "UP" );
    CHECK( accessibility::format_spoken_key( "DOWN" ) == "DOWN" );
    CHECK( accessibility::format_spoken_key( "LEFT" ) == "LEFT" );
    CHECK( accessibility::format_spoken_key( "RIGHT" ) == "RIGHT" );
    CHECK( accessibility::format_spoken_key( "ESC" ) == "ESC" );
    CHECK( accessibility::format_spoken_key( "TAB" ) == "TAB" );
    CHECK( accessibility::format_spoken_key( "ENTER" ) == "ENTER" );
    CHECK( accessibility::format_spoken_key( "HOME" ) == "HOME" );
    CHECK( accessibility::format_spoken_key( "END" ) == "END" );
    CHECK( accessibility::format_spoken_key( "PAGE_UP" ) == "PAGE_UP" );
    CHECK( accessibility::format_spoken_key( "PAGE_DOWN" ) == "PAGE_DOWN" );
    CHECK( accessibility::format_spoken_key( "KEYPAD_5" ) == "KEYPAD_5" );
    CHECK( accessibility::format_spoken_key( "F1" ) == "F1" );
    CHECK( accessibility::format_spoken_key( "F12" ) == "F12" );

    // Named keys with modifiers
    CHECK( accessibility::format_spoken_key( "CTRL-UP" ) == "Ctrl+UP" );
    CHECK( accessibility::format_spoken_key( "ALT-ENTER" ) == "Alt+ENTER" );
    CHECK( accessibility::format_spoken_key( "SHIFT-TAB" ) == "Shift+TAB" );

    // Digits and symbols
    CHECK( accessibility::format_spoken_key( "7" ) == "7" );
    CHECK( accessibility::format_spoken_key( "?" ) == "?" );
    CHECK( accessibility::format_spoken_key( "" ).empty() );

    // List of keys
    CHECK( accessibility::format_spoken_key( "Y, or 7" ) == "Shift+y, or 7" );
    CHECK( accessibility::format_spoken_key( "y, Y" ) == "y, Shift+y" );
}

TEST_CASE( "main_menu_documents_keep_independent_reading_positions", "[accessibility][nogame]" )
{
    accessibility::document_cursor_memory cursors( 2 );
    CHECK( cursors.position( 0 ) == 0 );
    CHECK( cursors.position( 1 ) == 0 );

    CHECK( cursors.move( 0, 5, 1 ) == 1 );
    CHECK( cursors.move( 0, 5, 1 ) == 2 );
    CHECK( cursors.move( 1, 3, 1 ) == 1 );
    CHECK( cursors.position( 0 ) == 2 );
    CHECK( cursors.position( 1 ) == 1 );
    CHECK( cursors.move( 0, 5, 1 ) == 3 );
    CHECK( cursors.position( 1 ) == 1 );

    cursors.reset();
    CHECK( cursors.position( 0 ) == 0 );
    CHECK( cursors.position( 1 ) == 0 );
}

TEST_CASE( "semantic_creature_status_descriptions_are_reusable", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_creature_attitude( "Hostile" ) == "Attitude: Hostile" );
    CHECK( accessibility::format_creature_attitude( "" ).empty() );
    CHECK( accessibility::format_creature_health( 100, 100 ) == "Health: uninjured" );
    CHECK( accessibility::format_creature_health( 80, 100 ) == "Health: lightly injured" );
    CHECK( accessibility::format_creature_health( 60, 100 ) == "Health: moderately injured" );
    CHECK( accessibility::format_creature_health( 30, 100 ) == "Health: heavily injured" );
    CHECK( accessibility::format_creature_health( 10, 100 ) == "Health: severely injured" );
    CHECK( accessibility::format_creature_health( 9, 100 ) == "Health: nearly dead" );
    CHECK( accessibility::format_creature_health( 0, 0 ).empty() );
}

TEST_CASE( "nearby_list_semantics_report_mode_focus_stacks_and_empty_results",
           "[accessibility][nogame]" )
{
    const accessibility::nearby_list_entry item_entry = {
        "aspirin", 6, 2, 3, "4 tiles northeast",
        { "Category: drugs", "At this location: 2", "High priority" }, 5, 9
    };

    CHECK( accessibility::format_nearby_list_context( "Nearby items", 9 ) ==
           "Nearby items. 9 results." );
    CHECK( accessibility::format_nearby_list_context( "Nearby items", 9,
            "Sorted by name" ) == "Nearby items. Sorted by name. 9 results." );
    CHECK( accessibility::format_nearby_list_context( "Nearby creatures", 0 ) ==
           "Nearby creatures. No results." );
    CHECK( accessibility::format_nearby_list_entry( item_entry ) ==
           "aspirin. Count: 6. 3 locations. 4 tiles northeast. Category: drugs. "
           "At this location: 2. High priority." );
    CHECK( accessibility::format_nearby_list_summary( "Nearby items", 9, item_entry ) ==
           "Nearby items. 9 results. 5 of 9. Location 2 of 3." );
    CHECK( accessibility::format_nearby_list_summary( "Nearby creatures", 0, {} ) ==
           "Nearby creatures. No results." );
}

TEST_CASE( "status_announcements_suppress_animated_progress_values",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::normalize_status_announcement(
               "  Select first point.\n", false ) == "Select first point." );
    CHECK( accessibility::normalize_status_announcement(
               "Searching…  10%\nCancel", true ) == "Searching… Cancel" );
    CHECK( accessibility::normalize_status_announcement(
               "Please wait as the map saves [12/50]", true ) ==
           "Please wait as the map saves" );
    CHECK( accessibility::normalize_status_announcement(
               "Compressing maps [1/20]", true ) ==
           accessibility::normalize_status_announcement(
               "Compressing maps [19/20]", true ) );
}

TEST_CASE( "semantic_accessibility_lists_group_connected_matching_regions",
           "[accessibility][nogame]" )
{
    std::vector<accessibility::semantic_list_entry> entries = {
        { 3, tripoint( 3, 0, 0 ), 1, "wall", "3 tiles east" },
        { 1, tripoint::east, 1, "wall", "1 tile east" },
        { 2, tripoint( 2, 1, 0 ), 1, "wall", "2 tiles southeast" },
        { 5, tripoint( -5, 0, 0 ), 1, "wall", "5 tiles west" },
        { 2, tripoint( 2, 0, 0 ), 2, "deep water", "2 tiles east" },
        { 3, tripoint( 3, 1, 0 ), 2, "deep water", "3 tiles southeast" },
        { 4, tripoint( 3, 1, 1 ), 1, "wall", "above" },
    };

    const std::vector<accessibility::connected_semantic_list_group> groups =
        accessibility::group_connected_semantic_list_entries( std::move( entries ) );

    REQUIRE( groups.size() == 4 );
    CHECK( groups[0].nearest.label == "wall" );
    CHECK( groups[0].nearest.context == "1 tile east" );
    CHECK( groups[0].count == 3 );
    CHECK( groups[1].nearest.label == "deep water" );
    CHECK( groups[1].count == 2 );
    CHECK( groups[2].nearest.context == "above" );
    CHECK( groups[2].count == 1 );
    CHECK( groups[3].nearest.context == "5 tiles west" );
    CHECK( groups[3].count == 1 );
}

TEST_CASE( "condition_transitions_detection", "[accessibility][nogame]" )
{
    accessibility::condition_tracker tracker;

    // Initial state: healthy and normal
    accessibility::condition_tracker initial_state;
    initial_state.hunger_tier = "Satisfied";
    initial_state.thirst_tier = "Slaked";
    initial_state.sleepiness_tier = "Rested";
    initial_state.pain_tier = "No Pain";
    initial_state.weariness_tier = "Fresh";
    initial_state.stamina_critical = false;

    // First turn: initialization should produce 0 alerts
    std::vector<std::string> alerts = accessibility::evaluate_condition_transitions(
                                          tracker, initial_state );
    CHECK( alerts.empty() );
    CHECK( tracker.initialized );

    // Same state on next turn: 0 alerts
    alerts = accessibility::evaluate_condition_transitions( tracker, initial_state );
    CHECK( alerts.empty() );

    // Hunger transitions to Hungry
    accessibility::condition_tracker hungry_state = initial_state;
    hungry_state.hunger_tier = "Hungry";
    alerts = accessibility::evaluate_condition_transitions( tracker, hungry_state );
    REQUIRE( alerts.size() == 1 );
    CHECK( alerts[0].find( "Hungry" ) != std::string::npos );

    // Next turn still hungry: 0 alerts
    alerts = accessibility::evaluate_condition_transitions( tracker, hungry_state );
    CHECK( alerts.empty() );

    // Bleeding starts on Left Arm
    accessibility::condition_tracker bleeding_state = hungry_state;
    bleeding_state.bleeding_limbs = { "Left Arm" };
    alerts = accessibility::evaluate_condition_transitions( tracker, bleeding_state );
    REQUIRE( alerts.size() == 1 );
    CHECK( alerts[0].find( "Bleeding" ) != std::string::npos );
    CHECK( alerts[0].find( "Left Arm" ) != std::string::npos );

    // Bleeding stops
    accessibility::condition_tracker healed_state = bleeding_state;
    healed_state.bleeding_limbs.clear();
    alerts = accessibility::evaluate_condition_transitions( tracker, healed_state );
    REQUIRE( alerts.size() == 1 );
    CHECK( alerts[0].find( "Stopped bleeding" ) != std::string::npos );
    CHECK( alerts[0].find( "Left Arm" ) != std::string::npos );

    // Broken limb occurs
    accessibility::condition_tracker broken_state = healed_state;
    broken_state.broken_limbs = { "Right Leg" };
    alerts = accessibility::evaluate_condition_transitions( tracker, broken_state );
    REQUIRE( alerts.size() == 1 );
    CHECK( alerts[0].find( "Broken limb" ) != std::string::npos );
    CHECK( alerts[0].find( "Right Leg" ) != std::string::npos );

    // Stamina critical drop occurs
    accessibility::condition_tracker exhausted_state = broken_state;
    exhausted_state.stamina_critical = true;
    alerts = accessibility::evaluate_condition_transitions( tracker, exhausted_state );
    REQUIRE( alerts.size() == 1 );
    CHECK( alerts[0].find( "Stamina critical" ) != std::string::npos );

    // Multiple transitions simultaneously
    accessibility::condition_tracker multi_state = exhausted_state;
    multi_state.pain_tier = "Severe Pain";
    multi_state.thirst_tier = "Dehydrated";
    alerts = accessibility::evaluate_condition_transitions( tracker, multi_state );
    REQUIRE( alerts.size() == 2 );
    CHECK( alerts[0].find( "Severe Pain" ) != std::string::npos );
    CHECK( alerts[1].find( "Dehydrated" ) != std::string::npos );

    // Global announce helper test with capture backend
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    // First call initializes
    CHECK_FALSE( accessibility::check_and_announce_condition_transitions( initial_state ) );
    CHECK( capture_ptr->messages.empty() );

    // Second call with change announces
    accessibility::condition_tracker changed = initial_state;
    changed.hunger_tier = "Starving";
    CHECK( accessibility::check_and_announce_condition_transitions( changed ) );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0].find( "Starving" ) != std::string::npos );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "accessible_status_summary_formats_semantic_state", "[accessibility][nogame]" )
{
    accessibility::status_snapshot healthy;
    healthy.health = "Feel Fine";
    healthy.pain = "No pain";
    healthy.stamina_percent = 100;
    healthy.focus = 100;
    healthy.weariness = "Fresh";
    healthy.hunger = "Satisfied";
    healthy.thirst = "Hydrated";
    healthy.sleepiness = "Rested";
    healthy.body_temperature = "Comfortable";
    healthy.environment_temperature = "21 C";
    healthy.wielded_weapon = "fists";
    healthy.movement_mode = "Walking";
    healthy.speed = 100;
    healthy.safe_mode = accessibility::format_safe_mode_status(
                            accessibility::safe_mode_state::on, true, 0 );
    healthy.time = "08:00";
    healthy.date = "Spring, day 1";
    healthy.weather = "Clear";

    const std::vector<accessibility::status_summary_entry> healthy_entries =
        accessibility::format_status_summary( healthy );
    REQUIRE( healthy_entries.size() == 7 );
    CHECK( healthy_entries[0].value == "Feel Fine, No pain" );
    CHECK( healthy_entries[3].value == "Body: Comfortable, Env: 21 C" );
    CHECK( healthy_entries[4].value == "fists" );
    CHECK( healthy_entries[5].value == "Mode: Walking, Speed: 100, Safe mode: On" );

    accessibility::status_snapshot snapshot;
    snapshot.health = "Feel Fine";
    snapshot.pain = "Moderate pain";
    snapshot.bleeding = true;
    snapshot.broken_limb = true;
    snapshot.stamina_percent = 24;
    snapshot.focus = 87;
    snapshot.weariness = "Weary";
    snapshot.hunger = "Hungry";
    snapshot.thirst = "Dehydrated";
    snapshot.sleepiness = "Tired";
    snapshot.body_temperature = "Chilly";
    snapshot.environment_temperature = "3 C";
    snapshot.active_effect_count = 2;
    snapshot.wielded_weapon = "Glock 19 (7/15)";
    snapshot.movement_mode = "Walking";
    snapshot.speed = 95;
    snapshot.safe_mode = accessibility::format_safe_mode_status(
                             accessibility::safe_mode_state::off, true, 12 );
    snapshot.time = "08:15";
    snapshot.date = "Spring, day 3";
    snapshot.weather = "Clear";

    const std::vector<accessibility::status_summary_entry> entries =
        accessibility::format_status_summary( snapshot );

    REQUIRE( entries.size() == 7 );
    CHECK( entries[0].label == "Health & Pain" );
    CHECK( entries[0].value ==
           "Feel Fine, Moderate pain, Bleeding!, Broken limb!" );
    CHECK( entries[1].value == "Stamina: 24%, Focus: 87, Weariness: Weary" );
    CHECK( entries[2].value == "Hunger: Hungry, Thirst: Dehydrated, Sleepiness: Tired" );
    CHECK( entries[3].value == "Body: Chilly, Env: 3 C, 2 effects" );
    CHECK( entries[4].value == "Glock 19 (7/15)" );
    CHECK( entries[5].value ==
           "Mode: Walking, Speed: 95, Safe mode: Off; auto-safe in 12 turns" );
    CHECK( entries[6].value == "08:15, Spring, day 3, Clear" );

    CHECK( accessibility::format_safe_mode_status(
               accessibility::safe_mode_state::on, true, 0 ) == "On" );
    CHECK( accessibility::format_safe_mode_status(
               accessibility::safe_mode_state::stopped, true, 0 ) == "Stopped by threat" );
    CHECK( accessibility::format_safe_mode_status(
               accessibility::safe_mode_state::off, false, 0 ) == "Off" );
    CHECK( accessibility::format_safe_mode_status(
               accessibility::safe_mode_state::off, true, 0 ) == "Off; auto-safe pending" );
    CHECK( accessibility::format_safe_mode_status(
               accessibility::safe_mode_state::off, true, 1 ) == "Off; auto-safe in 1 turn" );
}

TEST_CASE( "speech_text_scanners_match_the_regular_expressions_they_replace",
           "[accessibility][nogame]" )
{
    // The scanners replaced these expressions; they must remove the same text.
    const std::regex fraction( R"(\s*\[\s*\d+\s*/\s*\d+\s*\])" );
    const std::regex percentage( R"(\s*\d+(\.\d+)?\s*%)" );
    for( const std::string &progress : {
             "Saving [3/10]  25%", "Crafting [ 12 / 300 ]", "Reading 7.5 %", "Waiting 1.5.5%",
             "Unpacking [3/]", "Done [a/b] 100%%", "x[1/2][3/4]y", "Level 3.%", "  42%  ", "[1/2"
         } ) {
        CAPTURE( progress );
        const std::string expected = std::regex_replace( std::regex_replace( progress, fraction, "" ),
                                     percentage, "" );
        CHECK( accessibility::normalize_status_announcement( progress, true ) ==
               accessibility::normalize_status_announcement( expected, false ) );
    }
    const std::regex markup_tags( R"(<[^<>\s][^<>]*>)" );
    for( const std::string &text : {
             "<color_red>Hot</color> < 5 and <good>fine</good>", "a <b<c> d", "x <> y < z>",
             "<a\nb>c", "trailing <open", "<info>Weight: 1.5 kg</info>\n<bad>Broken.</bad>"
         } ) {
        CAPTURE( text );
        CHECK( accessibility::format_document_paragraphs( text ) ==
               accessibility::format_document_paragraphs( std::regex_replace( text, markup_tags, "" ) ) );
    }
}

TEST_CASE( "tutorial_document_preserves_instruction_lines_and_sentences",
           "[accessibility][nogame]" )
{
    const auto fragments = accessibility::format_document_paragraphs(
                               "Welcome!  Read this lesson at your own pace.\n\n"
                               "Movement Controls:\nNorth: <color_yellow>8</color>\nSouth: 2\n"
                               "A stack weighs 1.5 kg.  Keep its weight < 5 kg.\n\n"
                               "To continue, move east." );
    const std::vector<std::string> expected = {
        "Welcome!", "Read this lesson at your own pace.", "Movement Controls:",
        "North: 8", "South: 2", "A stack weighs 1.5 kg.", "Keep its weight < 5 kg.",
        "To continue, move east."
    };
    CHECK( fragments == expected );
    CHECK( accessibility::format_document_paragraphs( "\n  \n<color_red></color>" ).empty() );
    CHECK( accessibility::move_document_cursor( 0, fragments.size(), -1 ) == 0 );
    CHECK( accessibility::move_document_cursor( 0, fragments.size(), 3 ) == 3 );
    CHECK( accessibility::move_document_cursor( 3, fragments.size(), -3 ) == 0 );
    CHECK( accessibility::move_document_cursor( 3, fragments.size(), 100 ) == 7 );
}

TEST_CASE( "help_topic_and_document_formatting", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_help_menu_open( 23, "a", "Introduction", "Up", "Down",
            "Enter", "Escape" ) ==
           "Help.  23 topics.  Use Up and Down to browse, hotkey or Enter to open, Escape to exit.  a: Introduction" );
    CHECK( accessibility::format_help_menu_open( 0, "", "", "Up", "Down", "Enter",
            "Escape" ) ==
           "Help.  0 topics.  Escape to exit." );

    CHECK( accessibility::format_help_topic_focus( "b", "Movement" ) == "b: Movement" );
    CHECK( accessibility::format_help_topic_focus( "", "Movement" ) == "Movement" );
    CHECK( accessibility::format_help_topic_focus( "b", "" ) == "b" );

    CHECK( accessibility::format_document_focus( "Movement", "First paragraph" ) ==
           "Movement. First paragraph" );
    CHECK( accessibility::format_document_focus( "", "First paragraph" ) == "First paragraph" );
    CHECK( accessibility::format_document_focus( "Help", "b: Movement" ) == "Help. b: Movement" );

    const std::vector<std::pair<std::string, std::vector<std::string>>> bindings = {
        { "Northwest", { "7", "y" } },
        { "North", { "8", "k" } },
        { "Northeast", { "9", "u" } },
        { "West", { "4", "h" } },
        { "Pause", { "5", "." } },
        { "East", { "6", "l" } },
        { "Southwest", { "1", "b" } },
        { "South", { "2", "j" } },
        { "Southeast", { "3", "n" } }
    };
    CHECK( accessibility::format_help_directions( bindings ) ==
           "Movement keys: Northwest: 7, y; North: 8, k; Northeast: 9, u; West: 4, h; Pause: 5, .; East: 6, l; Southwest: 1, b; South: 2, j; Southeast: 3, n." );

    const std::vector<std::pair<std::string, std::vector<std::string>>> unbound = {
        { "Northwest", {} }
    };
    CHECK( accessibility::format_help_directions( unbound ) ==
           "Movement keys: Northwest: unbound." );
    CHECK( accessibility::format_help_directions( {} ).empty() );

    const std::string dir_speech = help::get_dir_grid_speech();
    CHECK( dir_speech.find( "North: " ) != std::string::npos );
    CHECK( dir_speech.find( "UP" ) != std::string::npos );
    CHECK( dir_speech.find( "DOWN" ) != std::string::npos );
    CHECK( dir_speech.find( "LEFT" ) != std::string::npos );
    CHECK( dir_speech.find( "RIGHT" ) != std::string::npos );
}

TEST_CASE( "format_item_info_paragraphs_splits_stats_and_sentences", "[accessibility][nogame]" )
{
    // Test 1: Empty raw info with type name
    {
        const auto paras = accessibility::format_item_info_paragraphs( "WEAPON", "" );
        REQUIRE( paras.size() == 1 );
        CHECK( paras[0] == "WEAPON" );
    }

    // Test 2: Empty raw info without type name
    {
        const auto paras = accessibility::format_item_info_paragraphs( "", "" );
        CHECK( paras.empty() );
    }

    // Test 3: Color tags, stat lines, and prose description with multiple sentences
    {
        const std::string raw =
            "<color_c_light_gray>Volume: </color><color_c_white>0.25 L</color>\n"
            "<color_c_light_gray>Weight: </color><color_c_white>0.45 kg</color>\n"
            "<color_c_light_gray>Bash: </color><color_c_white>4</color>  "
            "<color_c_light_gray>Cut: </color><color_c_white>18</color>\n"
            "\n"
            "<color_c_white>A combat knife with an 8-inch high carbon steel blade.  "
            "Designed for close combat and utility.  Handle has a rubberized grip.</color>\n";

        const auto paras = accessibility::format_item_info_paragraphs( "KNIFE", raw );
        REQUIRE( paras.size() == 7 );
        CHECK( paras[0] == "KNIFE" );
        CHECK( paras[1] == "Volume: 0.25 L" );
        CHECK( paras[2] == "Weight: 0.45 kg" );
        CHECK( paras[3] == "Bash: 4  Cut: 18" );
        CHECK( paras[4] == "A combat knife with an 8-inch high carbon steel blade." );
        CHECK( paras[5] == "Designed for close combat and utility." );
        CHECK( paras[6] == "Handle has a rubberized grip." );
    }

    // Test 4: Single sentence description without type name
    {
        const std::string raw =
            "Damage: 10\n"
            "A sturdy stick.\n";

        const auto paras = accessibility::format_item_info_paragraphs( "", raw );
        REQUIRE( paras.size() == 2 );
        CHECK( paras[0] == "Damage: 10" );
        CHECK( paras[1] == "A sturdy stick." );
    }

    // Test 5: Type name with color tags
    {
        const auto paras = accessibility::format_item_info_paragraphs( "<color_c_green>TOOL</color>", "" );
        REQUIRE( paras.size() == 1 );
        CHECK( paras[0] == "TOOL" );
    }
}

TEST_CASE( "inventory_focus_and_comparison_preserve_selection_semantics",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    accessibility::inventory_focus_entry first;
    first.caption = "2 flashlights";
    first.category = "Tools";
    first.location = "backpack";
    first.cells = { { "Weight", "0.4 kg" }, { "Volume", "0.5 L" }, { "Charges", "20" } };
    first.favorite = true;
    first.has_contents = true;
    first.contents_collapsed = true;

    const std::string initial = accessibility::format_inventory_focus( first, true, false );
    CHECK( initial == "Tools. 2 flashlights. Location: backpack. Favorite. Contents collapsed. "
           "Weight: 0.4 kg. Volume: 0.5 L." );
    CHECK( accessibility::format_inventory_focus( first, false, true ).find( "Charges: 20" ) !=
           std::string::npos );
    first.denial = "Too heavy";
    CHECK( accessibility::format_inventory_focus( first, false, false ).find(
               "Unavailable: Too heavy" ) != std::string::npos );
    first.denial.clear();

    first.selection = "Selected as first comparison item";
    CHECK( accessibility::announce( accessibility::format_inventory_focus( first, false, false ) ) );

    accessibility::inventory_focus_entry candidate;
    candidate.caption = "hammer";
    candidate.category = "Tools";
    candidate.location = "inventory";
    CHECK( accessibility::announce( accessibility::format_inventory_focus( candidate, false,
                                    false ) ) );
    CHECK( accessibility::announce( accessibility::format_inventory_focus( first, false, false ) ) );

    first.selection.clear();
    CHECK( accessibility::announce( accessibility::format_inventory_focus( first, false, false ) ) );
    CHECK( accessibility::announce( accessibility::format_inventory_comparison_choice(
                                        "flashlight", "hammer" ) ) );
    CHECK( accessibility::repeat_last_announcement() );

    CHECK( accessibility::format_inventory_filter( "ham", 1 ) ==
           "Filter: ham. 1 matching item." );
    CHECK( accessibility::format_inventory_filter( "", 0 ) ==
           "Filter cleared. 0 matching items." );
    CHECK( accessibility::format_inventory_comparison_stat( "Weight", "flashlight", "0.4 kg",
            "hammer", "1.2 kg",
            accessibility::inventory_comparison_relation::first_better ) ==
           "Weight. First, flashlight: 0.4 kg. Second, hammer: 1.2 kg. First is better." );
    CHECK( accessibility::format_inventory_comparison_stat( "Damage", "flashlight", "6",
            "hammer", "8",
            accessibility::inventory_comparison_relation::second_better ) ==
           "Damage. First, flashlight: 6. Second, hammer: 8. Second is better." );
    CHECK( accessibility::format_inventory_comparison_stat( "Volume", "flashlight", "1 L",
            "hammer", "1 L",
            accessibility::inventory_comparison_relation::equal ) ==
           "Volume. First, flashlight: 1 L. Second, hammer: 1 L. Equal." );
    CHECK( accessibility::compare_inventory_values( 0.4, 1.2, true ) ==
           accessibility::inventory_comparison_relation::first_better );
    CHECK( accessibility::compare_inventory_values( 6.0, 8.0, false ) ==
           accessibility::inventory_comparison_relation::second_better );
    CHECK( accessibility::compare_inventory_values( 5.0, 5.005, false ) ==
           accessibility::inventory_comparison_relation::equal );

    REQUIRE( capture_ptr->messages.size() == 6 );
    CHECK( capture_ptr->messages[0].find( "Selected as first comparison item" ) !=
           std::string::npos );
    CHECK( capture_ptr->messages[1].find( "comparison item" ) == std::string::npos );
    CHECK( capture_ptr->messages[2].find( "Selected as first comparison item" ) !=
           std::string::npos );
    CHECK( capture_ptr->messages[3].find( "comparison item" ) == std::string::npos );
    CHECK( capture_ptr->messages[4] ==
           "Second comparison item: hammer. First comparison item: flashlight" );
    CHECK( capture_ptr->messages[5] == capture_ptr->messages[4] );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "visual_hints_speak_fields_without_separator_symbols", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_visual_hint(
               "Food: Full \xe2\x94\x82 Drink: Thirsty \xe2\x94\x82 \nConsumed today (kcal): moderate " ) ==
           "Food: Full. Drink: Thirsty. Consumed today (kcal): moderate." );
    CHECK( accessibility::format_visual_hint( "Item hotkeys assigned: 3/52" ) ==
           "Item hotkeys assigned: 3/52." );
    CHECK( accessibility::format_visual_hint( "" ).empty() );
}

TEST_CASE( "semantic_sequences_respect_existing_sentence_ends", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_semantic_sequence( { "Loading the save…", "Please wait" } ) ==
           "Loading the save… Please wait." );
    CHECK( accessibility::format_semantic_sequence( { "  Wrapped\n  text\t here  " } ) ==
           "Wrapped text here." );
    CHECK( accessibility::normalize_status_announcement( "Saving [3/10]  25%", true ) == "Saving" );
    CHECK( accessibility::format_threat_announcement( {}, 0 ) == "No threats in view" );
    CHECK( accessibility::format_threat_description( "zombie", 1, "north" ) ==
           "zombie 1 tile to the north" );
    CHECK( accessibility::format_threat_announcement( { "zombie 2 tiles to the north" }, 2 ) ==
           "zombie 2 tiles to the north and 2 others spotted!" );
    CHECK( accessibility::format_game_arrival( true, "Ana", "field" ) ==
           "Welcome back, Ana. Location: field" );
}

TEST_CASE( "transfer_and_selection_formatters_speak_semantic_state", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_inventory_selection_state( 0, 3, true ) == "Not selected" );
    CHECK( accessibility::format_inventory_selection_state( 0, 3, false ).empty() );
    CHECK( accessibility::format_inventory_selection_state( 2, 3, true ) == "Selected 2 of 3" );
    CHECK( accessibility::format_inventory_selection_state( 3, 3, true ) == "Selected all 3" );
    CHECK( accessibility::format_inventory_selection_state( 1, 1, true ) == "Selected" );
    CHECK( accessibility::format_inventory_selection_change( "rock", "Selected 2 of 3", 1 ) ==
           "rock. Selected 2 of 3. 1 entry selected." );
    CHECK( accessibility::format_inventory_selection_total( 0 ) == "Nothing selected" );

    accessibility::transfer_row row;
    row.name = "rock";
    row.count = 3;
    row.weight = "1.50 kg";
    row.volume = "0.75 L";
    row.category = "OTHER";
    row.source_area = "North";
    row.not_owned = true;
    row.auto_pickup = true;
    CHECK( accessibility::format_transfer_row( row, false, false ) ==
           "rock, 3. Not owned. 1.50 kg. 0.75 L." );
    CHECK( accessibility::format_transfer_row( row, true, false ) ==
           "OTHER. rock, 3. Not owned. 1.50 kg. 0.75 L." );
    CHECK( accessibility::format_transfer_row( row, false, true ) ==
           "OTHER. rock, 3. Not owned. 1.50 kg. 0.75 L. From: North. Auto pickup." );

    accessibility::transfer_pane source;
    source.area = "North, dirt";
    source.item_count = 2;
    source.capacity = "Weight 3.0 kg, volume 1.5 L of 1000 L";
    source.filter = "rock";
    source.sort = "none";
    accessibility::transfer_pane destination;
    destination.area = "Inventory";
    destination.item_count = 1;
    destination.capacity = "Weight 1.0 kg of 40.0 kg, volume 0.5 L of 20 L";
    CHECK( accessibility::format_transfer_pane( source, true, false ) ==
           "Source: North, dirt. 2 items." );
    CHECK( accessibility::format_transfer_pane( destination, false, true ) ==
           "Destination: Inventory. 1 item. Weight 1.0 kg of 40.0 kg, volume 0.5 L of 20 L." );
    CHECK( accessibility::format_transfer_status( source, destination ) ==
           "Source: North, dirt. 2 items. Weight 3.0 kg, volume 1.5 L of 1000 L. Filter: rock. "
           "Sort: none. Destination: Inventory. 1 item. "
           "Weight 1.0 kg of 40.0 kg, volume 0.5 L of 20 L." );

    CHECK( accessibility::format_transfer_result( "rock", 1, 1, "North" ) ==
           "Moved rock to North." );
    CHECK( accessibility::format_transfer_result( "rocks", 3, 3, "North" ) ==
           "Moved rocks, 3, to North." );
    CHECK( accessibility::format_transfer_result( "rocks", 2, 3, "North" ) ==
           "Moved 2 of 3 rocks to North." );
    CHECK( accessibility::format_transfer_result( "", 4, 4, "Inventory" ) ==
           "Moved 4 stacks to Inventory." );
    CHECK( accessibility::format_transfer_result( "", 1, 4, "Inventory" ) ==
           "Moved 1 of 4 stacks to Inventory." );
}

TEST_CASE( "direction_prompt_speaks_grouped_targets_and_outcomes", "[accessibility][nogame]" )
{
    const std::vector<accessibility::direction_target> targets = {
        { "wooden door", "north" }, { "window", "east" }, { "window", "northeast" },
        { "table with 2 items", "here" }
    };
    CHECK( accessibility::format_direction_prompt( "Close where?", targets,
            "(Direction button or mouse)" ) ==
           "Close where? Targets: wooden door, north; window, east, northeast; "
           "table with 2 items, here. (Direction button or mouse)" );
    CHECK( accessibility::format_direction_prompt( "Open where?", {}, "(Direction button)" ) ==
           "Open where? No valid targets. (Direction button)" );
    CHECK( accessibility::format_direction_auto_selected( { "wooden door", "north" } ) ==
           "Auto-selected: wooden door, north." );
    CHECK( accessibility::format_direction_invalid( { "brick wall", "west" } ) ==
           "brick wall, west: not a valid target." );
}

TEST_CASE( "travel_start_summarizes_merged_route_segments", "[accessibility][nogame]" )
{
    const std::vector<accessibility::travel_segment> segments =
        accessibility::merge_travel_segments( { "grass", "grass", "dirt road", "grass" } );
    REQUIRE( segments.size() == 3 );
    CHECK( segments[0].name == "grass" );
    CHECK( segments[0].count == 2 );
    CHECK( segments[2].count == 1 );

    CHECK( accessibility::format_travel_start( "wooden door", "12 tiles northeast", 14,
    { { "grass", 6 }, { "dirt road", 5 }, { "wooden floor", 3 } } ) ==
    "Traveling to wooden door, 12 tiles northeast, 14 steps: grass 6, dirt road 5, "
    "wooden floor 3." );
    CHECK( accessibility::format_travel_start( "rock", "1 tile east", 1, { { "grass", 1 } } ) ==
    "Traveling to rock, 1 tile east, 1 step: grass 1." );
    CHECK( accessibility::format_travel_start( "evac shelter", "12 map tiles south", 0,
    { { "forest", 4 }, { "road", 8 } } ) ==
    "Traveling to evac shelter, 12 map tiles south: forest 4, road 8." );
    CHECK( accessibility::format_travel_start( "field", "2 map tiles west", 0,
    { { "a", 1 }, { "b", 1 }, { "c", 1 }, { "d", 1 }, { "e", 1 }, { "f", 1 } } ) ==
    "Traveling to field, 2 map tiles west: a 1, b 1, c 1, d 1, 2 more." );
    CHECK( accessibility::format_travel_arrival( "grass" ) == "Arrived: grass." );
    CHECK( accessibility::format_travel_stopped( "wooden door", "8 tiles north" ) ==
           "Travel stopped: wooden door, 8 tiles north." );
}

TEST_CASE( "pickup_and_drop_formatters_speak_capacity_ownership_and_result",
           "[accessibility][nogame]" )
{
    accessibility::inventory_capacity capacity;
    capacity.weight = "12.3";
    capacity.weight_limit = "40.0";
    capacity.weight_unit = "kg";
    capacity.volume = "8.10";
    capacity.volume_limit = "10.00";
    capacity.volume_unit = "L";
    CHECK( accessibility::format_inventory_capacity( capacity, true ) ==
           "Weight 12.3 of 40.0 kg, volume 8.10 of 10.00 L." );
    CHECK( accessibility::format_inventory_capacity( capacity, false ).empty() );
    capacity.over_volume = true;
    CHECK( accessibility::format_inventory_capacity( capacity, true ) ==
           "Weight 12.3 of 40.0 kg, volume 8.10 of 10.00 L. Over volume limit." );
    capacity.over_weight = true;
    CHECK( accessibility::format_inventory_capacity( capacity, false ) ==
           "Over weight limit. Over volume limit." );

    accessibility::inventory_focus_entry entry;
    entry.caption = "rock";
    entry.selection = "Not selected";
    entry.not_owned = true;
    entry.owner = "The Free Merchants";
    entry.destination = "backpack";
    CHECK( accessibility::format_inventory_focus( entry, false, false ) ==
           "rock. Not selected. Not owned." );
    CHECK( accessibility::format_inventory_focus( entry, true, true ) ==
           "rock. Not selected. Not owned. Owner: The Free Merchants. Goes into: backpack." );

    CHECK( accessibility::format_pickup_result( {}, {} ).empty() );
    CHECK( accessibility::format_pickup_result( { { "3 apples", "backpack" }, { "knife", "sheath" } },
    { { "rock", "too heavy" } } ) ==
    "Picked up: 3 apples into backpack, knife into sheath. Not picked up: rock, too heavy." );
    CHECK( accessibility::format_pickup_result( { { "nails (20)", "" } }, {} ) ==
    "Picked up: nails (20)." );
    CHECK( accessibility::format_pickup_result( {}, { { "rock", "no room" }, { "pipe", "no room" } } )
    ==
    "Not picked up: rock, no room; pipe, no room." );

    CHECK( accessibility::format_drop_result( "grass", { "3 apples", "knife" }, true ) ==
           "Dropped on grass: 3 apples, knife." );
    CHECK( accessibility::format_drop_result( "dresser", { "rock", "hammer" }, false ) ==
           "Put in dresser: rock, hammer." );
    CHECK( accessibility::format_drop_result( "grass", {}, true ).empty() );
}

TEST_CASE( "targeting_formatters_speak_target_problems_and_aim_levels",
           "[accessibility][nogame]" )
{
    accessibility::target_focus focus;
    focus.target = "zombie";
    focus.attitude = "Hostile";
    focus.location = "5 tiles northeast";
    focus.tile = { "Terrain: grass", "Creature: zombie", "5 tiles northeast" };
    focus.hit = accessibility::format_target_hit_chance( 62 );
    CHECK( accessibility::format_target_focus( focus, false ) ==
           "zombie, Hostile, 5 tiles northeast, 62% hit." );
    CHECK( accessibility::format_target_focus( focus, true ) ==
           "Terrain: grass. Creature: zombie. 5 tiles northeast. 62% hit." );
    CHECK( accessibility::format_target_problems( focus ).empty() );

    focus.hit.clear();
    focus.out_of_range = true;
    focus.blocked_by = "brick wall";
    focus.in_line_of_fire = { "dog", "cat" };
    CHECK( accessibility::format_target_focus( focus, false ) ==
           "zombie, Hostile, 5 tiles northeast. Out of range. No line of fire, blocked by brick wall. "
           "In line of fire: dog, cat." );

    accessibility::target_focus self;
    self.tile = { "Terrain: grass", "here" };
    self.invalid = true;
    self.in_area_of_effect = true;
    CHECK( accessibility::format_target_focus( self, false ) ==
           "Terrain: grass. here. Not a valid target. You are in the area of effect." );
    accessibility::target_focus empty_gun;
    empty_gun.out_of_ammo = true;
    CHECK( accessibility::format_target_problems( empty_gun ) == "Out of ammo." );

    CHECK( accessibility::format_target_count( 0 ) == "No targets" );
    CHECK( accessibility::format_target_count( 1 ) == "1 target" );
    CHECK( accessibility::format_target_ammo( "9mm", 8, 15 ) == "Ammo: 9mm, 8 of 15" );
    CHECK( accessibility::format_target_ammo( "9mm", 0, 15 ) == "Ammo: 9mm" );
    CHECK( accessibility::format_target_open( "Firing pistol",
            accessibility::format_target_ammo( "9mm", 8, 15 ), 2,
            "zombie, Hostile, 5 tiles northeast, 62% hit." ) ==
           "Firing pistol. Ammo: 9mm, 8 of 15. 2 targets. zombie, Hostile, 5 tiles northeast, 62% hit." );
    CHECK( accessibility::format_target_open( "Throwing rock", "", 0, "Terrain: grass. here." ) ==
           "Throwing rock. No targets. Terrain: grass. here." );

    accessibility::target_status status;
    status.firing_mode = "semi-auto (1)";
    status.aim_mode = "Careful";
    status.ammo = "Ammo: 9mm, 8 of 15";
    status.recoil = "Low";
    status.steadiness = 45;
    status.distance = 5;
    status.range = 30;
    CHECK( accessibility::format_target_status( status ) ==
           "Firing mode: semi-auto (1). Aim: Careful. Ammo: 9mm, 8 of 15. Recoil: Low. "
           "Steadiness: 45%. Range: 5 of 30." );
    accessibility::target_status throwing;
    throwing.range = 10;
    CHECK( accessibility::format_target_status( throwing ) == "Range: 10." );

    accessibility::target_aim_level level;
    level.name = "Careful";
    level.key = "c";
    level.selected = true;
    level.moves = 120;
    level.hit_chance = 62;
    level.outcomes = { { "Great", 10 }, { "Normal", 30 }, { "Graze", 22 }, { "Miss", 38 } };
    CHECK( accessibility::format_target_aim_level( level ) ==
           "Careful aim, selected, key c: 62% hit. Great 10%, Normal 30%, Graze 22%, Miss 38%. "
           "120 moves to fire." );
    level.selected = false;
    level.key.clear();
    level.moves = 1;
    CHECK( accessibility::format_target_aim_level( level ) ==
           "Careful aim: 62% hit. Great 10%, Normal 30%, Graze 22%, Miss 38%. 1 move to fire." );
}

TEST_CASE( "crafting_formatters_speak_requirements_and_recipe_rows", "[accessibility][nogame]" )
{
    using accessibility::requirement_state;
    accessibility::requirement_group tools;
    tools.kind = accessibility::requirement_kind::tools;
    tools.options = { { "hammer", requirement_state::have }, { "rock", requirement_state::missing } };
    CHECK( accessibility::format_requirement_group( tools ) ==
           "Tools: hammer, have; or rock, missing." );
    CHECK( accessibility::requirement_group_met( tools ) );

    accessibility::requirement_group components;
    components.kind = accessibility::requirement_kind::components;
    components.options = {
        { "2 rags (have 1)", requirement_state::missing },
        { "1 thread", requirement_state::not_enough }
    };
    CHECK( accessibility::format_requirement_group( components ) ==
           "Components: 2 rags (have 1), missing; or 1 thread, not enough." );
    CHECK_FALSE( accessibility::requirement_group_met( components ) );
    components.options.push_back( { "1 duct tape", requirement_state::uses_favorite } );
    CHECK( accessibility::requirement_group_met( components ) );
    CHECK( accessibility::format_requirement_state( requirement_state::uses_filled_container ) ==
           "have, uses a filled container" );
    CHECK( accessibility::format_requirement_group( accessibility::requirement_group() ) ==
           "Tools: none." );

    accessibility::requirement_group unmet = tools;
    unmet.options = { { "saw", requirement_state::missing } };
    CHECK( accessibility::count_missing_requirement_groups( { tools, unmet, unmet } ) == 2 );

    CHECK( accessibility::format_recipe_availability( true, 3 ) == "craftable" );
    CHECK( accessibility::format_recipe_availability( false, 2 ) == "2 missing" );
    CHECK( accessibility::format_recipe_availability( false, 0 ) == "not craftable" );

    accessibility::recipe_row row;
    row.name = "knife";
    row.missing = 2;
    CHECK( accessibility::format_recipe_row( row ) == "knife, 2 missing." );
    row.craftable = true;
    row.favorite = true;
    row.unread = true;
    row.warnings = { "low skill" };
    CHECK( accessibility::format_recipe_row( row ) == "knife, craftable, low skill, favorite, new." );
    accessibility::recipe_row group;
    group.name = "arrows";
    group.group = true;
    CHECK( accessibility::format_recipe_row( group ) ==
           "arrows, group, collapsed, nothing craftable." );
    group.expanded = true;
    group.craftable = true;
    CHECK( accessibility::format_recipe_row( group ) == "arrows, group, expanded, craftable." );

    CHECK( accessibility::format_recipe_count( 0 ) == "No recipes" );
    CHECK( accessibility::format_recipe_count( 1 ) == "1 recipe" );
    CHECK( accessibility::format_recipe_list( "WEAPON", 12, 2, true ) ==
           "WEAPON, 12 recipes, 2 hidden, new recipes" );
    CHECK( accessibility::format_recipe_list( "BASHING", 0, 0, false ) == "BASHING, No recipes" );
    CHECK( accessibility::format_recipe_batch( 3, "30 minutes", "craftable" ) ==
           "Batch size 3, 30 minutes, craftable." );
    CHECK( accessibility::format_recipe_batch( 2, "", "" ) == "Batch size 2." );
}

TEST_CASE( "construction_catalog_speaks_availability_and_variants", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_construction_list( "Walls", 0 ) == "Walls, No projects" );
    CHECK( accessibility::format_construction_list( "Walls", 1 ) == "Walls, 1 project" );
    CHECK( accessibility::format_construction_list( "Walls", 3 ) == "Walls, 3 projects" );
    CHECK( accessibility::format_construction_row( "Board up window", true, "", 1 ) ==
           "Board up window, available." );
    CHECK( accessibility::format_construction_row( "Dig pit", false,
            "no valid adjacent tile", 2 ) ==
           "Dig pit, Unavailable: no valid adjacent tile, 2 variants." );
}

TEST_CASE( "character_sheet_formatters_speak_values_shown_by_color", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_character_stat( "Strength", 10, 8 ) == "Strength 10, base 8." );
    CHECK( accessibility::format_character_value( "Weight", "<color_c_green>Normal</color>" ) ==
           "Weight, Normal." );

    accessibility::encumbrance_row torso;
    torso.part = "Torso";
    torso.encumbrance = 12;
    torso.layering = 3;
    torso.warmth = 50;
    torso.warmth_state = accessibility::body_warmth::comfortable;
    CHECK( accessibility::format_encumbrance_row( torso, false ) ==
           "Torso, encumbrance 12, layering 3, warmth comfortable." );
    CHECK( accessibility::format_encumbrance_row( torso, true ) ==
           "Torso, encumbrance 12, layering 3, total 15, warmth 50, comfortable." );
    accessibility::encumbrance_row eyes;
    eyes.part = "Eyes";
    eyes.warmth = 50;
    CHECK( accessibility::format_encumbrance_row( eyes, false ) == "Eyes, encumbrance 0." );
    CHECK( accessibility::format_encumbrance_row( eyes, true ) ==
           "Eyes, encumbrance 0, layering 0, total 0, warmth 50." );
    CHECK( accessibility::format_body_warmth( accessibility::body_warmth::very_cold ) ==
           "very cold" );

    CHECK( accessibility::format_speed_row( "Pain", true, -10, false ) == "Pain, speed -10." );
    CHECK( accessibility::format_speed_row( "Encumbrance", false, 20, true ) ==
           "Encumbrance, move cost +20%." );
    CHECK( accessibility::format_speed_row( "Quick", true, 0, false ) == "Quick." );

    accessibility::skill_row melee;
    melee.name = "Melee";
    melee.level = 3;
    melee.exercise = 45;
    CHECK( accessibility::format_skill_row( melee ) == "Melee 3, 45%, training." );
    melee.rusty = true;
    melee.training = false;
    CHECK( accessibility::format_skill_row( melee ) == "Melee 3, 45%, rusty, not training." );
    melee.set_by_bionic = true;
    melee.level = 5;
    CHECK( accessibility::format_skill_row( melee ) == "Melee 5, set by bionic." );
    accessibility::skill_row dodge;
    dodge.name = "Dodge";
    dodge.level = 2;
    dodge.exercise = -1;
    dodge.effective = "4.5";
    CHECK( accessibility::format_skill_row( dodge ) == "Dodge 2, 0%, effective 4.5, training." );

    CHECK( accessibility::format_trait_row( "Good Hearing", accessibility::trait_kind::positive ) ==
           "Good Hearing, positive." );
    CHECK( accessibility::format_trait_row( "Survivor", accessibility::trait_kind::profession ) ==
           "Survivor, profession." );
    CHECK( accessibility::format_bionic_row( "Power Storage", 1 ) == "Power Storage." );
    CHECK( accessibility::format_bionic_row( "Power Storage", 3 ) == "Power Storage, 3 installed." );
    CHECK( accessibility::format_proficiency_row( "Basic Knapping", true, false, 0 ) ==
           "Basic Knapping, known." );
    CHECK( accessibility::format_proficiency_row( "Basic Knapping", false, true, 45 ) ==
           "Basic Knapping, 45%, learning." );
    CHECK( accessibility::format_proficiency_row( "Secret", false, false, 0 ) ==
           "Secret, not known." );

    CHECK( accessibility::format_character_tab( "Stats", {}, 9, "Strength 10, base 8." ) ==
           "Stats. 9 items. Strength 10, base 8." );
    CHECK( accessibility::format_character_tab( "Speed", { "Speed 90", "Move cost 110" }, 1,
            "Pain, speed -10." ) == "Speed. Speed 90. Move cost 110. 1 item. Pain, speed -10." );
    CHECK( accessibility::format_character_tab( "Effects", {}, 0, "" ) == "Effects. No items." );
}

TEST_CASE( "bionic_and_mutation_formatters_speak_state_cost_and_unavailable_reason",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_power_menu_mode( accessibility::power_menu_mode::activating ) ==
           "Activating." );
    CHECK( accessibility::format_power_menu_mode( accessibility::power_menu_mode::examining ) ==
           "Examining." );
    CHECK( accessibility::format_power_menu_mode( accessibility::power_menu_mode::reassigning ) ==
           "Reassigning. Select an entry to assign a new letter." );
    CHECK( accessibility::format_power_menu_mode( accessibility::power_menu_mode::hiding ) ==
           "Hiding. Select an entry to hide or show its sprite." );
    CHECK( accessibility::format_assigned_key( "a" ) == "Key a" );
    CHECK( accessibility::format_assigned_key( "A" ) == "Key Shift+a" );
    CHECK( accessibility::format_assigned_key( " " ) == "No key" );

    accessibility::bionic_power_row night_vision;
    night_vision.name = "Night vision";
    night_vision.toggled = true;
    night_vision.powered = true;
    night_vision.over_time = "5 J";
    CHECK( accessibility::format_bionic_power_row( night_vision ) ==
           "Night vision, on, 5 J per turn." );
    night_vision.powered = false;
    night_vision.activate = "10 J";
    night_vision.charge_turns = 3;
    night_vision.not_enough_power = true;
    CHECK( accessibility::format_bionic_power_row( night_vision ) ==
           "Night vision, off, 10 J to activate, 5 J per 3 turns. Unavailable: not enough power." );

    accessibility::bionic_power_row reactor;
    reactor.name = "<color_c_red>Reactor</color>";
    reactor.power_source = true;
    reactor.deactivate = "1 kJ";
    reactor.trigger = "2 kJ";
    reactor.incapacitated = true;
    reactor.sprite_hidden = true;
    reactor.fuel_saving = 50;
    reactor.fuel_saving_inactive = true;
    CHECK( accessibility::format_bionic_power_row( reactor ) ==
           "Reactor, power source, 1 kJ to deactivate, 2 kJ per trigger, incapacitated, "
           "sprite hidden, fuel saving above 50%, fuel saving inactive." );

    CHECK( accessibility::format_bionic_power( "50.5 kJ", "100 kJ", {} ) ==
           "Bionic power 50.5 kJ of 100 kJ." );
    CHECK( accessibility::format_bionic_power( "5 J", "100 kJ", { "battery 5", "gasoline 10" } ) ==
           "Bionic power 5 J of 100 kJ. Available fuel: battery 5, gasoline 10." );

    accessibility::mutation_power_row photophore;
    photophore.name = "Photophore";
    photophore.activated = true;
    photophore.cost = 9;
    photophore.resources = { "kcal" };
    photophore.cooldown = "5 minutes";
    CHECK( accessibility::format_mutation_power_row( photophore ) ==
           "Photophore, off, 9 kcal, cooldown 5 minutes." );
    photophore.powered = true;
    CHECK( accessibility::format_mutation_power_row( photophore ) ==
           "Photophore, on, 9 kcal, cooldown 5 minutes." );
    photophore.powered = false;
    photophore.resources = { "kcal", "thirst" };
    photophore.insufficient = true;
    CHECK( accessibility::format_mutation_power_row( photophore ) ==
           "Photophore, off, 9 kcal and thirst, cooldown 5 minutes. "
           "Unavailable: not enough kcal and thirst." );

    accessibility::mutation_power_row hearing;
    hearing.name = "Good Hearing";
    hearing.starting_trait = true;
    hearing.sprite_hidden = true;
    CHECK( accessibility::format_mutation_power_row( hearing ) ==
           "Good Hearing, starting trait, sprite hidden." );
}

TEST_CASE( "spell_and_style_formatters_speak_castability_cost_and_description",
           "[accessibility][nogame]" )
{
    accessibility::spell_row missile;
    missile.name = "<color_c_cyan>Magic missile</color>";
    missile.cost = "10 mana";
    missile.casting_time = "100 moves";
    CHECK( accessibility::format_spell_row( missile ) ==
           "Magic missile, castable, 10 mana, 100 moves." );
    missile.favorite = true;
    missile.cost_impeded = true;
    missile.time_impeded = true;
    CHECK( accessibility::format_spell_row( missile ) ==
           "Magic missile, castable, 10 mana, cost impeded, 100 moves, time impeded, favorite." );
    missile.favorite = false;
    missile.cost_impeded = false;
    missile.time_impeded = false;
    missile.unavailable_reason = "not enough mana";
    CHECK( accessibility::format_spell_row( missile ) ==
           "Magic missile, 10 mana, 100 moves. Unavailable: not enough mana." );
    missile.unavailable_reason.clear();
    missile.spell_class = "<color_yellow>Magus</color>";
    missile.favorite = true;
    CHECK( accessibility::format_spell_row( missile ) ==
           "Magic missile, Magus, castable, 10 mana, 100 moves, favorite." );
    missile.favorite = false;
    missile.unavailable_reason = "not enough mana";
    CHECK( accessibility::format_spell_row( missile ) ==
           "Magic missile, Magus, 10 mana, 100 moves. Unavailable: not enough mana." );

    CHECK( accessibility::format_spell_count( 0 ) == "No spells" );
    CHECK( accessibility::format_spell_count( 1 ) == "1 spell" );
    CHECK( accessibility::format_spell_count( 3 ) == "3 spells" );
    CHECK( accessibility::format_uilist_open( "Choose a Spell. Favorites", "3 spells",
            "Magic missile, castable, 10 mana, 100 moves.", "Alt+d", true ) ==
           "Choose a Spell. Favorites. 3 spells. Use arrow keys to choose. Alt+d for details. "
           "Magic missile, castable, 10 mana, 100 moves." );

    CHECK( accessibility::format_martial_style_row( "<color_c_pink>Brawling</color>", true,
            "You're used to hand-to-creature fighting.  You know how to fight." ) ==
           "Brawling, current. You're used to hand-to-creature fighting." );
    CHECK( accessibility::format_martial_style_row( "Karate", false, "" ) == "Karate." );
}

TEST_CASE( "character_subscreen_formatters_speak_health_state_and_layering",
           "[accessibility][nogame]" )
{
    // Morale lines keep the window's label and value; a dash total is zero.
    CHECK( accessibility::format_morale_line( "Total morale:", "+12" ) == "Total morale: +12." );
    CHECK( accessibility::format_morale_line( "Total morale:", "-" ) == "Total morale: 0." );
    CHECK( accessibility::format_morale_line( "<color_c_green>Ate a good meal</color>", "25%" ) ==
           "Ate a good meal, 25%." );
    CHECK( accessibility::format_morale_line( "Nothing affects your morale", "" ) ==
           "Nothing affects your morale." );

    // Health bars in tenths, at the bar's own precision.
    CHECK( accessibility::format_health_bar( "|||||" ) == "health 10 of 10" );
    CHECK( accessibility::format_health_bar( "<color_c_yellow>||\\</color>" ) == "health 5 of 10" );
    CHECK( accessibility::format_health_bar( "\\" ) == "health 1 of 10" );
    CHECK( accessibility::format_health_bar( ":" ) == "health under 1 of 10" );
    CHECK( accessibility::format_health_bar( "-----" ) == "health 0 of 10" );
    CHECK( accessibility::format_mending_bar( 2 ) == "mending 2 of 5" );

    CHECK( accessibility::format_body_part_row( "Head", "health 10 of 10", false, true ) ==
           "Head, health 10 of 10, has details." );
    CHECK( accessibility::format_body_part_row( "Left arm", "", true, false ) ==
           "Left arm, missing." );
    CHECK( accessibility::format_medical_health_row( "Torso", "health 5 of 10", { "BROKEN", "Bleeding" } )
           == "Torso, health 5 of 10, BROKEN, Bleeding." );
    CHECK( accessibility::format_proficiency_window_row( "Knapping", true, 100 ) ==
           "Knapping, known." );
    CHECK( accessibility::format_proficiency_window_row( "Tailoring", false, 40 ) ==
           "Tailoring, learning, 40%." );

    // Armor rows speak what the list shows by color and letter.
    accessibility::armor_row hoodie;
    hoodie.name = "hoodie";
    CHECK( accessibility::format_armor_row( hoodie ) == "hoodie." );
    hoodie.side = "left";
    hoodie.hidden = true;
    hoodie.stacking_conflict = true;
    hoodie.out_of_order = true;
    CHECK( accessibility::format_armor_row( hoodie ) ==
           "hoodie, left, hidden, layering conflict, out of order." );

    // Moving speaks the neighbours; the list runs from innermost to outermost.
    CHECK( accessibility::format_armor_position( "", "hoodie" ) == "innermost, above hoodie." );
    CHECK( accessibility::format_armor_position( "T-shirt", "" ) == "outermost, below T-shirt." );
    CHECK( accessibility::format_armor_position( "T-shirt", "hoodie" ) ==
           "above hoodie, below T-shirt." );
    CHECK( accessibility::format_armor_position( "", "" ) == "only item." );
    CHECK( accessibility::format_armor_moving( "hoodie" ) == "Moving hoodie." );
    CHECK( accessibility::format_armor_placed( "hoodie." ) == "Placed. hoodie." );
    CHECK( accessibility::format_armor_sprite( true, "hoodie, hidden." ) ==
           "Sprite hidden. hoodie, hidden." );
    CHECK( accessibility::format_armor_sprite( false, "hoodie." ) == "Sprite shown. hoodie." );

    accessibility::armor_row shirt;
    shirt.name = "T-shirt";
    shirt.encumbrance = 4;
    hoodie = accessibility::armor_row();
    hoodie.name = "hoodie";
    hoodie.encumbrance = 8;
    hoodie.stacking_conflict = true;
    CHECK( accessibility::format_armor_summary( "Torso", { shirt, hoodie }, 14, 2 ) ==
           "Torso, 2 items. T-shirt, encumbrance 4. hoodie, encumbrance 8, layering conflict. "
           "Total encumbrance 14, layering 2." );
    CHECK( accessibility::format_armor_summary( "All", {}, -1, 0 ) == "All, No items." );

    accessibility::encumbrance_row torso;
    torso.part = "Torso";
    torso.encumbrance = 12;
    torso.layering = 2;
    torso.covered = true;
    CHECK( accessibility::format_encumbrance_row( torso, false ) ==
           "Torso, encumbrance 12, layering 2, covered by this item." );
}

TEST_CASE( "dialogue_formatters_speak_speaker_key_consequence_and_unavailable_reason",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_dialogue_utterance( "Rubik", "\"Hello, stranger.\"" ) ==
           "Rubik: \"Hello, stranger.\"" );
    CHECK( accessibility::format_dialogue_utterance( "", "<color_green>Access granted.</color>" ) ==
           "Access granted." );

    accessibility::dialogue_response_row row;
    row.key = "a";
    row.text = "Ask about the job";
    CHECK( accessibility::format_dialogue_response( row ) == "a, Ask about the job." );
    row.key = "L";
    row.text = "Look at";
    CHECK( accessibility::format_dialogue_response( row ) == "Shift+l, Look at." );
    row.key = "c";
    row.text = "[LIE 50%] I'm a doctor";
    row.unavailable = true;
    row.reason = "requires a lab coat.";
    CHECK( accessibility::format_dialogue_response( row ) ==
           "c, [LIE 50%] I'm a doctor. Unavailable: requires a lab coat." );
    row.reason.clear();
    CHECK( accessibility::format_dialogue_response( row ) ==
           "c, [LIE 50%] I'm a doctor. Unavailable." );
    row.unavailable = false;
    row.hint = accessibility::dialogue_response_hint::hostile;
    row.text = "Attack";
    CHECK( accessibility::format_dialogue_response( row ) == "c, Attack. May provoke an attack." );
    row.hint = accessibility::dialogue_response_hint::helpless;
    row.text = "Let them tie you up";
    CHECK( accessibility::format_dialogue_response( row ) ==
           "c, Let them tie you up. Leaves you helpless." );
    row.hint = accessibility::dialogue_response_hint::action;
    row.text = "Hand over the radio";
    CHECK( accessibility::format_dialogue_response( row ) ==
           "c, Hand over the radio. Action." );

    CHECK( accessibility::format_dialogue_prompt( "Your response", 1 ) == "Your response, 1 option." );
    CHECK( accessibility::format_dialogue_prompt( "Your response", 5 ) ==
           "Your response, 5 options." );
}

TEST_CASE( "dialogue_speech_model_navigates_history_and_responses", "[accessibility][nogame]" )
{
    using model = accessibility::dialogue_speech_model;
    const auto response = []( const std::string & key, const std::string & text ) {
        accessibility::dialogue_response_row row;
        row.key = key;
        row.text = text;
        return accessibility::format_dialogue_response( row );
    };
    accessibility::dialogue_response_row lie;
    lie.key = "b";
    lie.text = "[LIE 50%] I'm a doctor";
    lie.unavailable = true;
    lie.reason = "requires a lab coat.";
    const std::string lie_row = accessibility::format_dialogue_response( lie );

    model m;
    m.set_title( "Dialogue: Rubik" );
    m.set_prompt_label( "Your response" );
    m.add_utterance( accessibility::format_dialogue_utterance( "Rubik", "\"Hello, stranger.\"" ),
                     false );
    m.set_options( {
        { response( "a", "Ask about the job" ), "" },
        { lie_row, "" },
        { response( "c", "Bye" ), "" },
        { response( "L", "Look at" ), "LOOK_AT" }
    }, 3, 0 );

    // Entry speaks the new utterance once and focuses the prompt.
    CHECK( m.begin_topic() == "Rubik: \"Hello, stranger.\" Your response, 3 options." );
    CHECK_FALSE( m.in_response_list() );

    // Up rereads the utterance and stops at the oldest one.
    model::result r = m.handle( "UP" );
    CHECK( r.consumed );
    CHECK( r.speech == "Rubik: \"Hello, stranger.\"" );
    CHECK( r.response_cursor == -1 );
    CHECK( m.handle( "UP" ).speech == "Rubik: \"Hello, stranger.\"" );
    r = m.handle( "ACCESSIBILITY_DETAILS" );
    CHECK( r.consumed );
    CHECK( r.document == "Rubik: \"Hello, stranger.\"" );
    CHECK( m.handle( "ACCESSIBILITY_STATUS" ).speech == "Dialogue: Rubik. Conversation. 1 of 1." );
    // Enter on an utterance only rereads it.
    CHECK( m.handle( "CONFIRM" ).speech == "Rubik: \"Hello, stranger.\"" );
    CHECK( m.handle( "DOWN" ).speech == "Your response, 3 options." );
    CHECK( m.handle( "DOWN" ).speech == "Your response, 3 options." );
    CHECK( m.handle( "ACCESSIBILITY_STATUS" ).speech ==
           "Dialogue: Rubik. Your response, 3 options." );

    // Letter hotkeys and Escape keep the game behavior on the conversation level.
    r = m.handle( "ANY_INPUT" );
    CHECK_FALSE( r.consumed );
    CHECK( r.action == "ANY_INPUT" );
    CHECK( r.speech.empty() );
    r = m.handle( "QUIT" );
    CHECK_FALSE( r.consumed );
    CHECK( r.action == "QUIT" );

    // Enter on the prompt opens the response list at the visible cursor.
    r = m.handle( "CONFIRM" );
    CHECK( r.consumed );
    CHECK( m.in_response_list() );
    CHECK( r.speech == "Your response, 3 options. a, Ask about the job." );
    CHECK( r.response_cursor == 0 );
    r = m.handle( "DOWN" );
    CHECK( r.consumed );
    CHECK( r.speech == lie_row );
    CHECK( r.response_cursor == 1 );
    CHECK( m.focus() == lie_row );
    CHECK( m.option_row( 1 ) == lie_row );
    CHECK( m.handle( "DOWN" ).speech == "c, Bye." );
    // Extra actions follow the responses; the visible cursor stays on the responses.
    r = m.handle( "DOWN" );
    CHECK( r.speech == "Shift+l, Look at." );
    CHECK( r.response_cursor == -1 );
    CHECK( m.handle( "ACCESSIBILITY_STATUS" ).speech == "Dialogue: Rubik. Your response. 4 of 4." );
    CHECK( m.handle( "ACCESSIBILITY_DETAILS" ).document == "Shift+l, Look at." );
    r = m.handle( "CONFIRM" );
    CHECK_FALSE( r.consumed );
    CHECK( r.action == "LOOK_AT" );
    // Wrapping says top or bottom before the row.
    CHECK( m.handle( "DOWN" ).speech == "Top. a, Ask about the job." );
    CHECK( m.handle( "UP" ).speech == "Bottom. Shift+l, Look at." );
    CHECK( m.handle( "HOME" ).speech == "a, Ask about the job." );
    CHECK( m.handle( "END" ).speech == "Shift+l, Look at." );
    r = m.handle( "ANY_INPUT" );
    CHECK_FALSE( r.consumed );
    CHECK( r.action == "ANY_INPUT" );

    // Escape returns to the prompt instead of leaving the conversation.
    r = m.handle( "QUIT" );
    CHECK( r.consumed );
    CHECK( r.speech == "Your response, 3 options." );
    CHECK_FALSE( m.in_response_list() );

    // Reopening keeps the list position; Enter on a response confirms it.
    CHECK( m.handle( "CONFIRM" ).speech == "Your response, 3 options. Shift+l, Look at." );
    m.handle( "HOME" );
    m.handle( "DOWN" );
    r = m.handle( "CONFIRM" );
    CHECK_FALSE( r.consumed );
    CHECK( r.action == "CONFIRM" );
    CHECK( r.response_cursor == 1 );

    // A topic transition speaks only the new line, not the player's own choice.
    m.add_utterance( accessibility::format_dialogue_utterance( "You", "Ask about the job" ), true );
    m.add_utterance( accessibility::format_dialogue_utterance( "Rubik", "We need supplies." ), false );
    m.set_options( { { response( "a", "Sure" ), "" }, { response( "b", "No" ), "" } }, 2, 0 );
    CHECK( m.begin_topic() == "Rubik: We need supplies. Your response, 2 options." );
    CHECK_FALSE( m.in_response_list() );
    // A repeated prompt without new lines speaks only the prompt.
    CHECK( m.begin_topic() == "Your response, 2 options." );
    CHECK( m.handle( "UP" ).speech == "Rubik: We need supplies." );
    CHECK( m.handle( "UP" ).speech == "You: Ask about the job." );
    CHECK( m.handle( "ACCESSIBILITY_STATUS" ).speech == "Dialogue: Rubik. Conversation. 2 of 3." );
    CHECK( m.handle( "HOME" ).speech == "Rubik: \"Hello, stranger.\"" );
    CHECK( m.handle( "END" ).speech == "Your response, 2 options." );
    CHECK( m.handle( "PAGE_UP" ).speech == "Rubik: \"Hello, stranger.\"" );
    CHECK( m.handle( "PAGE_DOWN" ).speech == "Your response, 2 options." );
    // Alt+D on the prompt repeats it.
    r = m.handle( "ACCESSIBILITY_DETAILS" );
    CHECK( r.document.empty() );
    CHECK( r.speech == "Your response, 2 options." );
    // Unrelated actions pass through untouched.
    r = m.handle( "SCROLL_INFOBOX_UP" );
    CHECK_FALSE( r.consumed );
    CHECK( r.action == "SCROLL_INFOBOX_UP" );

    // Computer terminals follow the same model without a speaker.
    model terminal;
    terminal.set_prompt_label( "Your input" );
    terminal.add_utterance( accessibility::format_dialogue_utterance( "", "Enter password." ), false );
    terminal.set_options( { { response( "a", "Log out" ), "" } }, 1, 0 );
    CHECK( terminal.begin_topic() == "Enter password. Your input, 1 option." );
    CHECK( terminal.handle( "CONFIRM" ).speech == "Your input, 1 option. a, Log out." );
}

TEST_CASE( "trade_formatters_speak_price_marks_balance_and_result", "[accessibility][nogame]" )
{
    accessibility::trade_row row;
    row.item.caption = "apple";
    row.item.category = "FOOD";
    row.item.location = "on the floor";
    row.unit_price = "$2.00";
    row.available = 5;
    CHECK( accessibility::format_trade_row( row, false, false ) == "apple. $2.00 each." );
    row.marked = 3;
    CHECK( accessibility::format_trade_row( row, true, false ) ==
           "FOOD. apple. $2.00 each. 3 marked." );
    CHECK( accessibility::format_trade_row( row, false, true ) ==
           "apple. $2.00 each. 3 marked. Location: on the floor." );
    row.available = 1;
    row.marked = 1;
    CHECK( accessibility::format_trade_row( row, false, false ) == "apple. $2.00 each. Marked." );
    row.unit_price.clear();
    row.marked = 0;
    row.item.denial = "Won't sell their own equipment";
    row.item.not_owned = true;
    row.item.owner = "Hell's Raiders";
    CHECK( accessibility::format_trade_row( row, false, false ) ==
           "apple. Not owned. Unavailable: Won't sell their own equipment." );
    CHECK( accessibility::format_trade_row( row, false, true ).find( "Owner: Hell's Raiders" ) !=
           std::string::npos );

    CHECK( accessibility::format_trade_marked( 0, 4 ).empty() );
    CHECK( accessibility::format_trade_mark_change( "apple", 2, 4 ) == "apple. 2 marked." );
    CHECK( accessibility::format_trade_mark_change( "apple", 0, 4 ) == "apple. Unmarked." );

    accessibility::trade_balance balance;
    balance.amount = "$3.00";
    CHECK( accessibility::format_trade_balance( balance ) == "Balance: credit $3.00, acceptable" );
    balance.debt = true;
    balance.amount = "$5.00";
    balance.acceptable = false;
    CHECK( accessibility::format_trade_balance( balance ) ==
           "Balance: you owe $5.00, trader refuses" );
    accessibility::trade_balance exchange;
    exchange.exchange = true;
    CHECK( accessibility::format_trade_balance( exchange ) == "Free exchange, acceptable" );

    CHECK( accessibility::format_trade_pane_label( false ) == "Trader's goods" );
    CHECK( accessibility::format_trade_pane_label( true ) == "Your goods" );
    CHECK( accessibility::format_trade_opening( "Pay:", "Rubik", balance ) ==
           "Pay: Rubik. Balance: you owe $5.00, trader refuses." );
    CHECK( accessibility::format_trade_opening( "Trade", "Rubik", exchange ) ==
           "Trade: Rubik. Free exchange, acceptable." );

    accessibility::trade_summary summary;
    summary.title = "Trade";
    summary.trader = "Rubik";
    summary.balance = balance;
    summary.buying = "$6.00";
    summary.selling = "$1.00";
    CHECK( accessibility::format_trade_summary( summary ) ==
           "Trade: Rubik. Balance: you owe $5.00, trader refuses. Buying: $6.00. Selling: $1.00." );
    summary.bank = "$100.00";
    CHECK( accessibility::format_trade_summary( summary ).find( "Bank balance: $100.00." ) !=
           std::string::npos );
    CHECK( accessibility::format_trade_bank_change( balance, "$95.00" ) ==
           "Balance: you owe $5.00, trader refuses. Bank balance: $95.00." );

    CHECK( accessibility::format_trade_result( true ) == "Trade complete" );
    CHECK( accessibility::format_trade_result( false ) == "Trade cancelled" );
}

TEST_CASE( "mission_formatters_speak_state_deadline_location_and_objective",
           "[accessibility][nogame]" )
{
    using accessibility::mission_category;
    accessibility::mission_row row;
    row.title = "Reach Refugee Center";
    row.tracked = true;
    row.time = "2 days";
    CHECK( accessibility::format_mission_row( row ) ==
           "Reach Refugee Center, current objective, deadline in 2 days." );
    row.tracked = false;
    row.deadline_passed = true;
    CHECK( accessibility::format_mission_row( row ) == "Reach Refugee Center, deadline passed." );
    row.deadline_passed = false;
    row.time.clear();
    CHECK( accessibility::format_mission_row( row ) == "Reach Refugee Center." );
    row.category = mission_category::completed;
    row.time = "3 hours";
    CHECK( accessibility::format_mission_row( row ) == "Reach Refugee Center, completed 3 hours ago." );
    row.category = mission_category::failed;
    CHECK( accessibility::format_mission_row( row ) == "Reach Refugee Center, failed 3 hours ago." );

    CHECK( accessibility::format_mission_tab( mission_category::active, 3, "Find a book." ) ==
           "Active, 3 missions. Find a book." );
    CHECK( accessibility::format_mission_tab( mission_category::failed, 0, "" ) ==
           "Failed, no missions." );
    CHECK( accessibility::format_mission_screen_opening(
               accessibility::format_mission_tab( mission_category::completed, 1, "Find a book." ) ) ==
           "Your missions. Completed, 1 mission. Find a book." );
    CHECK( accessibility::format_mission_summary( mission_category::active, 2, 3, "Find a book" ) ==
           "Your missions, Active, 3 missions. 2 of 3. Current objective: Find a book." );
    CHECK( accessibility::format_mission_summary( mission_category::failed, 0, 0, "" ) ==
           "Your missions, Failed, no missions. No current objective." );
    CHECK( accessibility::format_mission_tracked( "Find a book" ) ==
           "Current objective: Find a book." );
    CHECK( accessibility::format_mission_track_unavailable( mission_category::active, 2 ).empty() );
    CHECK( accessibility::format_mission_track_unavailable( mission_category::completed, 2 ) ==
           "Unavailable: only an active mission can be the current objective." );
    CHECK( accessibility::format_mission_track_unavailable( mission_category::active, 0 ) ==
           "Unavailable: no mission selected." );

    CHECK( accessibility::format_mission_location( "Target", 12, "northeast", "about 288 m" ) ==
           "Target: 12 map tiles northeast, about 288 m." );
    CHECK( accessibility::format_mission_location( "Target", 1, "south", "24 m" ) ==
           "Target: 1 map tile south, 24 m." );
    CHECK( accessibility::format_mission_location( "Giver location", 0, "", "" ) ==
           "Giver location: here." );

    accessibility::mission_details details;
    details.row.title = "Find a book";
    details.row.tracked = true;
    details.position = 1;
    details.count = 2;
    details.giver = "Hector";
    details.faction = "Free Merchants";
    details.giver_location = "Giver location: here.";
    details.description = "Bring me a <color_white>book</color>.  I will pay $100.\nHurry up!";
    details.time_lines = { "Deadline: Day 3", "Time remaining: 2 days" };
    details.location = "Target: 3 map tiles west, 72 m.";
    const std::vector<std::string> paragraphs = accessibility::format_mission_details( details );
    const std::vector<std::string> expected = {
        "Find a book, current objective. Active. 1 of 2.",
        "Given by: Hector. Faction: Free Merchants.",
        "Giver location: here.",
        "Objective: Bring me a book.",
        "I will pay $100.",
        "Hurry up!",
        "Deadline: Day 3.",
        "Time remaining: 2 days.",
        "Target: 3 map tiles west, 72 m."
    };
    CHECK( paragraphs == expected );
    details.giver.clear();
    details.location.clear();
    const std::vector<std::string> unknown = accessibility::format_mission_details( details );
    CHECK( unknown[1] == "Objective: Bring me a book." );
    CHECK( unknown.back() == "No known location." );
}

TEST_CASE( "overmap_formatters_speak_offset_markers_places_and_routes",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_overmap_offset( tripoint( 2, -3, 0 ) ) == "3 north 2 east" );
    CHECK( accessibility::format_overmap_offset( tripoint::zero ) == "here" );
    CHECK( accessibility::format_overmap_offset( tripoint( -5, 0, -1 ) ) == "5 west, 1 level below" );
    CHECK( accessibility::format_overmap_offset( tripoint( 0, 4, 2 ) ) == "4 south, 2 levels above" );
    CHECK( accessibility::format_overmap_level( -1 ) == "Level -1" );

    accessibility::overmap_tile tile;
    tile.terrain = "forest";
    tile.offset = tripoint( 2, -3, 0 );
    CHECK( accessibility::format_overmap_tile( tile, false ) == "forest, 3 north 2 east." );
    CHECK( accessibility::format_overmap_tile( tile, true ) == "forest." );
    tile.note = "Base";
    tile.danger_radius = 3;
    tile.missions = { "Find a book" };
    tile.route_destination = true;
    tile.on_route = true;
    CHECK( accessibility::format_overmap_tile( tile, false ) ==
           "forest, note: Base, dangerous area, radius 3, mission target: Find a book, "
           "route destination, 3 north 2 east." );
    // Identical terrain drops only the offset; markers stay.
    CHECK( accessibility::format_overmap_tile( tile, true ) ==
           "forest, note: Base, dangerous area, radius 3, mission target: Find a book, "
           "route destination." );
    accessibility::overmap_tile unknown;
    unknown.in_dangerous_area = true;
    unknown.on_route = true;
    unknown.offset = tripoint::south;
    CHECK( accessibility::format_overmap_tile( unknown, false ) ==
           "unexplored, in dangerous area, on route, 1 south." );

    CHECK( accessibility::format_overmap_opening( "", "forest, here." ) ==
           "Overmap. forest, here." );
    CHECK( accessibility::format_overmap_opening( "<color_white>Select a camp</color>",
            "forest, here." ) == "Select a camp. forest, here." );
    CHECK( accessibility::format_overmap_level_change( -1, "basement, here." ) ==
           "Level -1. basement, here." );

    accessibility::known_place place;
    place.name = "evac shelter";
    place.distance = 12;
    place.direction = "south";
    place.note = "supplies";
    CHECK( accessibility::format_known_place_row( place ) ==
           "evac shelter, 12 map tiles south, note: supplies" );
    accessibility::known_place city;
    city.name = "Townsville";
    city.distance = 1;
    city.direction = "north";
    city.city = true;
    CHECK( accessibility::format_known_place_row( city ) == "Townsville, 1 map tile north, city" );
    accessibility::known_place target;
    target.name = "unexplored";
    target.dz = -1;
    target.missions = { "Find a book" };
    target.current_objective = true;
    target.route_destination = true;
    target.in_dangerous_area = true;
    CHECK( accessibility::format_known_place_row( target ) ==
           "unexplored, here, 1 level below, in dangerous area, mission target: Find a book, "
           "current objective, route destination" );
    CHECK( accessibility::format_known_place_count( 0 ) == "no places" );
    CHECK( accessibility::format_known_place_count( 1 ) == "1 place" );
    CHECK( accessibility::format_known_place_count( 12 ) == "12 places" );

    CHECK( accessibility::format_overmap_route_preview( "field", "8 map tiles north", 9, "Enter" ) ==
           "Route to field, 8 map tiles north. 9 map tiles of travel. Press Enter again to travel." );
    CHECK( accessibility::format_overmap_no_route( "forest", false ) == "No route to forest." );
    CHECK( accessibility::format_overmap_no_route( "forest", true ) ==
           "No route to forest. Travel needs the destination on your level." );
    CHECK( accessibility::format_overmap_no_route_selected() == "No route selected." );
    CHECK( accessibility::format_overmap_route_state( "", "" ) == "No route selected." );
    CHECK( accessibility::format_overmap_route_state( "field", "8 map tiles north" ) ==
           "Route: field, 8 map tiles north." );
    CHECK( accessibility::format_overmap_summary( "", "forest, here.", 0, "No route selected." ) ==
           "Overmap. Cursor: forest, here. Level 0. No route selected." );
    CHECK( accessibility::format_overmap_search_results( 5, "forest, 3 north." ) ==
           "Search results, 5 results. forest, 3 north." );
    CHECK( accessibility::format_overmap_search_results( 1, "forest, 3 north." ) ==
           "Search results, 1 result. forest, 3 north." );
    CHECK( accessibility::format_overmap_search_position( 2, 5, "forest, 3 north." ) ==
           "Result 2 of 5. forest, 3 north." );
    CHECK( accessibility::format_semantic_sequence( {
        accessibility::format_list_wrap( 4, 0, 1 ), "forest, 3 north."
    } ) == "Top. forest, 3 north." );
    CHECK( accessibility::format_overmap_explored( true ) == "Marked explored." );
    CHECK( accessibility::format_overmap_explored( false ) == "Marked unexplored." );
}

TEST_CASE( "zone_formatters_speak_row_size_state_and_details", "[accessibility][nogame]" )
{
    accessibility::zone_row row;
    row.name = "Loot: Food";
    row.type = "Loot: Food";
    row.distance = 5;
    row.direction = "north";
    // A name equal to the type is spoken once.
    CHECK( accessibility::format_zone_row( row ) == "Loot: Food, enabled, 5 tiles north." );
    row.name = "Pantry";
    row.enabled = false;
    row.personal = true;
    row.dz = -1;
    CHECK( accessibility::format_zone_row( row ) ==
           "Pantry, Loot: Food, personal, disabled, 5 tiles north, 1 level below." );
    row.personal = false;
    row.vehicle = true;
    row.distance = 0;
    row.dz = 0;
    CHECK( accessibility::format_zone_row( row ) == "Pantry, Loot: Food, vehicle, disabled, here." );
    row.distance = 1;
    row.direction = "east";
    CHECK( accessibility::format_zone_row( row ) ==
           "Pantry, Loot: Food, vehicle, disabled, 1 tile east." );

    CHECK( accessibility::format_zone_count( 0 ) == "no zones" );
    CHECK( accessibility::format_zone_count( 1 ) == "1 zone" );
    CHECK( accessibility::format_zones_manager_opening( 0, "" ) == "Zones manager, no zones." );
    CHECK( accessibility::format_zones_manager_opening( 2, "Den, enabled, here." ) ==
           "Zones manager, 2 zones. Den, enabled, here." );
    CHECK( accessibility::format_zone_size( 4, 3, 1 ) == "Zone 4 by 3." );
    CHECK( accessibility::format_zone_size( 4, 3, 2 ) == "Zone 4 by 3, 2 levels." );
    CHECK( accessibility::format_zone_enabled( "Pantry", true ) == "Pantry enabled." );
    CHECK( accessibility::format_zone_enabled( "Pantry", false ) == "Pantry disabled." );
    CHECK( accessibility::format_zone_displayed( "Pantry", true ) == "Pantry shown on map." );
    CHECK( accessibility::format_zone_displayed( "Pantry", false ) == "Pantry hidden from map." );
    CHECK( accessibility::format_zone_moved( "Pantry", 2, 5 ) == "Pantry, 2 of 5." );
    CHECK( accessibility::format_zone_removed( "Pantry", "Den, enabled, here." ) ==
           "Removed Pantry. Den, enabled, here." );
    CHECK( accessibility::format_zone_removed( "Pantry", "" ) == "Removed Pantry. No zones." );
    CHECK( accessibility::format_personal_zones_changed( true, 2 ) ==
           "2 personal zones enabled." );
    CHECK( accessibility::format_personal_zones_changed( false, 1 ) ==
           "1 personal zone disabled." );
    CHECK( accessibility::format_personal_zones_changed( true, 0 ) ==
           "No personal zones changed." );
    CHECK( accessibility::format_zone_list_change( accessibility::format_zone_filter( true ), 3,
            "Den, enabled, here." ) == "Showing all zones, 3 zones. Den, enabled, here." );
    CHECK( accessibility::format_zone_list_change( accessibility::format_zone_filter( false ), 0,
            "" ) == "Hiding distant zones, no zones." );
    CHECK( accessibility::format_zone_options( {} ).empty() );
    CHECK( accessibility::format_zone_options( { { "Filter: ", "apple" } } ) ==
    "Options: Filter: apple." );
    CHECK( accessibility::format_zone_options( { { "Unload: ", "mods ignore stacks over 20, " } } ) ==
    "Options: Unload: mods ignore stacks over 20." );

    accessibility::zone_details details;
    details.row = row;
    details.position = 2;
    details.count = 5;
    details.displayed = true;
    details.width = 4;
    details.height = 3;
    details.options = { { "Filter: ", "apple" } };
    CHECK( accessibility::format_zone_details( details ) ==
           "Pantry, Loot: Food, vehicle, disabled, 1 tile east. 2 of 5. Zone 4 by 3. Shown on map. "
           "Options: Filter: apple." );
    CHECK( accessibility::format_zones_manager_summary( "Showing all zones", "", 2, 5,
            "Den, enabled, here." ) == "Zones manager. Showing all zones. 2 of 5. Den, enabled, here." );
    CHECK( accessibility::format_zones_manager_summary( "Hiding distant zones", "hells_raiders", 0,
            0, "" ) == "Zones manager. Hiding distant zones. Faction: hells_raiders. No zones." );
}

TEST_CASE( "vehicle_formatters_speak_speed_heading_fuel_and_obstacles", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_vehicle_speed( 20, "mph" ) == "20 mph" );
    CHECK( accessibility::format_vehicle_speed( -5, "km/h" ) == "5 km/h reverse" );
    CHECK( accessibility::format_vehicle_heading( "northeast", 45 ) ==
           "Heading northeast, 45 degrees." );
    CHECK( accessibility::format_vehicle_cruise( 20, "mph", false, false ) == "Cruise 20 mph." );
    CHECK( accessibility::format_vehicle_cruise( 60, "mph", true, false ) ==
           "Cruise 60 mph, above safe speed." );
    CHECK( accessibility::format_vehicle_cruise( 80, "mph", true, true ) ==
           "Cruise 80 mph, above safe speed, maximum." );
    CHECK( accessibility::format_vehicle_cruise( -10, "mph", false, true ) ==
           "Cruise 10 mph reverse, maximum." );
    CHECK( accessibility::format_vehicle_strain( accessibility::vehicle_strain::none ).empty() );
    CHECK( accessibility::format_vehicle_strain( accessibility::vehicle_strain::light ) ==
           "light engine strain" );
    CHECK( accessibility::format_vehicle_strain( accessibility::vehicle_strain::severe ) ==
           "severe engine strain" );

    accessibility::vehicle_fuel_gauge gasoline;
    gasoline.fuel = "gasoline";
    gasoline.percent = 45;
    gasoline.estimate = "2 hours";
    CHECK( accessibility::format_vehicle_fuel( gasoline ) == "gasoline 45%, 2 hours until empty." );
    accessibility::vehicle_fuel_gauge battery;
    battery.fuel = "battery";
    battery.percent = 80;
    battery.estimate = "3 hours";
    battery.filling = true;
    CHECK( accessibility::format_vehicle_fuel( battery ) == "battery 80%, 3 hours until full." );
    accessibility::vehicle_fuel_gauge diesel;
    diesel.fuel = "diesel";
    diesel.percent = 100;
    CHECK( accessibility::format_vehicle_fuel( diesel ) == "diesel 100%." );

    accessibility::vehicle_status status;
    status.name = "Car";
    status.units = "mph";
    status.speed = 20;
    status.cruise = 30;
    status.strain = accessibility::vehicle_strain::heavy;
    status.heading = 0;
    status.compass = "north";
    status.engine_on = true;
    status.fuels = { gasoline, battery };
    CHECK( accessibility::format_vehicle_status( status ) ==
           "Car. Speed 20 mph, cruise 30 mph, heavy engine strain. Heading north, 0 degrees. "
           "Engine on. gasoline 45%, 2 hours until empty. battery 80%, 3 hours until full." );
    status.speed = -5;
    status.cruise = -10;
    status.strain = accessibility::vehicle_strain::none;
    status.turning = true;
    status.steering = 45;
    status.steering_compass = "northeast";
    status.engine_on = false;
    status.skidding = true;
    status.fuels.clear();
    CHECK( accessibility::format_vehicle_status( status ) ==
           "Car. Speed 5 mph reverse, cruise 10 mph reverse. Heading north, 0 degrees, turning to "
           "northeast, 45 degrees. Engine off. Skidding." );

    CHECK( accessibility::format_vehicle_obstacle( "wall", 4, false ) == "wall ahead, 4 tiles." );
    CHECK( accessibility::format_vehicle_obstacle( "tree", 1, true ) == "tree behind, 1 tile." );
    CHECK( accessibility::format_vehicle_engine_row( "V8 engine", "gasoline", true ) ==
           "V8 engine, gasoline, on" );
    CHECK( accessibility::format_vehicle_engine_row( "V8 engine", "gasoline", false ) ==
           "V8 engine, gasoline, off" );
}

TEST_CASE( "vehicle_workshop_formatters_speak_location_condition_and_requirements",
           "[accessibility][nogame]" )
{
    accessibility::vehicle_grid_position position;
    position.min.x = -1;
    position.max.x = 1;
    position.min.y = -1;
    position.max.y = 1;
    position.tile.x = 1;
    position.tile.y = -1;
    CHECK( accessibility::format_vehicle_location( position ) == "front left" );
    CHECK( accessibility::format_vehicle_position_details( position ) ==
           "Row 1 of 3 from the front, column 1 of 3 from the left." );
    position.tile.x = 0;
    position.tile.y = 0;
    CHECK( accessibility::format_vehicle_location( position ) == "center" );
    position.tile.y = 1;
    CHECK( accessibility::format_vehicle_location( position ) == "right" );
    position.tile.x = -1;
    position.tile.y = 0;
    CHECK( accessibility::format_vehicle_location( position ) == "rear" );
    position.tile.x = 3;
    CHECK( accessibility::format_vehicle_location( position ) == "outside, front" );
    CHECK( accessibility::format_vehicle_position_details( position ) ==
           "2 tiles in front of the vehicle, column 2 of 3 from the left." );
    // A vehicle one tile wide has no left or right side.
    position.min.y = 0;
    position.max.y = 0;
    position.tile.x = 1;
    CHECK( accessibility::format_vehicle_location( position ) == "front" );

    CHECK( accessibility::format_vehicle_part_condition( false, 0 ).empty() );
    CHECK( accessibility::format_vehicle_part_condition( false, 40 ) == "damaged 40%" );
    CHECK( accessibility::format_vehicle_part_condition( true, 100 ) == "broken" );
    accessibility::vehicle_part_row row;
    row.name = "headlight";
    row.location = "front left";
    row.damage_percent = 40;
    CHECK( accessibility::format_vehicle_part_row( row ) == "headlight, front left, damaged 40%" );
    CHECK( accessibility::format_vehicle_tile_part( row ) == "headlight, damaged 40%" );
    row.name = "tank";
    row.damage_percent = 0;
    row.states = { "leaking" };
    CHECK( accessibility::format_vehicle_part_row( row ) == "tank, front left, leaking" );
    CHECK( accessibility::format_vehicle_tile( "front left", { "frame", "door, damaged 10%" },
            false ) == "front left: frame; door, damaged 10%." );
    CHECK( accessibility::format_vehicle_tile( "outside, front", {}, true ) ==
           "outside, front: no parts, obstructed." );
    CHECK( accessibility::format_vehicle_part_count( 12 ) == "12 parts" );
    CHECK( accessibility::format_vehicle_part_count( 0 ) == "No parts" );
    CHECK( accessibility::format_vehicle_install_count( 1 ) == "1 installable part" );

    CHECK( accessibility::format_vehicle_task( "Repair", "" ) == "Repair." );
    CHECK( accessibility::format_vehicle_task( "Remove", "something is attached to it" ) ==
           "Remove. Unavailable: something is attached to it." );
    CHECK( accessibility::format_vehicle_task_blocked( "Repair", "headlight",
            accessibility::format_vehicle_missing_requirements( 2 ) ) ==
           "Repair headlight. Unavailable: 2 requirements missing." );
    CHECK( accessibility::format_vehicle_missing_requirements( 0 ) == "requirements not met" );
    CHECK( accessibility::format_vehicle_skills( {} ) == "Skills required: none." );
    CHECK( accessibility::format_vehicle_skills( { { "mechanics", 2, true }, { "electronics", 1, false } } )
    ==
    "Skills required: mechanics 2, have; electronics 1, missing." );
    CHECK( accessibility::format_vehicle_extra_skill( "mechanics", 5, false, "for extra engines" ) ==
           "mechanics 5 for extra engines, missing." );
    CHECK( accessibility::format_vehicle_task_time( "30 minutes" ) == "Time required: 30 minutes." );
    CHECK( accessibility::format_vehicle_lift_requirement( "1 tool with jacking 3", true,
            "strength 12", false ) ==
           "Lifting: 1 tool with jacking 3, have; or strength 12, missing." );

    accessibility::vehicle_workshop_stats stats;
    stats.name = "Car";
    stats.units = "mph";
    stats.speeds = { { "", 30, 60, 10 }, { "On water", 5, 8, 2 } };
    stats.mass = "1200 kg";
    stats.cargo_used = "10 L";
    stats.cargo_total = "200 L";
    stats.status = "good";
    stats.wheels = "Wheels: enough";
    stats.most_damaged = "headlight";
    accessibility::vehicle_fuel_gauge gasoline;
    gasoline.fuel = "gasoline";
    gasoline.percent = 45;
    stats.fuels = { gasoline };
    stats.air_drag = 0.5;
    stats.has_rolling_drag = true;
    stats.rolling_drag = 1.25;
    stats.static_drag = 300;
    stats.offroad = 60;
    CHECK( accessibility::format_vehicle_workshop_stats( stats ) ==
           "Car. Safe speed 30 mph, top speed 60 mph, acceleration 10 mph per second. On water: safe "
           "speed 5 mph, top speed 8 mph, acceleration 2 mph per second. Mass: 1200 kg. Cargo: 10 L of "
           "200 L. Status: good. Wheels: enough. Most damaged: headlight. gasoline 45%. Air drag 0.50. "
           "Rolling drag 1.25. Static drag 300. Offroad 60%." );
    stats.owner = "the Old Guard";
    stats.speeds.clear();
    stats.most_damaged_repairable = false;
    stats.needs_repair = "door";
    stats.fuels.clear();
    stats.has_rolling_drag = false;
    CHECK( accessibility::format_vehicle_workshop_stats( stats ) ==
           "Car, owned by the Old Guard. Mass: 1200 kg. Cargo: 10 L of 200 L. Status: good. Wheels: "
           "enough. Most damaged, can't repair: headlight. Needs repair: door. Air drag 0.50. Static "
           "drag 300. Offroad 60%." );
}

TEST_CASE( "character_creation_formatters_speak_rows_points_tabs_and_results",
           "[accessibility][nogame]" )
{
    accessibility::character_choice_row row;
    row.name = "Evacuee";
    row.selected = true;
    CHECK( accessibility::format_character_choice_row( row ) == "Evacuee, 0 points, selected." );
    row.name = "Lab Patient";
    row.points = 2;
    row.selected = false;
    row.unavailable = "requires achievement Escaped";
    CHECK( accessibility::format_character_choice_row( row ) ==
           "Lab Patient, 2 points. Unavailable: requires achievement Escaped." );
    row.unavailable.clear();
    row.short_of_points = true;
    CHECK( accessibility::format_character_choice_row( row ) ==
           "Lab Patient, 2 points. Not enough points." );
    row.name = "Book Hating";
    row.points = -1;
    row.short_of_points = false;
    CHECK( accessibility::format_character_choice_row( row ) == "Book Hating, earns 1 point." );
    row.show_points = false;
    CHECK( accessibility::format_character_choice_row( row ) == "Book Hating." );

    accessibility::character_point_balance balance;
    CHECK( accessibility::format_character_point_balance( balance ).empty() );
    balance.pool = accessibility::character_point_pool::one_pool;
    balance.total = 4;
    CHECK( accessibility::format_character_point_balance( balance ) == "4 points left" );
    balance.total = -3;
    CHECK( accessibility::format_character_point_balance( balance ) == "3 points over budget" );
    balance.pool = accessibility::character_point_pool::multi_pool;
    balance.stat_points = 2;
    balance.trait_points = 3;
    balance.skill_points = -1;
    balance.total = 4;
    CHECK( accessibility::format_character_point_balance( balance ) ==
           "4 points left: stats 2, traits 3, skills -1" );
    balance.valid = false;
    CHECK( accessibility::format_character_point_balance( balance ) ==
           "4 points left: stats 2, traits 3, skills -1. Over budget." );

    CHECK( accessibility::format_character_creation_tab( "Points", 3, "Survivor, selected.", true ) ==
           "Character creation. Points, 3 items. Survivor, selected." );
    CHECK( accessibility::format_character_creation_tab( "Profession", 42, "Unemployed, 0 points.",
            false ) ==
           "Profession, 42 items. Unemployed, 0 points." );
    CHECK( accessibility::format_character_choice_selected( "Evacuee", true, "4 points left" ) ==
           "Selected Evacuee. 4 points left." );
    CHECK( accessibility::format_character_choice_selected( "Reading", false, "" ) ==
           "Removed Reading." );
    CHECK( accessibility::format_character_choice_filter( "doc", 3, "Doctor, 2 points." ) ==
           "Filter doc, 3 items. Doctor, 2 points." );
    CHECK( accessibility::format_character_choice_filter( "", 42, "Unemployed, 0 points." ) ==
           "Filter cleared, 42 items. Unemployed, 0 points." );
    CHECK( accessibility::format_character_choice_sort( true, "Bandit, 0 points." ) ==
           "Sorted by name. Bandit, 0 points." );
    CHECK( accessibility::format_character_choice_sort( false, "Unemployed, 0 points." ) ==
           "Default sort order. Unemployed, 0 points." );
    CHECK( accessibility::format_character_tab_summary( "Background", 5, 60, { "Reading", "Swimming" },
            "4 points left", "Summary: Lifestyle: average" ) ==
           "Background, 60 items. 5 of 60. Current: Reading, Swimming. 4 points left. "
           "Summary: Lifestyle: average." );
    CHECK( accessibility::format_character_tab_summary( "Background", 1, 60, {}, "", "" ) ==
           "Background, 60 items. 1 of 60. Current: none." );
}

TEST_CASE( "character_creation_stat_trait_skill_and_description_formatters",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_character_creation_pane( "Traits", "Positive traits", 40,
            "Fast Learner, 3 points.", true ) ==
           "Character creation. Traits. Positive traits, 40 items. Fast Learner, 3 points." );
    CHECK( accessibility::format_character_creation_pane( "", "Negative traits", 30,
            "Slow Learner, earns 3 points.", false ) ==
           "Negative traits, 30 items. Slow Learner, earns 3 points." );

    CHECK( accessibility::format_character_stat_row( "Strength", 8, "average human", "" ) ==
           "Strength 8, average human." );
    CHECK( accessibility::format_character_stat_row( "Strength", 14, "extraordinary",
            "Increasing Strength further costs 2 points" ) ==
           "Strength 14, extraordinary. Increasing Strength further costs 2 points." );
    CHECK( accessibility::format_character_value_change( "Strength 10", "4 points left", "" ) ==
           "Strength 10, 4 points left." );
    CHECK( accessibility::format_character_value_change( "Strength 4", "",
            "Strength cannot be further decreased" ) ==
           "Strength 4. Strength cannot be further decreased." );
    CHECK( accessibility::format_character_value_change( "Level 2", "3 points over budget", "" ) ==
           "Level 2, 3 points over budget." );

    accessibility::character_trait_row trait;
    trait.name = "Fast Learner";
    trait.points = 3;
    trait.selected = true;
    CHECK( accessibility::format_character_trait_row( trait ) == "Fast Learner, 3 points, selected." );
    trait.locked_by = "profession";
    CHECK( accessibility::format_character_trait_row( trait ) ==
           "Fast Learner, 3 points, selected, locked by profession." );
    trait = accessibility::character_trait_row();
    trait.name = "Slow Learner";
    trait.points = -3;
    trait.conflicts = { "Fast Learner" };
    CHECK( accessibility::format_character_trait_row( trait ) ==
           "Slow Learner, earns 3 points, conflicts with Fast Learner." );
    trait.conflicts.clear();
    trait.forbidden_by = "scenario";
    trait.show_points = false;
    CHECK( accessibility::format_character_trait_row( trait ) ==
           "Slow Learner, forbidden by scenario." );

    CHECK( accessibility::format_character_skill_row( "Melee", 0, 0 ) == "Melee, level 0." );
    CHECK( accessibility::format_character_skill_row( "Melee", 3, 1 ) ==
           "Melee, level 3, 1 from profession." );

    CHECK( accessibility::format_character_description_field( "Name", "Ada" ) == "Name: Ada." );
    CHECK( accessibility::format_blood_type( "AB", false ) == "AB negative" );
    CHECK( accessibility::format_blood_type( "O", true ) == "O positive" );
    CHECK( accessibility::format_character_start_location( "Shelter", 3 ) == "Shelter, 3 variants" );
    CHECK( accessibility::format_character_start_location( "Shelter", 1 ) == "Shelter" );
    CHECK( accessibility::format_character_description_field( "Starting location",
            accessibility::format_character_start_location( "Random location", 5 ) ) ==
           "Starting location: Random location, 5 variants." );
}

TEST_CASE( "world_and_mod_formatters_speak_rows_changes_and_summaries",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_world_row( "Survival", 0 ) == "Survival, no characters" );
    CHECK( accessibility::format_world_row( "Survival", 1 ) == "Survival, 1 character" );
    CHECK( accessibility::format_world_row( "Survival", 3 ) == "Survival, 3 characters" );
    CHECK( accessibility::format_list_open( "World selection", 4, "Survival, 3 characters" ) ==
           "World selection, 4 items. Survival, 3 characters." );
    CHECK( accessibility::format_list_open( "Mod Load Order", 0, "ignored" ) ==
           "Mod Load Order, No items." );
    CHECK( accessibility::format_world_setting( "World name", "Survival" ) == "World name: Survival" );
    CHECK( accessibility::format_world_creation_summary( 3, 8, {
        "World name: Survival", "Population density: High", "Monster evolution: Custom"
    }, true, 2 ) == "Create World, 3 of 8. World name: Survival, Population density: High, "
    "Monster evolution: Custom. Advanced options customized. 2 mods in load order." );

    accessibility::world_mod_row row;
    row.name = "Magiclysm";
    row.category = "Content";
    CHECK( accessibility::format_world_mod_row( row ) == "Magiclysm, Content." );
    row.active = true;
    CHECK( accessibility::format_world_mod_row( row ) == "Magiclysm, active, Content." );
    row = accessibility::world_mod_row();
    row.name = "Backrooms";
    row.category = "Content";
    row.incompatible = true;
    row.unavailable = "missing dependency extra";
    CHECK( accessibility::format_world_mod_row( row ) ==
           "Backrooms, incompatible with an active mod, Unavailable: missing dependency extra, Content." );
    row = accessibility::world_mod_row();
    row.name = "old_mod";
    row.category = "MISSING MODS";
    row.missing = true;
    CHECK( accessibility::format_world_mod_row( row ) == "old_mod, not installed, MISSING MODS." );
    row.missing = false;
    row.obsolete = true;
    row.category = "OBSOLETE MODS";
    CHECK( accessibility::format_world_mod_row( row ) == "old_mod, obsolete, OBSOLETE MODS." );

    CHECK( accessibility::format_mod_list_pane( "Mod List", "Default" ) == "Mod List: Default" );
    CHECK( accessibility::format_mod_added( "Magiclysm", 2, 2, { "Dark Days Ahead" }, {} ) ==
           "Added Magiclysm, position 2 of 2. Also added: Dark Days Ahead." );
    CHECK( accessibility::format_mod_added( "Aftershock", 3, 3, {}, {} ) ==
           "Added Aftershock, position 3 of 3." );
    CHECK( accessibility::format_mod_added( "Other core", 1, 1, {}, { "Dark Days Ahead", "Magiclysm" } )
           ==
           "Added Other core, position 1 of 1. Also removed: Dark Days Ahead and Magiclysm." );
    CHECK( accessibility::format_mod_removed( "Dark Days Ahead", 0, { "Magiclysm" } ) ==
           "Removed Dark Days Ahead, 0 mods in load order. Also removed: Magiclysm." );
    CHECK( accessibility::format_mod_removed( "Magiclysm", 1, {} ) ==
           "Removed Magiclysm, 1 mod in load order." );
    CHECK( accessibility::format_mod_not_added( "Magiclysm", "already in the load order" ) ==
           "Cannot add Magiclysm: already in the load order." );
    CHECK( accessibility::format_mod_moved( 2, 3 ) == "Position 2 of 3." );
    CHECK( accessibility::format_mod_not_moved( true, "Dark Days Ahead loads first" ) ==
           "Cannot move up: Dark Days Ahead loads first." );
    CHECK( accessibility::format_mod_not_moved( false, "already last" ) ==
           "Cannot move down: already last." );
    CHECK( accessibility::format_mod_manager_summary( "Mod List: Default", 4, 40, 2, "" ) ==
           "Mod List: Default, 4 of 40. 2 mods in load order." );
    CHECK( accessibility::format_mod_manager_summary( "Mod List: Balance", 0, 0, 1, "magic" ) ==
           "Mod List: Balance, No items. 1 mod in load order. Filter: magic." );
}

TEST_CASE( "options_rule_and_panel_formatters_speak_rows_changes_and_summaries",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_screen_page( "Options", "General" ) == "Options: General" );
    CHECK( accessibility::format_screen_page( "Pickup rules for Ada", "" ) == "Pickup rules for Ada" );
    CHECK( accessibility::format_list_change( "Rule added", 3 ) == "Rule added, 3 items" );
    CHECK( accessibility::format_list_change( "Rule removed", 0 ) == "Rule removed, No items" );
    CHECK( accessibility::format_shortcut_label( "[<Global>]" ) == "Global" );
    CHECK( accessibility::format_shortcut_label( "[<Character>]" ) == "Character" );
    CHECK( accessibility::format_shortcut_label( "Ada" ) == "Ada" );

    // Options: rows, group state, value changes, tooltip, and summary.
    CHECK( accessibility::format_option_row( "Use tiles", "True", "" ) == "Use tiles: True" );
    CHECK( accessibility::format_option_row( "Font blending", "False", "Use tiles" ) ==
           "Font blending: False, Unavailable: requires Use tiles" );
    CHECK( accessibility::format_option_group_row( "Graphics", false, 12 ) ==
           "Graphics, collapsed, 12 options" );
    CHECK( accessibility::format_option_group_row( "Sound", true, 1 ) == "Sound, expanded, 1 option" );
    CHECK( accessibility::format_option_group_toggled( true, 12 ) == "Expanded, 12 options" );
    CHECK( accessibility::format_option_group_toggled( false, 12 ) == "Collapsed" );
    CHECK( accessibility::format_list_open( "Options: General", 4,
                                            accessibility::format_option_row( "Use tiles", "True", "" ) ) ==
           "Options: General, 4 items. Use tiles: True." );
    CHECK( accessibility::format_option_tooltip( "Show tiles. #Default: True", "" ) ==
           "Show tiles. Default: True." );
    CHECK( accessibility::format_option_tooltip( "World size #Default: 5 - Min: 1, Max: 10",
            "Note: Some of these options may produce unexpected results if changed." ) ==
           "World size. Default: 5 - Min: 1, Max: 10. "
           "Note: Some of these options may produce unexpected results if changed." );
    CHECK( accessibility::format_options_summary( "Options: Graphics", 3, 20, 3, 6 ) ==
           "Options: Graphics, 3 of 20. Page 3 of 6." );
    CHECK( accessibility::format_options_summary( "World Options", 1, 8, 4, 1 ) ==
           "World Options, 1 of 8." );

    // Safe mode rules: columns are spoken as words; unused columns are left out of the row.
    accessibility::safemode_rule_row rule;
    rule.rule = "zombie";
    rule.attitude = "Hostile";
    rule.proximity = 5;
    rule.mode = "Both";
    CHECK( accessibility::format_safemode_rule_row( rule ) ==
           "zombie, Blacklist, Category: Hostile, Attitude: Hostile, Distance: 5, Mode: Both." );
    rule.whitelist = true;
    rule.active = false;
    CHECK( accessibility::format_safemode_rule_row( rule ) ==
           "zombie, Whitelist, Category: Hostile, Attitude: Hostile, Mode: Both, disabled." );
    CHECK( accessibility::format_safemode_rule_fields( rule ) == std::vector<std::string> {
        "Rule: zombie", "Attitude: Hostile", "Distance: not applicable", "Whitelist",
        "Category: Hostile", "Mode: Both"
    } );
    rule = accessibility::safemode_rule_row();
    rule.sound = true;
    rule.whitelist = true;
    rule.attitude = "Hostile";
    rule.proximity = 0;
    rule.mode = "Driving";
    CHECK( accessibility::format_safemode_rule_row( rule ) ==
           "empty rule, Whitelist, Category: Sound, Distance: 0, Mode: Driving." );
    CHECK( accessibility::format_safemode_rule_values( rule ) == std::vector<std::string> {
        "empty rule", "not applicable", "0", "Whitelist", "Sound", "Driving"
    } );

    // Autopickup rules and the shared rule manager summary.
    CHECK( accessibility::format_autopickup_rule_row( "wooden arrow", false, true ) ==
           "wooden arrow, Include." );
    CHECK( accessibility::format_autopickup_rule_row( "*", true, false ) == "*, Exclude, disabled." );
    CHECK( accessibility::format_autopickup_rule_row( "", false, true ) == "empty rule, Include." );
    CHECK( accessibility::format_rule_manager_summary( "Safe mode manager: Global", 2, 5,
            "Safe mode enabled", "Rule" ) ==
           "Safe mode manager: Global, 2 of 5. Safe mode enabled. Column: Rule." );
    CHECK( accessibility::format_rule_manager_summary( "Auto pickup manager: Character", 1, 0,
            "Auto pickup disabled", "" ) ==
           "Auto pickup manager: Character, No items. Auto pickup disabled." );
    CHECK( accessibility::format_mod_moved( 2, 5 ) == "Position 2 of 5." );

    // Sidebar panels and layouts.
    CHECK( accessibility::format_panel_row( "Stats", true ) == "Stats, shown" );
    CHECK( accessibility::format_panel_row( "Location", false ) == "Location, hidden" );
    CHECK( accessibility::format_layout_row( "Classic", true ) == "Classic, active" );
    CHECK( accessibility::format_layout_row( "Labels", false ) == "Labels" );
    CHECK( accessibility::format_list_open( "Panels", 20, "Stats, shown" ) ==
           "Panels, 20 items. Stats, shown." );
    CHECK( accessibility::format_panel_move_start( "Stats", 3, 20 ) ==
           "Moving Stats, position 3 of 20" );
    CHECK( accessibility::format_panel_moved( "Stats", 5, 20 ) ==
           "Moved Stats to position 5 of 20." );
}

// NOLINTEND(cata-text-style)
