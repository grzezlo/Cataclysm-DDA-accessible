#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "accessibility.h"
#include "accessibility_format.h"
#include "accessibility_gameplay.h"
#include "action.h"
#include "activity_handlers.h"
#include "advanced_inv.h"
#include "advanced_inv_area.h"
#include "advanced_inv_pane.h"
#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "game.h"
#include "help.h"
#include "input.h"
#include "input_context.h"
#include "input_enums.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_category.h"
#include "map.h"
#include "map_helpers.h"
#include "map_item_stack.h"
#include "map_selector.h"
#include "monster.h"
#include "pickup.h"
#include "player_helpers.h"
#include "point.h"
#include "string_formatter.h"
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

        void cancel() override {
            ++cancels;
        }

        std::vector<std::string> messages;
        int cancels = 0;
};

class capture_compare_selector final : public inventory_compare_selector
{
    public:
        explicit capture_compare_selector( Character &u ) : inventory_compare_selector( u ) {}

        void speak_focus() {
            announce_accessibility_focus();
        }

        void toggle_focused() {
            inventory_entry &entry = get_active_column().get_highlighted();
            toggle_entry( &entry );
        }

        void switch_category_mode() {
            toggle_navigation_mode();
        }
};

class capture_inventory_selector final : public inventory_pick_selector
{
    public:
        explicit capture_inventory_selector( Character &u ) : inventory_pick_selector( u ) {}

        void speak_focus() {
            announce_accessibility_focus();
        }

        // Opening the selector again, as the inventory does after an item action.
        void reopen() {
            create_or_get_ui_adaptor();
        }

        static bool invlet_allowed_for( const std::string &action ) {
            return action_allows_invlet( action );
        }

        // A focus move key followed by the resulting highlight.
        bool move_to( const std::string &action, const item_location &target ) {
            note_accessibility_move( action );
            const bool found = highlight_one_of( { target } );
            announce_accessibility_focus();
            return found;
        }

        std::string status() const {
            return accessibility_status();
        }
};

class capture_drop_selector final : public inventory_drop_selector
{
    public:
        explicit capture_drop_selector( Character &u ) : inventory_drop_selector( u ) {}

        void speak_focus() {
            announce_accessibility_focus();
        }

        // Marking as the TOGGLE_ENTRY and count inputs do.
        void select_focused( size_t count ) {
            toggle_entry( get_active_column().get_highlighted(), count );
            announce_accessibility_selection();
        }

        std::string status() const {
            return accessibility_status();
        }
};

class capture_pickup_selector final : public pickup_selector
{
    public:
        explicit capture_pickup_selector( Character &u ) : pickup_selector( u ) {}

        std::string focus() const {
            return accessibility::format_inventory_focus(
                       accessibility_entry( get_active_column().get_highlighted() ), false, false );
        }

        // The row as ACCESSIBILITY_DETAILS reads it.
        std::string details() const {
            return accessibility::format_inventory_focus(
                       accessibility_entry( get_active_column().get_highlighted() ), true, true );
        }
};

// Pane areas as the advanced inventory tests set them.
void set_transfer_panes( advanced_inventory &advinv, const std::string &source_action,
                         aim_location source, const std::string &destination_action, aim_location destination )
{
    if( advinv.get_pane( advinv.get_src() ).get_area() != destination ) {
        advinv.process_action( destination_action );
        REQUIRE( advinv.get_pane( advinv.get_src() ).get_area() == destination );
    }
    advinv.process_action( "TOGGLE_TAB" );
    if( advinv.get_pane( advinv.get_src() ).get_area() != source ) {
        advinv.process_action( source_action );
        REQUIRE( advinv.get_pane( advinv.get_src() ).get_area() == source );
    }
    advinv.recalc_pane( advanced_inventory::side::left );
    advinv.recalc_pane( advanced_inventory::side::right );
}

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

    input_context template_action( "LOAD_DELETE_CANCEL", keyboard_mode::keycode );
    template_action.register_action( "LOAD" );
    template_action.register_action( "DELETE" );
    template_action.register_action( "CANCEL" );
    template_action.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( template_action.input_to_action( alt_r ) == "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( template_action.input_to_action( input_event( {}, KEY_ESCAPE,
                                            input_event_t::keyboard_code ) ) == "CANCEL" );

    input_context number( "STRING_INPUT", keyboard_mode::keychar );
    number.register_action( "ANY_INPUT" );
    number.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( number.input_to_action( alt_r_char ) == "ACCESSIBILITY_REPEAT_LAST" );
    CHECK( number.input_to_action( pure_r ) != "ACCESSIBILITY_REPEAT_LAST" );
}

TEST_CASE( "accessibility_announcements_can_be_captured_and_repeated", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    CHECK( accessibility::is_active() );
    CHECK_FALSE( accessibility::announce( "" ) );
    CHECK( capture_ptr->messages.empty() );

    CHECK( accessibility::announce( "Main menu" ) );
    CHECK( accessibility::announce_focus( "Custom character", "New game" ) );
    CHECK( accessibility::announce_status( "Loading" ) );
    CHECK( accessibility::last_announcement() == "Loading" );
    CHECK_FALSE( accessibility::announce_game_message( "Hidden during setup" ) );
    accessibility::enable_game_messages( true );
    CHECK( accessibility::announce_game_message( "You hear a noise." ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    CHECK( accessibility::announce_movement( "northwest", "wooden floor" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( accessibility::announce_movement( "northwest", "grass" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( accessibility::announce_movement( "east", "grass" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    // Routine movement speech never replaces the repeat-last message.
    CHECK( accessibility::last_announcement() == "You hear a noise." );
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
    CHECK( capture_ptr->messages[4] == "wooden floor. northwest." );
    CHECK( capture_ptr->messages[5] == "grass." );
    CHECK( capture_ptr->messages[6] == "grass. east." );
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
                                 "Treat which part?", 3, disabled, "Alt+d", true );
    CHECK( open == "Treat which part? 3 items. Use arrow keys to choose. Alt+d for details. " +
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
    CHECK( accessibility::format_uilist_open( "Choose intensity", 1, plain, "Alt+d", false ) ==
           "Choose intensity. 1 item. Use arrow keys to choose. Moderate." );
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
    const std::string opened = accessibility::format_popup_prompt(
                                   question, "No", { "Yes", "No" }, "Left arrow", "Right arrow", "Enter", true );
    CHECK( opened == "Stop exercising? No selected. Choose with Left arrow or Right arrow; "
           "confirm with Enter." );
    CHECK( accessibility::announce( opened ) );
    CHECK( accessibility::format_popup_option_focus( "Yes" ) == "Yes selected" );
    CHECK( accessibility::announce( accessibility::format_popup_option_focus( "Yes" ) ) );
    const std::string context = accessibility::format_popup_prompt(
                                    question, "Yes", {}, "", "", "", false );
    CHECK( context == "Stop exercising? Yes selected." );
    accessibility::set_last_announcement( context );
    CHECK( accessibility::repeat_last_announcement() );
    REQUIRE( capture_ptr->messages.size() == 3 );
    CHECK( capture_ptr->messages.back() == context );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "multi_option_popup_announces_choices_then_focused_option",
           "[accessibility][nogame]" )
{
    const std::string question = "What to do with template \"Survivor\"?";
    const std::vector<std::string> choices = { "Load", "Delete", "Cancel" };
    CHECK( accessibility::format_popup_prompt( question, "Load", choices,
            "Left arrow", "Right arrow", "Enter", true ) ==
           "What to do with template \"Survivor\"? Choices: Load, Delete, Cancel. "
           "Load selected. Choose with Left arrow or Right arrow; confirm with Enter." );
    CHECK( accessibility::format_popup_option_focus( "Delete" ) == "Delete selected" );
    CHECK( accessibility::format_popup_prompt( question, "Delete", {}, "", "", "", false ) ==
           "What to do with template \"Survivor\"? Delete selected." );
}

TEST_CASE( "text_input_reads_character_after_cursor_in_either_direction",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_text_input( "Minutes?", "", "123" ) ==
           "Minutes? Value: 123." );
    CHECK( accessibility::format_text_input( "Minutes?", "Minimum 1", "" ) ==
           "Minutes? Minimum 1. Value: empty." );
    CHECK( accessibility::format_text_cursor( "123", 3 ) == "End of field" );
    CHECK( accessibility::format_text_cursor( "123", 2 ) == "3" );
    CHECK( accessibility::format_text_cursor( "123", 1 ) == "2" );
    CHECK( accessibility::format_text_cursor( "123", 0 ) == "1" );
    CHECK( accessibility::format_text_cursor( "", 0 ) == "End of field" );
    CHECK( accessibility::format_text_cursor( "-12", 0 ) == "Minus" );
    CHECK( accessibility::format_text_character( " " ) == "Space" );
    CHECK( accessibility::format_text_cursor( u8"ąb", 0 ) == u8"ą" );
    CHECK( accessibility::format_text_cursor( u8"ąb", 1 ) == "b" );
    CHECK( accessibility::format_text_cursor( u8"ąb", 2 ) == "End of field" );
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

TEST_CASE( "keybinding_accessibility_actions_registered_in_contexts", "[accessibility]" )
{
    const input_context gameplay = get_default_mode_input_context();
    CHECK( gameplay.is_registered_action( "accessibility_situation" ) );
    CHECK( gameplay.is_registered_action( "accessibility_status" ) );
    CHECK( gameplay.is_registered_action( "ACCESSIBILITY_REPEAT_LAST" ) );

    input_context custom_ctx( "CUSTOM_CTX", keyboard_mode::keychar );
    custom_ctx.register_action( "SOME_ACTION" );
    CHECK_FALSE( custom_ctx.is_registered_action( "ACCESSIBILITY_DETAILS" ) );
    CHECK_FALSE( custom_ctx.is_registered_action( "ACCESSIBILITY_REPEAT_LAST" ) );

    for( const std::string &accessibility_action : {
             "ACCESSIBILITY_DETAILS",
             "ACCESSIBILITY_REPEAT_LAST"
         } ) {
        if( !custom_ctx.is_registered_action( accessibility_action ) ) {
            custom_ctx.register_action( accessibility_action );
        }
    }
    CHECK( custom_ctx.is_registered_action( "ACCESSIBILITY_DETAILS" ) );
    CHECK( custom_ctx.is_registered_action( "ACCESSIBILITY_REPEAT_LAST" ) );
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
           "aspirin. Count: 6. 3 locations. 4 tiles northeast. Category: drugs. "
           "At this location: 2. High priority." );
    CHECK( accessibility::format_nearby_list_summary( "Nearby items", 9, item_entry ) ==
           "Nearby items. 9 results. 5 of 9. Location 2 of 3." );
    CHECK( accessibility::format_nearby_list_summary( "Nearby creatures", 0, {} ) ==
           "Nearby creatures. No results." );
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
           "Nearby items. 2 results. aspirin. Count: 2. 2 locations. 2 tiles east. "
           "Category: drugs." );
    CHECK( capture_ptr->messages[1] ==
           "flashlight. Count: 1. 3 tiles south. Category: tools." );
    CHECK( capture_ptr->messages[2] ==
           "aspirin. Count: 2. 2 locations. 4 tiles northeast. Category: drugs." );
    CHECK( capture_ptr->messages[3].find( "Filter: asp" ) != std::string::npos );
    CHECK( capture_ptr->messages[4].find( "Sorted by name" ) != std::string::npos );
    CHECK( capture_ptr->messages[5] ==
           "Nearby creatures. 1 result. zombie. 5 tiles northwest. Attitude: Hostile. "
           "Behavior: Tracking. Health: lightly injured." );
    CHECK( capture_ptr->messages[6] ==
           "zombie. 5 tiles northwest. Attitude: Hostile. Behavior: Tracking. "
           "Health: lightly injured." );
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

    int descriptions_built = 0;
    const auto describe = [&descriptions_built]( const std::string & text ) {
        return [&descriptions_built, text]() {
            ++descriptions_built;
            return text;
        };
    };
    CHECK_FALSE( accessibility::evaluate_threat_transition( {}, describe( "No threats in view" ) ) );
    CHECK( descriptions_built == 0 );
    CHECK( accessibility::evaluate_condition_transitions( normal ).empty() );

    const std::optional<std::string> threat = accessibility::evaluate_threat_transition(
    { "monster-1" }, describe( "Zombie spotted!" ) );
    REQUIRE( threat );
    CHECK( descriptions_built == 1 );
    CHECK_FALSE( accessibility::evaluate_threat_transition( { "monster-1" },
                 describe( "Unchanged threats are not described" ) ) );
    CHECK( descriptions_built == 1 );

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
    CHECK( accessibility::evaluate_threat_transition( { "monster-1" },
            describe( "Zombie spotted!" ) ) );

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
    CHECK( accessibility::format_help_menu_open( 23, "a", "Introduction", "Up", "Down",
            "Enter", "Escape" ) ==
           "Help. 23 topics. Use Up and Down to browse, hotkey or Enter to open, Escape to exit. a: Introduction" );
    CHECK( accessibility::format_help_menu_open( 0, "", "", "Up", "Down", "Enter",
            "Escape" ) ==
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
    accessibility::announce( accessibility::format_help_menu_open( 23, "a", "Introduction", "Up",
                             "Down",
                             "Enter", "Escape" ) );

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

TEST_CASE( "comparison_selector_announces_first_mark_and_focus_revisit", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    item &first_item = here.add_item_or_charges( position, item( itype_id( "hammer" ) ) );
    item &second_item = here.add_item_or_charges( position, item( itype_id( "2x4" ) ) );
    const item_location first( map_cursor( position ), &first_item );
    const item_location second( map_cursor( position ), &second_item );
    REQUIRE( first );
    REQUIRE( second );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    capture_compare_selector selector( you );
    selector.set_title( "Compare" );
    selector.add_map_items( position );
    REQUIRE( selector.highlight_one_of( { first } ) );
    selector.speak_focus();
    REQUIRE( !capture_ptr->messages.empty() );
    CHECK( capture_ptr->messages.back().find( "comparison item" ) == std::string::npos );
    const size_t after_open = capture_ptr->messages.size();
    selector.speak_focus();
    CHECK( capture_ptr->messages.size() == after_open );

    const size_t before_mark = capture_ptr->messages.size();
    selector.toggle_focused();
    REQUIRE( capture_ptr->messages.size() == before_mark + 1 );
    CHECK( capture_ptr->messages.back().find( "Selected as first comparison item" ) !=
           std::string::npos );

    REQUIRE( selector.highlight_one_of( { second } ) );
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "comparison item" ) == std::string::npos );
    CHECK( capture_ptr->messages.back().find( first->tname() ) == std::string::npos );
    CHECK( capture_ptr->messages.back().find(
               second->get_category_of_contents().name_header() ) != std::string::npos );

    REQUIRE( selector.highlight_one_of( { first } ) );
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Selected as first comparison item" ) !=
           std::string::npos );

    selector.toggle_focused();
    CHECK( capture_ptr->messages.back().find( "deselected for comparison" ) !=
           std::string::npos );
    const size_t after_deselect = capture_ptr->messages.size();
    selector.speak_focus();
    CHECK( capture_ptr->messages.size() == after_deselect );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( capture_ptr->messages.back() == capture_ptr->messages[capture_ptr->messages.size() - 2] );

    selector.switch_category_mode();
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Category selection mode" ) !=
           std::string::npos );
    selector.switch_category_mode();
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Item selection mode" ) !=
           std::string::npos );

    selector.set_filter( "hammer" );
    REQUIRE( selector.highlight_one_of( { first } ) );
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Filter: hammer. 1 matching item" ) !=
           std::string::npos );
    selector.set_filter( "" );
    REQUIRE( selector.highlight_one_of( { first } ) );
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Filter cleared" ) != std::string::npos );
    CHECK( capture_ptr->messages.back().find( first->tname() ) != std::string::npos );

    selector.toggle_focused();
    REQUIRE( selector.highlight_one_of( { second } ) );
    selector.toggle_focused();
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Selected as second comparison item" ) !=
           std::string::npos );
    selector.toggle_focused();
    CHECK( capture_ptr->messages.back().find( "deselected for comparison" ) !=
           std::string::npos );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "inventory_selector_names_column_and_focus_when_source_changes", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const item_location held = you.i_add( item( itype_id( "hammer" ) ) );
    REQUIRE( held );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    item &ground_item = here.add_item_or_charges( position, item( itype_id( "rock" ) ) );
    const item_location ground( map_cursor( position ), &ground_item );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    capture_inventory_selector selector( you );
    selector.set_title( "Inventory" );
    selector.add_character_items( you );
    selector.add_map_items( position );
    REQUIRE( selector.highlight_one_of( { held } ) );
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( held->tname() ) != std::string::npos );

    REQUIRE( selector.highlight_one_of( { ground } ) );
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Nearby items" ) != std::string::npos );
    CHECK( capture_ptr->messages.back().find( ground->tname() ) != std::string::npos );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "inventory_selector_speaks_count_once_wrap_and_position_on_request",
           "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
    const item_location hammer = you.i_add( item( itype_id( "hammer" ) ) );
    const item_location rock = you.i_add( item( itype_id( "rock" ) ) );
    REQUIRE( hammer );
    REQUIRE( rock );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    capture_inventory_selector selector( you );
    selector.set_title( "Inventory" );
    selector.add_character_items( you );
    REQUIRE( selector.highlight_one_of( { hammer } ) );
    const bool hammer_first = selector.status().find( "1 of 2" ) != std::string::npos;
    const item_location &first = hammer_first ? hammer : rock;
    const item_location &last = hammer_first ? rock : hammer;
    REQUIRE( selector.highlight_one_of( { first } ) );
    selector.speak_focus();
    REQUIRE_FALSE( capture_ptr->messages.empty() );
    CHECK( capture_ptr->messages.back().find( "2 items. " ) != std::string::npos );
    CHECK( capture_ptr->messages.back().find( " of 2" ) == std::string::npos );

    REQUIRE( selector.move_to( "DOWN", last ) );
    CHECK( capture_ptr->messages.back().find( last->tname() ) != std::string::npos );
    INFO( capture_ptr->messages.back() );
    // The row's location may name the container's contents, but no list count precedes the row.
    CHECK( capture_ptr->messages.back().find( "2 items. " ) == std::string::npos );
    CHECK( capture_ptr->messages.back().find( "Top" ) == std::string::npos );
    CHECK( selector.status().find( "2 of 2" ) != std::string::npos );

    REQUIRE( selector.move_to( "DOWN", first ) );
    CHECK( capture_ptr->messages.back().find( "Top. " ) == 0 );
    REQUIRE( selector.move_to( "UP", last ) );
    CHECK( capture_ptr->messages.back().find( "Bottom. " ) == 0 );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( capture_ptr->messages.back().find( "Bottom. " ) == 0 );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "item_action_menu_speaks_state_shown_only_by_color", "[accessibility][nogame]" )
{
    using accessibility::item_action_availability;
    CHECK( accessibility_gameplay::item_action_availability( hint_rating::good ) ==
           item_action_availability::available );
    CHECK( accessibility_gameplay::item_action_availability( hint_rating::iffy ) ==
           item_action_availability::not_now );
    CHECK( accessibility_gameplay::item_action_availability( hint_rating::cant ) ==
           item_action_availability::not_applicable );

    CHECK( accessibility::format_uilist_focus( "wield", "", false,
            accessibility::format_item_action_availability( item_action_availability::available ) ) ==
           "wield." );
    CHECK( accessibility::format_uilist_focus( "wear", "", false,
            accessibility::format_item_action_availability( item_action_availability::not_now ) ) ==
           "wear. Not possible now." );
    CHECK( accessibility::format_uilist_focus( "read", "", false,
            accessibility::format_item_action_availability(
                item_action_availability::not_applicable ) ) == "read. Not applicable." );
    CHECK( accessibility::format_uilist_focus( "Auto pickup", "", false,
            accessibility::format_auto_pickup_rule_state( true ) ) == "Auto pickup. Rule set." );
    CHECK( accessibility::format_uilist_focus( "Auto pickup", "", false,
            accessibility::format_auto_pickup_rule_state( false ) ) == "Auto pickup. No rule." );

    const std::string open = accessibility::format_uilist_open(
                                 accessibility::format_item_action_menu_context( "combat knife" ),
                                 12, "activate.", "Alt+d", true );
    CHECK( open ==
           "Actions for combat knife. 12 items. Use arrow keys to choose. Alt+d for details. activate." );
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

TEST_CASE( "inventory_selector_reopened_after_item_action_speaks_only_focus",
           "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const item_location held = you.i_add( item( itype_id( "hammer" ) ) );
    REQUIRE( held );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    capture_inventory_selector selector( you );
    selector.set_title( "Inventory" );
    selector.set_hint( "Food: Full \xe2\x94\x82 Drink: Thirsty" );
    selector.add_character_items( you );
    REQUIRE( selector.highlight_one_of( { held } ) );
    selector.speak_focus();
    REQUIRE_FALSE( capture_ptr->messages.empty() );
    const std::string first = capture_ptr->messages.back();
    CHECK( first.find( "Inventory. Food: Full. Drink: Thirsty." ) == 0 );
    CHECK( first.find( "\xe2\x94\x82" ) == std::string::npos );
    CHECK( first.find( held->tname() ) != std::string::npos );

    selector.reopen();
    selector.speak_focus();
    const std::string reopened = capture_ptr->messages.back();
    CHECK( reopened.find( "Inventory" ) == std::string::npos );
    CHECK( reopened.find( "Food" ) == std::string::npos );
    CHECK( reopened.find( held->tname() ) != std::string::npos );

    // A new purpose is announced again.
    selector.set_title( "Wield item" );
    selector.reopen();
    selector.speak_focus();
    CHECK( capture_ptr->messages.back().find( "Wield item" ) == 0 );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "screen_reader_mode_controls_speech_and_latches_detection", "[accessibility][nogame]" )
{
    class toggled_backend final : public accessibility::backend
    {
        public:
            bool is_available() const override {
                return available;
            }

            bool announce( const std::string &message ) override {
                messages.push_back( message );
                return true;
            }

            bool available = false;
            std::vector<std::string> messages;
    };

    auto backend = std::make_unique<toggled_backend>();
    toggled_backend *backend_ptr = backend.get();
    accessibility::set_backend_for_testing( std::move( backend ) );

    // Automatic mode stays inactive while no screen reader is detected.
    CHECK_FALSE( accessibility::is_active() );
    CHECK_FALSE( accessibility::announce( "Silent" ) );
    CHECK( backend_ptr->messages.empty() );

    // Forced on works without a detected screen reader, e.g. for testing.
    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::on );
    CHECK( accessibility::is_active() );
    CHECK( accessibility::announce( "Forced" ) );

    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::off );
    backend_ptr->available = true;
    CHECK_FALSE( accessibility::is_active() );
    CHECK_FALSE( accessibility::announce( "Disabled" ) );
    CHECK_FALSE( accessibility::repeat_last_announcement() );

    // Selecting automatic mode detects immediately and keeps the interface
    // active if the screen reader later becomes unavailable.
    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::automatic );
    CHECK( accessibility::is_active() );
    backend_ptr->available = false;
    CHECK( accessibility::is_active() );

    REQUIRE( backend_ptr->messages.size() == 1 );
    CHECK( backend_ptr->messages[0] == "Forced" );

    accessibility::reset_backend_after_testing();
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

TEST_CASE( "threat_alerts_ignore_unrelated_creature_removal", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = you.pos_bub();
    monster &unrelated = spawn_test_monster( "mon_zombie", origin + tripoint_rel_ms( 4, 0, 0 ) );
    monster &threat = spawn_test_monster( "mon_zombie", origin + tripoint_rel_ms( 0, 3, 0 ) );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    accessibility_gameplay::announce_turn_alerts( here, you, { &threat } );
    accessibility::flush_gameplay_announcements();
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0].find( "spotted!" ) != std::string::npos );

    // Removing a creature earlier in the tracker must not re-announce the same threat.
    g->remove_zombie( unrelated );
    accessibility_gameplay::announce_turn_alerts( here, you, { &threat } );
    accessibility::flush_gameplay_announcements();
    CHECK( capture_ptr->messages.size() == 1 );

    accessibility_gameplay::announce_turn_alerts( here, you, {} );
    accessibility::flush_gameplay_announcements();
    REQUIRE( capture_ptr->messages.size() == 2 );
    CHECK( capture_ptr->messages[1] == "No threats in view." );

    accessibility::reset_backend_after_testing();
    clear_map();
}

TEST_CASE( "inventory_invlets_still_select_items_except_for_accessibility_shortcuts",
           "[accessibility][nogame]" )
{
    CHECK( capture_inventory_selector::invlet_allowed_for( "ANY_INPUT" ) );
    // An action that shares its key with an invlet keeps the upstream invlet behavior.
    CHECK( capture_inventory_selector::invlet_allowed_for( "EXAMINE" ) );
    CHECK_FALSE( capture_inventory_selector::invlet_allowed_for( "ACCESSIBILITY_DETAILS" ) );
    CHECK_FALSE( capture_inventory_selector::invlet_allowed_for( "ACCESSIBILITY_REPEAT_LAST" ) );
    CHECK_FALSE( capture_inventory_selector::invlet_allowed_for( "ACCESSIBILITY_STATUS" ) );
}

TEST_CASE( "focus_speech_interrupts_once_per_key_and_gameplay_speech_queues",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    // Speech without a new key press never interrupts.
    CHECK( accessibility::announce( "Inventory" ) );
    CHECK( capture_ptr->cancels == 0 );

    // The first focus speech after a key replaces unfinished speech; context
    // spoken for the same key queues behind it.
    accessibility::note_player_input();
    CHECK( accessibility::announce( "Hammer" ) );
    CHECK( accessibility::announce( "Details available" ) );
    CHECK( capture_ptr->cancels == 1 );
    accessibility::note_player_input();
    CHECK( accessibility::announce( "Rock" ) );
    CHECK( capture_ptr->cancels == 2 );

    // Gameplay speech of one turn is one utterance and never interrupts.
    accessibility::enable_game_messages( true );
    accessibility::note_player_input();
    CHECK( accessibility::announce_movement( "north", "grass" ) );
    CHECK( accessibility::announce_game_message( "You hear a whack!" ) );
    accessibility::queue_gameplay_announcement( "Zombie spotted!",
            accessibility::gameplay_speech::meaningful );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( capture_ptr->cancels == 2 );
    CHECK( capture_ptr->messages.back() == "grass. north. You hear a whack! Zombie spotted!" );
    // Repeat-last keeps the meaningful parts without the routine tile name.
    CHECK( accessibility::last_announcement() == "You hear a whack! Zombie spotted!" );

    // A prompt caused by the same turn speaks pending alerts first and does
    // not cut them off.
    accessibility::note_player_input();
    accessibility::queue_gameplay_announcement( "Bleeding: Torso!",
            accessibility::gameplay_speech::meaningful );
    CHECK( accessibility::announce( "Really continue?" ) );
    CHECK( capture_ptr->cancels == 2 );
    REQUIRE( capture_ptr->messages.size() >= 2 );
    CHECK( capture_ptr->messages[capture_ptr->messages.size() - 2] == "Bleeding: Torso!" );
    CHECK( capture_ptr->messages.back() == "Really continue?" );

    // Repeat-last is a key press and replaces unfinished speech.
    accessibility::note_player_input();
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( capture_ptr->cancels == 3 );
    CHECK( capture_ptr->messages.back() == "Really continue?" );

    // Skipped auto-move steps are reported once.
    accessibility::note_suppressed_movement();
    CHECK( accessibility::take_suppressed_movement() );
    CHECK_FALSE( accessibility::take_suppressed_movement() );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "walking_speaks_each_destination_tile_after_travel", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms start = you.pos_bub();

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    const auto step_east = [&]() {
        you.set_moves( you.get_speed() );
        REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
        accessibility_gameplay::before_player_input( here, you );
    };

    step_east();
    REQUIRE_FALSE( capture_ptr->messages.empty() );
    const std::string tile = here.tername( you.pos_bub() );
    CHECK( capture_ptr->messages.back().find( tile ) != std::string::npos );

    // A finished travel route leaves the saved destination point behind; later
    // manual steps must still be spoken.
    you.set_destination( { you.pos_bub() + tripoint_rel_ms::east } );
    CHECK( you.get_next_auto_move_direction() != ACTION_NULL );
    CHECK_FALSE( you.has_destination() );
    REQUIRE( you.is_auto_moving() );
    const size_t before_travel_end = capture_ptr->messages.size();
    step_east();
    REQUIRE( capture_ptr->messages.size() == before_travel_end + 1 );
    CHECK( capture_ptr->messages.back().find( tile ) != std::string::npos );
    CHECK( capture_ptr->messages.back().find( "Gameplay" ) == std::string::npos );

    const size_t before_manual = capture_ptr->messages.size();
    step_east();
    REQUIRE( capture_ptr->messages.size() == before_manual + 1 );
    CHECK( capture_ptr->messages.back().find( tile ) != std::string::npos );
    CHECK( capture_ptr->messages.back().find( "Gameplay" ) == std::string::npos );

    // Steps with more route ahead are quiet, and the arrival step is spoken
    // once without a separate position report.
    you.set_destination( { you.pos_bub() + tripoint_rel_ms::east,
                           you.pos_bub() + tripoint_rel_ms::east * 2 } );
    CHECK( you.get_next_auto_move_direction() != ACTION_NULL );
    const size_t before_route = capture_ptr->messages.size();
    you.set_moves( you.get_speed() );
    REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    CHECK( you.get_next_auto_move_direction() != ACTION_NULL );
    step_east();
    REQUIRE( capture_ptr->messages.size() == before_route + 1 );
    CHECK( capture_ptr->messages.back().find( "Gameplay" ) == std::string::npos );

    you.clear_destination();
    accessibility::reset_backend_after_testing();
    you.setpos( here, start );
    clear_map();
}

TEST_CASE( "repeated_game_messages_need_a_new_key_press", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    // An activity such as smashing repeats the same message every turn
    // without input; only the first one is spoken.
    accessibility::note_player_input();
    CHECK( accessibility::announce_game_message( "You hear whack!" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK_FALSE( accessibility::announce_game_message( "You hear whack!" ) );
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    CHECK_FALSE( accessibility::announce_game_message( "You hear whack!" ) );
    CHECK( accessibility::announce_game_message( "The chair is destroyed." ) );
    CHECK( accessibility::flush_gameplay_announcements() );

    // A repeat caused by a new key press, such as another attack, is spoken.
    accessibility::note_player_input();
    CHECK( accessibility::announce_game_message( "You miss the zombie." ) );
    accessibility::note_player_input();
    CHECK( accessibility::announce_game_message( "You miss the zombie." ) );
    CHECK( accessibility::flush_gameplay_announcements() );

    REQUIRE( capture_ptr->messages.size() == 3 );
    CHECK( capture_ptr->messages[0] == "You hear whack!" );
    CHECK( capture_ptr->messages[1] == "The chair is destroyed." );
    CHECK( capture_ptr->messages[2] == "You miss the zombie. You miss the zombie." );

    accessibility::reset_backend_after_testing();
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

TEST_CASE( "multiselect_speaks_selection_state_totals_and_status", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
    item_location rock;
    for( int i = 0; i < 3; ++i ) {
        rock = you.i_add( item( itype_id( "rock" ) ) );
    }
    REQUIRE( rock );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    capture_drop_selector selector( you );
    selector.set_title( "Drop" );
    selector.add_character_items( you );
    REQUIRE( selector.highlight_one_of( { rock } ) );
    selector.speak_focus();
    REQUIRE( !capture_ptr->messages.empty() );
    CHECK( capture_ptr->messages.back().find( "Not selected" ) != std::string::npos );

    const size_t before_mark = capture_ptr->messages.size();
    selector.select_focused( 2 );
    REQUIRE( capture_ptr->messages.size() == before_mark + 1 );
    CHECK( capture_ptr->messages.back().find( "Selected 2 of 3. 1 entry selected" ) !=
           std::string::npos );
    selector.speak_focus();
    CHECK( capture_ptr->messages.size() == before_mark + 1 );

    selector.select_focused( 3 );
    CHECK( capture_ptr->messages.back().find( "Selected all 3" ) != std::string::npos );
    CHECK( selector.status().find( "1 entry selected" ) != std::string::npos );

    selector.select_focused( 0 );
    CHECK( capture_ptr->messages.back().find( "Not selected. Nothing selected" ) !=
           std::string::npos );
    CHECK( selector.status().find( "Drop" ) == 0 );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( capture_ptr->messages.back() == capture_ptr->messages[capture_ptr->messages.size() - 2] );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "advanced_inventory_speaks_panes_rows_status_and_transfer_result",
           "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    here.add_item_or_charges( position, item( itype_id( "hammer" ) ) );
    here.add_item_or_charges( position, item( itype_id( "rock" ) ) );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    advanced_inventory advinv;
    advinv.init();
    set_transfer_panes( advinv, "ITEMS_CE", AIM_CENTER, "ITEMS_INVENTORY", AIM_INVENTORY );
    const advanced_inventory_pane &ground = advinv.get_pane( advinv.get_src() );
    REQUIRE( ground.items.size() == 2 );

    advinv.announce_accessibility();
    REQUIRE( !capture_ptr->messages.empty() );
    const std::string opening = capture_ptr->messages.back();
    CHECK( opening.find( "Advanced inventory. Source: Directly below you" ) == 0 );
    CHECK( opening.find( "2 items" ) != std::string::npos );
    CHECK( opening.find( "Destination: Inventory" ) != std::string::npos );
    CHECK( opening.find( ground.items.front().items.front()->display_name() ) != std::string::npos );
    const size_t after_open = capture_ptr->messages.size();
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.size() == after_open );

    advinv.process_action( "DOWN" );
    advinv.announce_accessibility();
    REQUIRE( capture_ptr->messages.size() == after_open + 1 );
    const item &second = *ground.items[1].items.front();
    const std::string second_name = second.display_name();
    CHECK( capture_ptr->messages.back().find( "Source:" ) == std::string::npos );
    CHECK( capture_ptr->messages.back().find( second_name ) == 0 );

    advinv.process_action( "ACCESSIBILITY_DETAILS" );
    CHECK( capture_ptr->messages.back().find( second.get_category_of_contents().name_header() ) ==
           0 );
    advinv.process_action( "ACCESSIBILITY_STATUS" );
    CHECK( capture_ptr->messages.back().find( "Source: Directly below you" ) == 0 );
    CHECK( capture_ptr->messages.back().find( "Destination: Inventory" ) != std::string::npos );
    CHECK( capture_ptr->messages.back().find( "Weight" ) != std::string::npos );
    // The position is heard only on request.
    CHECK( capture_ptr->messages.back().find( "2 of 2" ) != std::string::npos );
    const size_t after_status = capture_ptr->messages.size();
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.size() == after_status );

    // Wrapping past either end names the end before the row.
    const std::string first_name = ground.items[0].items.front()->display_name();
    advinv.process_action( "DOWN" );
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.back().find( "Top. " + first_name ) == 0 );
    advinv.process_action( "UP" );
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.back().find( "Bottom. " + second_name ) == 0 );
    advinv.process_action( "UP" );
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.back().find( first_name ) == 0 );
    CHECK( capture_ptr->messages.back().find( " of " ) == std::string::npos );
    advinv.process_action( "DOWN" );
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.back().find( second_name ) == 0 );

    // Switching panes names the new source once, without its capacity.
    advinv.process_action( "TOGGLE_TAB" );
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.back().find( "Source: Inventory" ) == 0 );
    CHECK( capture_ptr->messages.back().find( "Weight" ) == std::string::npos );
    advinv.process_action( "TOGGLE_TAB" );
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.back().find( "Source: Directly below you" ) == 0 );

    advinv.process_action( "MOVE_SINGLE_ITEM" );
    process_activity( you );
    advinv.recalc_pane( advanced_inventory::side::left );
    advinv.recalc_pane( advanced_inventory::side::right );
    advinv.announce_accessibility();
    CHECK( capture_ptr->messages.back().find( "Moved " ) == 0 );
    CHECK( capture_ptr->messages.back().find( "to Inventory." ) != std::string::npos );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "screen_status_binding_reaches_transfer_screens", "[accessibility]" )
{
    const input_event alt_s( { keymod_t::alt }, 's', input_event_t::keyboard_code );
    for( const std::string category : {
             "ADVANCED_INVENTORY", "INVENTORY", "LIST_ITEMS", "LIST_MONSTERS", "DISPLAY_HELP",
             "MAIN_MENU"
         } ) {
        input_context ctxt( category, keyboard_mode::keycode );
        ctxt.register_action( "ACCESSIBILITY_STATUS" );
        CHECK( ctxt.input_to_action( alt_s ) == "ACCESSIBILITY_STATUS" );
    }
    const input_event alt_d( { keymod_t::alt }, 'd', input_event_t::keyboard_code );
    for( const std::string category : {
             "MESSAGE_LOG", "UILIST"
         } ) {
        input_context ctxt( category, keyboard_mode::keycode );
        ctxt.register_action( "ACCESSIBILITY_DETAILS" );
        CHECK( ctxt.input_to_action( alt_d ) == "ACCESSIBILITY_DETAILS" );
    }
}

TEST_CASE( "list_convention_speaks_count_wrap_and_position_on_request",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_list_count( 0 ) == "No items" );
    CHECK( accessibility::format_list_count( 1 ) == "1 item" );
    CHECK( accessibility::format_list_count( 12 ) == "12 items" );

    // Moving down past the last row lands on top; moving up past the first lands on bottom.
    CHECK( accessibility::format_list_wrap( 11, 0, 1 ) == "Top" );
    CHECK( accessibility::format_list_wrap( 0, 11, -1 ) == "Bottom" );
    CHECK( accessibility::format_list_wrap( 11, 0, 10 ) == "Top" );
    CHECK( accessibility::format_list_wrap( 3, 4, 1 ).empty() );
    CHECK( accessibility::format_list_wrap( 4, 3, -1 ).empty() );
    // Home and End, filter changes, and unset rows are not wraps.
    CHECK( accessibility::format_list_wrap( 11, 0, 0 ).empty() );
    CHECK( accessibility::format_list_wrap( -1, 0, 1 ).empty() );
    CHECK( accessibility::format_list_wrap( 5, -1, -1 ).empty() );

    CHECK( accessibility::format_list_position( 3, 12 ) == "3 of 12" );
    CHECK( accessibility::format_list_position( 0, 12 ).empty() );
    CHECK( accessibility::format_list_position( 1, 0 ).empty() );

    CHECK( accessibility::format_help_menu_summary( 23, 4 ) == "Help. 23 topics. 4 of 23." );
    CHECK( accessibility::format_main_menu_position( "Load", 4, 10, 2, 5 ) ==
           "Main menu. Tab Load, 4 of 10. Item 2 of 5." );
    CHECK( accessibility::format_main_menu_position( "Tutorial Game", 6, 10, 0, 0 ) ==
           "Main menu. Tab Tutorial Game, 6 of 10." );

    // A focus move speaks only the row; a wrap prefixes it and repeat-last keeps it.
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    CHECK( accessibility::announce( accessibility::format_uilist_open( "Status", 3, "Health.",
                                    "Alt+d", true ) ) );
    CHECK( accessibility::announce( "Stamina." ) );
    CHECK( accessibility::announce( "Needs." ) );
    CHECK( accessibility::announce( accessibility::format_semantic_sequence( {
        accessibility::format_list_wrap( 2, 0, 1 ), "Health."
    } ) ) );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( accessibility::announce( accessibility::format_semantic_sequence( {
        "Light exercise.", accessibility::format_list_position( 1, 3 )
    } ) ) );
    REQUIRE( capture_ptr->messages.size() == 6 );
    CHECK( capture_ptr->messages[0] ==
           "Status. 3 items. Use arrow keys to choose. Alt+d for details. Health." );
    CHECK( capture_ptr->messages[1] == "Stamina." );
    CHECK( capture_ptr->messages[3] == "Top. Health." );
    CHECK( capture_ptr->messages[4] == "Top. Health." );
    CHECK( capture_ptr->messages[5] == "Light exercise. 1 of 3." );
    for( size_t i = 1; i < capture_ptr->messages.size(); ++i ) {
        CHECK( capture_ptr->messages[i].find( "items" ) == std::string::npos );
    }
    accessibility::reset_backend_after_testing();
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

TEST_CASE( "direction_targets_name_visible_adjacent_things_in_compass_order", "[accessibility]" )
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
    const tripoint_bub_ms door = origin + tripoint::north;
    const tripoint_bub_ms table = origin + tripoint::east;
    here.ter_set( door, ter_str_id( "t_door_c" ) );
    here.furn_set( table, furn_str_id( "f_table" ) );
    here.add_item_or_charges( table, item( itype_id( "rock" ), calendar::turn ) );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );

    // Only the closed door can be opened, as the highlighted open prompt allows.
    std::vector<tripoint_bub_ms> openable;
    for( const tripoint_bub_ms &p : here.points_in_radius( origin, 1 ) ) {
        if( can_interact_at( ACTION_OPEN, here, p ) ) {
            openable.push_back( p );
        }
    }
    const std::vector<accessibility::direction_target> open_targets =
        accessibility_gameplay::direction_targets( here, viewer, origin, openable );
    REQUIRE( open_targets.size() == 1 );
    CHECK( open_targets[0].target == here.tername( door ) );
    CHECK( open_targets[0].direction == "north" );

    const std::vector<accessibility::direction_target> targets =
        accessibility_gameplay::direction_targets( here, viewer, origin, { origin, table, door } );
    REQUIRE( targets.size() == 3 );
    CHECK( targets[0].direction == "north" );
    CHECK( targets[1].target == here.furnname( table ) + " with 1 item" );
    CHECK( targets[1].direction == "east" );
    CHECK( targets[2].direction == "here" );
    CHECK( targets[2].target == here.tername( origin ) );

    player_wear_blindfold();
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    CHECK( accessibility_gameplay::direction_target( here, viewer, origin, table ).target ==
           "unseen" );
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

TEST_CASE( "auto_travel_speaks_start_arrival_and_early_stop", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    const time_point previous_time = calendar::turn;
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms start = you.pos_bub();
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    const on_out_of_scope cleanup( [&]() {
        you.clear_destination();
        accessibility::reset_game_state();
        accessibility::reset_backend_after_testing();
        you.setpos( here, start );
        clear_map();
        set_time( previous_time );
    } );
    accessibility::reset_game_state();
    accessibility::enable_game_messages( true );
    set_time_to_day();

    std::vector<tripoint_bub_ms> route;
    for( int i = 1; i <= 4; ++i ) {
        route.push_back( start + tripoint_rel_ms::east * i );
    }
    here.ter_set( route[2], ter_str_id( "t_floor" ) );
    here.ter_set( route[3], ter_str_id( "t_floor" ) );
    here.invalidate_map_cache( start.z() );
    here.build_map_cache( start.z(), true );
    const std::string ground = here.tername( route[0] );
    const std::string floor = here.tername( route[3] );
    REQUIRE( ground != floor );

    const auto begin_travel = [&]( const std::vector<tripoint_bub_ms> &path ) {
        you.set_destination( path );
        accessibility_gameplay::announce_travel_start( here, you, path );
        REQUIRE( accessibility::flush_gameplay_announcements() );
    };
    // One auto-move step as the game performs it: take the step, then move.
    const auto travel_step = [&]() {
        REQUIRE( you.get_next_auto_move_direction() != ACTION_NULL );
        you.set_moves( you.get_speed() );
        REQUIRE( avatar_action::move( you, here, tripoint_rel_ms::east ) );
    };

    SECTION( "start summary, silent steps, and one arrival" ) {
        begin_travel( route );
        CHECK( capture_ptr->messages.back() == "Traveling to " + floor + ", 4 tiles east, 4 steps: " +
               ground + " 2, " + floor + " 2." );
        CHECK( accessibility::last_announcement() == capture_ptr->messages.back() );
        const size_t before_steps = capture_ptr->messages.size();
        for( int i = 0; i < 3; ++i ) {
            travel_step();
            CHECK_FALSE( accessibility::flush_gameplay_announcements() );
        }
        travel_step();
        accessibility_gameplay::before_player_input( here, you );
        REQUIRE( capture_ptr->messages.size() == before_steps + 1 );
        CHECK( capture_ptr->messages.back() == "Arrived: " + floor + "." );
        CHECK_FALSE( accessibility::travel_in_progress() );
    }

    SECTION( "an obstruction stop names the remaining distance, then the position" ) {
        begin_travel( route );
        travel_step();
        // An obstacle aborts the route without a player-confirmed cancel.
        you.abort_automove();
        const size_t before_stop = capture_ptr->messages.size();
        accessibility_gameplay::before_player_input( here, you );
        REQUIRE( capture_ptr->messages.size() == before_stop + 1 );
        CHECK( capture_ptr->messages.back().rfind( "Travel stopped: " + floor + ", 3 tiles east.",
                0 ) == 0 );
        CHECK( capture_ptr->messages.back().find( ground ) != std::string::npos );
        // The stop is reported once.
        accessibility_gameplay::before_player_input( here, you );
        CHECK( capture_ptr->messages.size() == before_stop + 1 );
    }

    SECTION( "a confirmed cancel adds no travel speech" ) {
        begin_travel( route );
        travel_step();
        you.abort_automove();
        accessibility_gameplay::travel_canceled();
        const size_t before_cancel = capture_ptr->messages.size();
        accessibility_gameplay::before_player_input( here, you );
        REQUIRE( capture_ptr->messages.size() == before_cancel + 1 );
        CHECK( capture_ptr->messages.back().find( "Travel stopped" ) == std::string::npos );
        CHECK_FALSE( accessibility::travel_in_progress() );
    }

    SECTION( "unseen route tiles are not named" ) {
        player_wear_blindfold();
        here.invalidate_map_cache( start.z() );
        here.build_map_cache( start.z(), true );
        begin_travel( route );
        CHECK( capture_ptr->messages.back() ==
               "Traveling to unseen, 4 tiles east, 4 steps: unseen 4." );
    }
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

TEST_CASE( "suppressed_game_messages_are_not_spoken", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    accessibility::note_player_input();
    {
        accessibility::game_message_speech_suppression quiet;
        CHECK_FALSE( accessibility::announce_game_message( "You pick up: 1 rock" ) );
        {
            accessibility::game_message_speech_suppression nested;
            CHECK_FALSE( accessibility::announce_game_message( "You pick up: 1 hammer" ) );
        }
        CHECK_FALSE( accessibility::announce_game_message( "You pick up: 1 pipe" ) );
    }
    CHECK( accessibility::announce_game_message( "Picked up: rock, hammer, pipe." ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0] == "Picked up: rock, hammer, pipe." );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "pickup_selector_rows_speak_ownership_and_destination", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    item &own_rock = here.add_item_or_charges( position, item( itype_id( "rock" ) ) );
    own_rock.set_owner( you );
    item &merchant_hammer = here.add_item_or_charges( position, item( itype_id( "hammer" ) ) );
    merchant_hammer.set_owner( faction_id( "free_merchants" ) );
    const item_location rock_location( map_cursor( position ), &own_rock );
    const item_location hammer_location( map_cursor( position ), &merchant_hammer );

    capture_pickup_selector selector( you );
    selector.set_title( "Pickup" );
    selector.add_map_items( position );

    REQUIRE( selector.highlight_one_of( { rock_location } ) );
    CAPTURE( selector.details() );
    CHECK( selector.focus().find( "Not owned" ) == std::string::npos );
    CHECK( selector.focus().find( "Goes into" ) == std::string::npos );
    CHECK( selector.details().find( "Goes into: backpack" ) != std::string::npos );

    REQUIRE( selector.highlight_one_of( { hammer_location } ) );
    CHECK( selector.focus().find( "Not owned" ) != std::string::npos );
    CHECK( selector.focus().find( "Owner:" ) == std::string::npos );
    CHECK( selector.details().find( "Owner: " ) != std::string::npos );
}

TEST_CASE( "multiselect_marking_speaks_resulting_capacity", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
    const item_location rock = you.i_add( item( itype_id( "rock" ) ) );
    REQUIRE( rock );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    capture_drop_selector selector( you );
    selector.set_title( "Drop" );
    selector.add_character_items( you );
    REQUIRE( selector.highlight_one_of( { rock } ) );
    selector.speak_focus();
    selector.select_focused( 1 );
    REQUIRE( !capture_ptr->messages.empty() );
    const std::string &marked = capture_ptr->messages.back();
    CHECK( marked.find( "1 entry selected. Weight " ) != std::string::npos );
    CHECK( marked.find( ", volume " ) != std::string::npos );
    CHECK( marked.find( "Over " ) == std::string::npos );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "pickup_speaks_one_result_with_destination_and_left_items", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    const auto pick_up = [&]( const std::string & type, int count ) {
        std::vector<item_location> targets;
        std::vector<int> quantities;
        item stack = item( itype_id( type ) );
        if( stack.count_by_charges() ) {
            stack.charges = count;
            count = 1;
        }
        for( int i = 0; i < count; ++i ) {
            item &placed = here.add_item( position, stack );
            placed.set_owner( you );
            targets.emplace_back( map_cursor( position ), &placed );
            quantities.push_back( 0 );
        }
        you.set_moves( 1000 );
        bool stash_successful = true;
        Pickup::pick_info info;
        accessibility::note_player_input();
        Pickup::do_pickup( targets, quantities, false, stash_successful, info );
        accessibility::flush_gameplay_announcements();
    };

    SECTION( "items without a pocket are named with the reason" ) {
        pick_up( "hammer", 1 );
        REQUIRE( !capture_ptr->messages.empty() );
        CHECK( capture_ptr->messages.back() == "Not picked up: hammer, no room." );
    }

    SECTION( "stored items name their container" ) {
        you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
        pick_up( "hammer", 2 );
        REQUIRE( !capture_ptr->messages.empty() );
        CHECK( capture_ptr->messages.back() == "Picked up: 2 hammers into backpack." );
        CHECK( accessibility::repeat_last_announcement() );
        CHECK( capture_ptr->messages.back() == "Picked up: 2 hammers into backpack." );
    }

    SECTION( "stacks counted in charges name the container" ) {
        you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
        pick_up( "rock", 2 );
        REQUIRE( !capture_ptr->messages.empty() );
        CHECK( capture_ptr->messages.back() == "Picked up: rocks (2) into backpack." );
    }

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "dropping_several_kinds_of_items_names_them", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    accessibility::note_player_input();
    put_into_vehicle_or_drop( you, item_drop_reason::deliberate, {
        item( itype_id( "hammer" ) ), item( itype_id( "rock" ) ), item( itype_id( "hammer" ) ),
        item( itype_id( "rock" ) )
    } );
    accessibility::flush_gameplay_announcements();
    REQUIRE( !capture_ptr->messages.empty() );
    CHECK( capture_ptr->messages.back() ==
           string_format( "Dropped on %s: 2 hammers, rocks (2).", here.name( you.pos_bub() ) ) );

    accessibility::reset_backend_after_testing();
}
