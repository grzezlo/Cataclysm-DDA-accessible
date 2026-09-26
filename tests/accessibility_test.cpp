#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "accessibility.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "game.h"
#include "help.h"
#include "input_context.h"
#include "input_enums.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "map_item_stack.h"
#include "player_helpers.h"
#include "point.h"
#include "type_id.h"

namespace
{

class capture_backend final : public accessibility::backend
{
    public:
        bool is_available() const override {
            return true;
        }

        bool announce( const std::string &message ) override {
            messages.push_back( message );
            return true;
        }

        std::vector<std::string> messages;
};

} // namespace

TEST_CASE( "accessibility_alt_bindings", "[accessibility]" )
{
    input_context gameplay( "DEFAULTMODE", keyboard_mode::keycode );
    gameplay.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    gameplay.register_action( "accessibility_situation" );
    gameplay.register_action( "accessibility_status" );

    const input_event alt_r( { keymod_t::alt }, 'r', input_event_t::keyboard_code );
    const input_event alt_v( { keymod_t::alt }, 'v', input_event_t::keyboard_code );
    const input_event alt_s( { keymod_t::alt }, 's', input_event_t::keyboard_code );
    const input_event alt_r_char( { keymod_t::alt }, 'r', input_event_t::keyboard_char );
    const input_event alt_v_char( { keymod_t::alt }, 'v', input_event_t::keyboard_char );
    const input_event alt_s_char( { keymod_t::alt }, 's', input_event_t::keyboard_char );

    CHECK( gameplay.input_to_action( alt_r ) == "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( gameplay.input_to_action( alt_v ) == "accessibility_situation" );
    CHECK( gameplay.input_to_action( alt_s ) == "accessibility_status" );
    CHECK( gameplay.input_to_action( alt_r_char ) == "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( gameplay.input_to_action( alt_v_char ) == "accessibility_situation" );
    CHECK( gameplay.input_to_action( alt_s_char ) == "accessibility_status" );

    const input_event altgr_r( { keymod_t::ctrl, keymod_t::alt }, 'r', input_event_t::keyboard_code );
    const input_event altgr_v( { keymod_t::ctrl, keymod_t::alt }, 'v', input_event_t::keyboard_code );
    const input_event altgr_s( { keymod_t::ctrl, keymod_t::alt }, 's', input_event_t::keyboard_code );
    const input_event altgr_r_char( { keymod_t::ctrl, keymod_t::alt }, 'r',
                                    input_event_t::keyboard_char );
    const input_event altgr_v_char( { keymod_t::ctrl, keymod_t::alt }, 'v',
                                    input_event_t::keyboard_char );
    const input_event altgr_s_char( { keymod_t::ctrl, keymod_t::alt }, 's',
                                    input_event_t::keyboard_char );

    CHECK( gameplay.input_to_action( altgr_r ) != "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( gameplay.input_to_action( altgr_v ) != "accessibility_situation" );
    CHECK( gameplay.input_to_action( altgr_s ) != "accessibility_status" );
    CHECK( gameplay.input_to_action( altgr_r_char ) != "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( gameplay.input_to_action( altgr_v_char ) != "accessibility_situation" );
    CHECK( gameplay.input_to_action( altgr_s_char ) != "accessibility_status" );

    const input_event pure_r( {}, 'r', input_event_t::keyboard_char );
    const input_event pure_v( {}, 'v', input_event_t::keyboard_char );
    const input_event pure_s( {}, 's', input_event_t::keyboard_char );
    CHECK( gameplay.input_to_action( pure_r ) != "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( gameplay.input_to_action( pure_v ) != "accessibility_situation" );
    CHECK( gameplay.input_to_action( pure_s ) != "accessibility_status" );

    input_context message_log( "MESSAGE_LOG", keyboard_mode::keycode );
    message_log.register_action( "FILTER" );
    message_log.register_action( "RESET_FILTER" );
    message_log.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( message_log.input_to_action( input_event( {}, 'f', input_event_t::keyboard_code ) ) ==
           "FILTER" );
    CHECK( message_log.input_to_action( input_event( {}, 'r', input_event_t::keyboard_code ) ) ==
           "RESET_FILTER" );
    CHECK( message_log.input_to_action( alt_r ) == "ACCESSIBILITY_REPEAT_LAST" );

    input_context list( "UILIST", keyboard_mode::keycode );
    list.register_action( "ACCESSIBILITY_DETAILS" );
    const input_event alt_d( { keymod_t::alt }, 'd', input_event_t::keyboard_code );
    const input_event alt_d_char( { keymod_t::alt }, 'd', input_event_t::keyboard_char );
    const input_event altgr_d( { keymod_t::ctrl, keymod_t::alt }, 'd',
                               input_event_t::keyboard_code );
    CHECK( list.input_to_action( alt_d ) == "ACCESSIBILITY_DETAILS" );
    CHECK( list.input_to_action( alt_d_char ) == "ACCESSIBILITY_DETAILS" );
    CHECK( list.input_to_action( altgr_d ) != "ACCESSIBILITY_DETAILS" );
    CHECK( gameplay.input_to_action( alt_d ) != "ACCESSIBILITY_DETAILS" );

    input_context yes_no( "YESNO", keyboard_mode::keycode );
    yes_no.register_action( "YES" );
    yes_no.register_action( "NO" );
    yes_no.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( yes_no.input_to_action( alt_r ) == "ACCESSIBILITY_REPEAT_LAST" );
}

TEST_CASE( "accessibility_announcements_can_be_captured_and_repeated", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    CHECK( accessibility::is_available() );
    CHECK_FALSE( accessibility::announce( "" ) );
    CHECK( capture_ptr->messages.empty() );

    CHECK( accessibility::announce( "Main menu" ) );
    CHECK( accessibility::announce_focus( "Custom character", "New game" ) );
    CHECK( accessibility::announce_status( "Loading" ) );
    CHECK( accessibility::last_announcement() == "Loading" );
    CHECK_FALSE( accessibility::announce_game_message( "Hidden during setup" ) );
    accessibility::enable_game_messages( true );
    CHECK( accessibility::announce_game_message( "You hear a noise." ) );
    CHECK( accessibility::announce_movement( "northwest", "wooden floor" ) );
    CHECK( accessibility::announce_movement( "northwest", "grass" ) );
    CHECK( accessibility::announce_movement( "east", "grass" ) );
    CHECK_FALSE( accessibility::announce_threats( {}, "No nearby threats." ) );
    CHECK( accessibility::announce_threats( { "monster-1" },
                                            "Nearby threats: zombie, 5 tiles north." ) );
    CHECK_FALSE( accessibility::announce_threats( { "monster-1" }, "This must be deduplicated." ) );
    CHECK( accessibility::announce_threats( { "monster-2" }, "A new nearby threat." ) );
    CHECK_FALSE( accessibility::announce_threats( { "monster-2" }, "This must be deduplicated too." ) );
    CHECK( accessibility::announce_threats( {}, "No nearby threats." ) );
    CHECK( accessibility::last_announcement() == "No nearby threats." );
    REQUIRE( accessibility::repeat_last_announcement() );

    REQUIRE( capture_ptr->messages.size() == 11 );
    CHECK( capture_ptr->messages[0] == "Main menu" );
    CHECK( capture_ptr->messages[1] == "New game. Custom character" );
    CHECK( capture_ptr->messages[2] == "Loading" );
    CHECK( capture_ptr->messages[3] == "You hear a noise." );
    CHECK( capture_ptr->messages[4] == "wooden floor. northwest" );
    CHECK( capture_ptr->messages[5] == "grass" );
    CHECK( capture_ptr->messages[6] == "grass. east" );
    CHECK( capture_ptr->messages[7] == "Nearby threats: zombie, 5 tiles north." );
    CHECK( capture_ptr->messages[8] == "A new nearby threat." );
    CHECK( capture_ptr->messages[9] == "No nearby threats." );
    CHECK( capture_ptr->messages[10] == "No nearby threats." );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "message_log_review_keeps_complete_messages_and_filter_focus",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    accessibility::message_log_review log;
    log.set_entries( {
        { 0, "A zombie approaches.", "warning", "Warning", "Now" },
        { 1, "You hear a crash x 3", "info", "Information", "2 minutes ago" },
        { 2, "You close the door.", "neutral", "", "5 minutes ago" }
    }, true );
    const std::vector<std::string> types = { "warning", "info", "neutral" };
    REQUIRE( log.set_filter( "", types ) );
    REQUIRE( log.focused() != nullptr );
    accessibility::announce( accessibility::format_message_log_open( log.matches().size(),
                             log.focused() ) );
    CHECK( log.move( 1 ) );
    REQUIRE( log.focused() != nullptr );
    accessibility::announce( accessibility::format_message_log_focus( *log.focused() ) );
    CHECK( log.page( 1, 2 ) );
    REQUIRE( log.focused() != nullptr );
    accessibility::announce( accessibility::format_message_log_focus( *log.focused() ) );
    CHECK( log.focused_position() == 3 );
    accessibility::announce( accessibility::format_message_log_detail( *log.focused(),
                             log.focused_position(), log.matches().size() ) );
    CHECK_FALSE( log.move( 1 ) );
    CHECK( accessibility::repeat_last_announcement() );

    accessibility::announce( accessibility::format_message_log_filter_field( "",
    { "warning", "info", "neutral" } ) );
    accessibility::announce( accessibility::format_message_log_filter_value( "warning:z" ) );
    REQUIRE( log.set_filter( "warning:z", types ) );
    CHECK( log.matches().size() == 1 );
    REQUIRE( log.focused() != nullptr );
    CHECK( accessibility::format_message_log_detail( *log.focused(), log.focused_position(),
            log.matches().size() ) == "Message 1 of 1. Warning. A zombie approaches. Now." );
    accessibility::announce( accessibility::format_message_log_filter( "warning:z",
                             log.matches().size(), log.focused() ) );
    REQUIRE( log.set_filter( "warning:nothing", types ) );
    CHECK( log.focused() == nullptr );
    accessibility::announce( accessibility::format_message_log_filter( "warning:nothing",
                             log.matches().size(), log.focused() ) );
    CHECK_FALSE( log.set_filter( "unknown:zombie", types ) );
    CHECK( log.matches().empty() );
    accessibility::announce( accessibility::format_message_log_invalid_type( "unknown" ) );

    REQUIRE( log.set_filter( "", types ) );
    CHECK( log.focus_source( 2 ) );
    accessibility::announce( accessibility::format_message_log_filter( "", log.matches().size(),
                             log.focused() ) );
    accessibility::announce( "Gameplay. Current position: floor" );

    REQUIRE( capture_ptr->messages.size() == 12 );
    CHECK( capture_ptr->messages[0] ==
           "Message log. 3 messages. A zombie approaches. Now." );
    CHECK( capture_ptr->messages[1] ==
           "You hear a crash x 3. 2 minutes ago." );
    CHECK( capture_ptr->messages[2] == "You close the door. 5 minutes ago." );
    CHECK( capture_ptr->messages[3] ==
           "Message 3 of 3. You close the door. 5 minutes ago." );
    CHECK( capture_ptr->messages[4] == capture_ptr->messages[3] );
    CHECK( capture_ptr->messages[5] ==
           "Message log filter. Empty. Search text or use type:text. Types: warning, info, neutral. Enter to apply, Escape to cancel." );
    CHECK( capture_ptr->messages[6] == "Filter text: warning:z" );
    CHECK( capture_ptr->messages[7] ==
           "Filter: warning:z. 1 message. A zombie approaches. Now." );
    CHECK( capture_ptr->messages[8] ==
           "Filter: warning:nothing. 0 messages. No matching messages." );
    CHECK( capture_ptr->messages[9] == "Unknown message type: unknown" );
    CHECK( capture_ptr->messages[10] ==
           "Filter cleared. 3 messages. You close the door. 5 minutes ago." );
    CHECK( capture_ptr->messages[11] == "Gameplay. Current position: floor" );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "message_log_review_handles_oldest_first_and_empty_history", "[accessibility][nogame]" )
{
    accessibility::message_log_review log;
    log.set_entries( {}, false );
    CHECK( log.focused() == nullptr );
    CHECK_FALSE( log.move( 1 ) );
    CHECK( accessibility::format_message_log_open( 0, log.focused() ) ==
           "Message log. 0 messages. Empty." );

    log.set_entries( {
        { 2, "Oldest", "neutral", "", "1 hour ago" },
        { 1, "Middle", "info", "Information", "5 minutes ago" },
        { 0, "Newest", "warning", "Warning", "Now" }
    }, false );
    REQUIRE( log.focused() != nullptr );
    CHECK( log.focused()->source_index == 0 );
    CHECK( log.page( -1, 2 ) );
    REQUIRE( log.focused() != nullptr );
    CHECK( log.focused()->source_index == 2 );
    REQUIRE( log.set_filter( ":Middle", { "neutral", "info", "warning" } ) );
    REQUIRE( log.focused() != nullptr );
    CHECK( log.focused()->source_index == 1 );
    REQUIRE( log.set_filter( "", { "neutral", "info", "warning" } ) );
    REQUIRE( log.focused() != nullptr );
    CHECK( log.focused()->source_index == 1 );
}

TEST_CASE( "accessibility_last_announcement_can_be_set_quietly_and_repeated",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    accessibility::announce_status( "Loading the save…" );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0] == "Loading the save…" );

    // Quietly update progress during loading - no speech should be emitted
    accessibility::set_last_announcement( "Loading the save… Core game data" );
    CHECK( capture_ptr->messages.size() == 1 );
    CHECK( accessibility::last_announcement() == "Loading the save… Core game data" );

    accessibility::set_last_announcement( "Loading the save… Overmap" );
    CHECK( capture_ptr->messages.size() == 1 );
    CHECK( accessibility::last_announcement() == "Loading the save… Overmap" );

    // Repeating speaks the latest quiet progress update on demand
    REQUIRE( accessibility::repeat_last_announcement() );
    REQUIRE( capture_ptr->messages.size() == 2 );
    CHECK( capture_ptr->messages[1] == "Loading the save… Overmap" );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "semantic_accessibility_lists_have_stable_distance_ordering",
           "[accessibility][nogame]" )
{
    std::vector<accessibility::semantic_list_entry> entries = {
        { 2, 0, 0, 2, 0, "far east", "2 tiles east" },
        { 1, 0, 1, 0, 1, "south item", "1 tile south" },
        { 1, 0, -1, 0, 2, "north obstacle", "1 tile north" },
        { 0, 0, 0, 0, 0, "current position", "here" },
        { 1, 0, -1, 0, 1, "north hazard", "1 tile north" },
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

TEST_CASE( "uilist_announces_details_once_and_speaks_short_descriptions_in_place",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    const std::string disabled = accessibility::format_uilist_focus(
                                     "Torso", "Bandaged", true );
    CHECK( disabled == "Torso. Bandaged. Unavailable." );
    const std::string open = accessibility::format_uilist_open(
                                 "Treat which part?", disabled, "Alt+D", true );
    CHECK( open == "Treat which part? List. Use arrow keys to choose. Alt+D for details. " +
           disabled );
    CHECK( accessibility::announce( open ) );
    const std::string short_details = "Light exercise comparable in intensity to walking.";
    CHECK_FALSE( accessibility::uilist_details_need_document( short_details ) );
    CHECK( accessibility::announce( short_details ) );
    CHECK( accessibility::repeat_last_announcement() );
    REQUIRE( capture_ptr->messages.size() == 3 );
    CHECK( capture_ptr->messages[2] == short_details );

    const std::string plain = accessibility::format_uilist_focus( "Moderate", "", false );
    CHECK( plain == "Moderate." );
    CHECK( accessibility::format_uilist_open( "Choose intensity", plain, "Alt+D", false ) ==
           "Choose intensity. List. Use arrow keys to choose. Moderate." );
    CHECK( accessibility::uilist_details_need_document( std::string( 161, 'x' ) ) );
    CHECK_FALSE( accessibility::uilist_details_need_document( std::string( 160, 'x' ) ) );
    CHECK_FALSE( accessibility::uilist_details_need_document( std::string( 100, 'x' ) + "ó" ) );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "long_list_details_follow_sentences_across_visual_lines", "[accessibility][nogame]" )
{
    const auto firewood = accessibility::split_uilist_detail_sentences(
                              "Source for firewood or other flammable materials in this zone may be used "
                              "to automatically refuel fires.  This will be done to maintain light "
                              "during long-running tasks such as crafting, reading or waiting." );
    REQUIRE( firewood.size() == 2 );
    CHECK( firewood[0] == "Source for firewood or other flammable materials in this zone may "
           "be used to automatically refuel fires." );
    CHECK( firewood[1] == "This will be done to maintain light during long-running "
           "tasks such as crafting, reading or waiting." );

    const auto wrapped = accessibility::split_uilist_detail_sentences(
                             "This sentence spans a visual\nline but stays together. Next sentence." );
    REQUIRE( wrapped.size() == 2 );
    CHECK( wrapped[0] == "This sentence spans a visual line but stays together." );
    const auto numeric = accessibility::split_uilist_detail_sentences(
                             "Wait 1.5 seconds. Then continue." );
    REQUIRE( numeric.size() == 2 );
    CHECK( numeric[0] == "Wait 1.5 seconds." );
}

TEST_CASE( "yes_no_confirmation_announces_default_focus_and_repeats_context",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    const std::string question = "Stop exercising?";
    const std::string opened = accessibility::format_yes_no_prompt(
                                   question, "No", "Left arrow", "Right arrow", "Enter", true );
    CHECK( opened == "Stop exercising? No selected. Choose with Left arrow or Right arrow; "
           "confirm with Enter." );
    CHECK( accessibility::announce( opened ) );
    CHECK( accessibility::format_yes_no_focus( "Yes" ) == "Yes selected" );
    CHECK( accessibility::announce( accessibility::format_yes_no_focus( "Yes" ) ) );
    const std::string context = accessibility::format_yes_no_prompt(
                                    question, "Yes", "", "", "", false );
    CHECK( context == "Stop exercising? Yes selected." );
    accessibility::set_last_announcement( context );
    CHECK( accessibility::repeat_last_announcement() );
    REQUIRE( capture_ptr->messages.size() == 3 );
    CHECK( capture_ptr->messages.back() == context );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "main_menu_information_follows_focus_and_source_lines", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    accessibility::announce( accessibility::format_main_menu_focus(
                                 "Main menu", "New Game", "Custom Character. Customize the character",
                                 "Cataclysm: Dark Days Ahead", "Version: 0.I",
                                 "Tip of the day: Find shelter. Bugs? Use MOTD to report them" ) );
    accessibility::announce( accessibility::format_main_menu_focus(
                                 "", "", "Preset Character. Choose a template" ) );
    accessibility::announce( accessibility::format_main_menu_focus(
                                 "", "Tip of the day", "Find shelter. Bugs? Use MOTD to report them" ) );
    accessibility::announce( accessibility::format_main_menu_focus(
                                 "", "Load", accessibility::format_main_menu_world( "Empty", 0 ) ) );
    accessibility::announce( accessibility::format_main_menu_focus(
                                 "", "World", accessibility::format_main_menu_world( "Survival", 3 ) ) );
    accessibility::announce( accessibility::format_main_menu_focus(
                                 "", "Load", "No worlds to load" ) );

    const std::string long_line( 240, 'x' );
    const std::vector<accessibility::semantic_document_line> motd =
        accessibility::select_document_lines( { "", "*******", "First MOTD line", "  ",
                long_line, "Last MOTD line" } );
    REQUIRE( motd.size() == 3 );
    CHECK( motd[0].source_index == 2 );
    CHECK( motd[1].source_index == 4 );
    CHECK( motd[1].text == long_line );
    CHECK( motd[2].source_index == 5 );

    accessibility::announce( accessibility::format_main_menu_focus( "", "MOTD", motd[0].text ) );
    const int second = accessibility::move_document_cursor( 0, motd.size(), 1 );
    CHECK( second == 1 );
    accessibility::announce( motd[second].text );
    CHECK( accessibility::move_document_cursor( second, motd.size(), 10 ) == 2 );
    CHECK( accessibility::move_document_cursor( 0, motd.size(), -1 ) == 0 );
    CHECK( accessibility::move_document_cursor( 0, 0, 1 ) == 0 );
    CHECK( accessibility::repeat_last_announcement() );

    const auto credits = accessibility::select_document_lines( { "Author: Ada", "Author: Bea" } );
    accessibility::announce( accessibility::format_main_menu_focus( "", "Credits", credits[0].text ) );
    accessibility::announce( accessibility::format_main_menu_focus(
                                 "Main menu", "World", accessibility::format_main_menu_world( "Survival", 3 ) ) );

    REQUIRE( capture_ptr->messages.size() == 11 );
    CHECK( capture_ptr->messages[0] == "Cataclysm: Dark Days Ahead. Version: 0.I. "
           "Tip of the day: Find shelter. Bugs? Use MOTD to report them. "
           "Main menu. New Game. Custom Character. "
           "Customize the character." );
    CHECK( capture_ptr->messages[1] == "Preset Character. Choose a template." );
    CHECK( capture_ptr->messages[2] ==
           "Tip of the day. Find shelter. Bugs? Use MOTD to report them." );
    CHECK( capture_ptr->messages[3] == "Load. Empty. 0 saves." );
    CHECK( capture_ptr->messages[4] == "World. Survival. 3 saves." );
    CHECK( capture_ptr->messages[5] == "Load. No worlds to load." );
    CHECK( capture_ptr->messages[6] == "MOTD. First MOTD line." );
    CHECK( capture_ptr->messages[7] == long_line );
    CHECK( capture_ptr->messages[8] == long_line );
    CHECK( capture_ptr->messages[9] == "Credits. Author: Ada." );
    CHECK( capture_ptr->messages[10] == "Main menu. World. Survival. 3 saves." );

    accessibility::reset_backend_after_testing();
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
           "aspirin. Count: 6. Location 2 of 3. 4 tiles northeast. Category: drugs. "
           "At this location: 2. High priority. 5 of 9." );
}

TEST_CASE( "nearby_list_focus_announcements_cover_navigation_and_context_restoration",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    accessibility::nearby_list_entry first_item = {
        "aspirin", 2, 1, 2, "2 tiles east", { "Category: drugs" }, 1, 2
    };
    accessibility::nearby_list_entry second_item = {
        "flashlight", 1, 0, 0, "3 tiles south", { "Category: tools" }, 2, 2
    };
    accessibility::nearby_list_entry creature = {
        "zombie", 0, 0, 0, "5 tiles northwest",
        { "Attitude: Hostile", "Behavior: Tracking", "Health: lightly injured" }, 1, 1
    };

    CHECK( accessibility::announce_nearby_list_focus(
               first_item, accessibility::format_nearby_list_context( "Nearby items", 2 ) ) );
    CHECK( accessibility::announce_nearby_list_focus( second_item ) );

    first_item.position_index = 2;
    first_item.location = "4 tiles northeast";
    CHECK( accessibility::announce_nearby_list_focus( first_item ) );

    CHECK( accessibility::announce_nearby_list_focus(
               first_item, accessibility::format_nearby_list_context(
                   "Nearby items", 1, "Filter: asp" ) ) );
    CHECK( accessibility::announce_nearby_list_focus(
               first_item, accessibility::format_nearby_list_context(
                   "Nearby items", 1, "Sorted by name" ) ) );
    CHECK( accessibility::announce_nearby_list_focus(
               creature, accessibility::format_nearby_list_context( "Nearby creatures", 1 ) ) );

    // Returning from look or detail deliberately restores the same focused row.
    CHECK( accessibility::announce_nearby_list_focus( creature ) );
    CHECK( accessibility::announce_focus( {}, "Gameplay. Current position: grass" ) );

    REQUIRE( capture_ptr->messages.size() == 8 );
    CHECK( capture_ptr->messages[0] ==
           "Nearby items. 2 results. aspirin. Count: 2. Location 1 of 2. 2 tiles east. "
           "Category: drugs. 1 of 2." );
    CHECK( capture_ptr->messages[1] ==
           "flashlight. Count: 1. 3 tiles south. Category: tools. 2 of 2." );
    CHECK( capture_ptr->messages[2].find( "Location 2 of 2" ) != std::string::npos );
    CHECK( capture_ptr->messages[3].find( "Filter: asp" ) != std::string::npos );
    CHECK( capture_ptr->messages[4].find( "Sorted by name" ) != std::string::npos );
    CHECK( capture_ptr->messages[5] ==
           "Nearby creatures. 1 result. zombie. 5 tiles northwest. Attitude: Hostile. "
           "Behavior: Tracking. Health: lightly injured. 1 of 1." );
    CHECK( capture_ptr->messages[6] ==
           "zombie. 5 tiles northwest. Attitude: Hostile. Behavior: Tracking. "
           "Health: lightly injured. 1 of 1." );
    CHECK( capture_ptr->messages[7] == "Gameplay. Current position: grass" );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "nearby_item_collection_groups_visible_positions_and_respects_blindness",
           "[accessibility]" )
{
    clear_avatar();
    clear_map();
    const time_point previous_time = calendar::turn;
    const on_out_of_scope cleanup( [previous_time]() {
        clear_avatar();
        clear_map();
        set_time( previous_time );
    } );

    set_time_to_day();
    avatar &viewer = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = viewer.pos_bub( here );
    const itype_id rock_type( "rock" );
    const item rock( rock_type, calendar::turn );
    here.add_item_or_charges( origin + tripoint::east, rock );
    here.add_item_or_charges( origin + tripoint::south, rock );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );

    const std::vector<map_item_stack> visible_items = g->find_nearby_items( 10 );
    const auto rock_stack = std::find_if( visible_items.begin(), visible_items.end(),
    [&]( const map_item_stack & stack ) {
        return stack.example != nullptr && stack.example->typeId() == rock_type;
    } );
    REQUIRE( rock_stack != visible_items.end() );
    CHECK( rock_stack->totalcount == 2 );
    CHECK( rock_stack->vIG.size() == 2 );

    player_wear_blindfold();
    REQUIRE( viewer.is_blind() );
    CHECK( g->find_nearby_items( 10 ).empty() );
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
        { 3, 0, 0, 3, 1, "wall", "3 tiles east" },
        { 1, 0, 0, 1, 1, "wall", "1 tile east" },
        { 2, 0, 1, 2, 1, "wall", "2 tiles southeast" },
        { 5, 0, 0, -5, 1, "wall", "5 tiles west" },
        { 2, 0, 0, 2, 2, "deep water", "2 tiles east" },
        { 3, 0, 1, 3, 2, "deep water", "3 tiles southeast" },
        { 4, 1, 1, 3, 1, "wall", "above" },
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

TEST_CASE( "accessibility_urgent_events_are_combined_and_reset_between_games",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    accessibility::reset_game_state();

    accessibility::condition_tracker normal;
    normal.hunger_tier = "Satisfied";
    normal.thirst_tier = "Hydrated";
    normal.sleepiness_tier = "Rested";
    normal.pain_tier = "No pain";
    normal.weariness_tier = "Fresh";

    CHECK_FALSE( accessibility::evaluate_threat_transition( {}, "Nothing" ) );
    CHECK( accessibility::evaluate_condition_transitions( normal ).empty() );

    const std::optional<std::string> threat = accessibility::evaluate_threat_transition(
    { "monster-1" }, "Zombie spotted!" );
    REQUIRE( threat );

    accessibility::condition_tracker hungry = normal;
    hungry.hunger_tier = "Hungry";
    std::vector<std::string> alerts = { *threat };
    const std::vector<std::string> condition_alerts =
        accessibility::evaluate_condition_transitions( hungry );
    alerts.insert( alerts.end(), condition_alerts.begin(), condition_alerts.end() );

    REQUIRE( accessibility::announce_semantic_sequence( alerts ) );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0] == "Zombie spotted! Hunger: Hungry." );
    CHECK( accessibility::last_announcement() == capture_ptr->messages[0] );

    accessibility::reset_game_state();
    CHECK( accessibility::evaluate_condition_transitions( hungry ).empty() );
    CHECK( accessibility::evaluate_threat_transition( { "monster-1" }, "Zombie spotted!" ) );

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

TEST_CASE( "help_topic_and_document_formatting", "[accessibility][nogame]" )
{
    CHECK( accessibility::format_help_menu_open( 23, "a", "Introduction" ) ==
           "Help. 23 topics. Use Up and Down to browse, hotkey or Enter to open, Escape to exit. a: Introduction" );
    CHECK( accessibility::format_help_menu_open( 0, "", "" ) ==
           "Help. 0 topics. Escape to exit." );

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

TEST_CASE( "help_browser_navigation_document_reading_and_repeat_last", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    // 1. Open Help menu
    accessibility::announce( accessibility::format_help_menu_open( 23, "a", "Introduction" ) );

    // 2. Linear navigation through topics across visual columns
    accessibility::announce( accessibility::format_help_topic_focus( "b", "Movement" ) );
    accessibility::announce( accessibility::format_help_topic_focus( "c", "Viewing" ) );
    // Cross-column boundary navigation (e.g. topic 13: Combat)
    accessibility::announce( accessibility::format_help_topic_focus( "n", "Combat" ) );

    // 3. Repeat-last on topic list
    CHECK( accessibility::repeat_last_announcement() );

    // 4. Open Movement topic
    const std::string para0 = "Movement is performed using the numpad, the arrow keys, or vikeys.";
    const std::string para1 =
        "Movement keys: Northwest: 7, y; North: 8, k; Northeast: 9, u; West: 4, h; Pause: 5, .; East: 6, l; Southwest: 1, b; South: 2, j; Southeast: 3, n.";
    const std::string para2 =
        "Each step will take 100 movement points (or more, depending on the terrain).";

    accessibility::announce( accessibility::format_document_focus( "Movement", para0 ) );

    // 5. Navigate paragraphs without index prefixes
    int para_cursor = 0;
    para_cursor = accessibility::move_document_cursor( para_cursor, 3, 1 );
    CHECK( para_cursor == 1 );
    accessibility::announce( para1 );

    para_cursor = accessibility::move_document_cursor( para_cursor, 3, 1 );
    CHECK( para_cursor == 2 );
    accessibility::announce( para2 );

    // 6. Repeat-last in document reading
    CHECK( accessibility::repeat_last_announcement() );

    // 7. Navigate back up
    para_cursor = accessibility::move_document_cursor( para_cursor, 3, -1 );
    CHECK( para_cursor == 1 );
    accessibility::announce( para1 );

    // 8. Return from document to Help topic list
    accessibility::announce( accessibility::format_document_focus(
                                 "Help", accessibility::format_help_topic_focus( "b", "Movement" ) ) );

    // 9. Repeat-last on returned topic list
    CHECK( accessibility::repeat_last_announcement() );

    // 10. Exit Help back to originating caller (e.g. Main menu)
    accessibility::announce( "Main menu. Help." );

    REQUIRE( capture_ptr->messages.size() == 13 );
    CHECK( capture_ptr->messages[0] ==
           "Help. 23 topics. Use Up and Down to browse, hotkey or Enter to open, Escape to exit. a: Introduction" );
    CHECK( capture_ptr->messages[1] == "b: Movement" );
    CHECK( capture_ptr->messages[2] == "c: Viewing" );
    CHECK( capture_ptr->messages[3] == "n: Combat" );
    CHECK( capture_ptr->messages[4] == "n: Combat" );
    CHECK( capture_ptr->messages[5] == "Movement. " + para0 );
    CHECK( capture_ptr->messages[6] == para1 );
    CHECK( capture_ptr->messages[7] == para2 );
    CHECK( capture_ptr->messages[8] == para2 );
    CHECK( capture_ptr->messages[9] == para1 );
    CHECK( capture_ptr->messages[10] == "Help. b: Movement" );
    CHECK( capture_ptr->messages[11] == "Help. b: Movement" );
    CHECK( capture_ptr->messages[12] == "Main menu. Help." );

    accessibility::reset_backend_after_testing();
}
