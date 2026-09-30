#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "accessibility.h"
#include "accessibility_format.h"
#include "accessibility_gameplay.h"
#include "action.h"
#include "activity_handlers.h"
#include "activity_actor_definitions.h"
#include "advanced_inv.h"
#include "advanced_inv_area.h"
#include "advanced_inv_pane.h"
#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "crafting_gui.h"
#include "dialogue.h"
#include "dialogue_win.h"
#include "faction.h"
#include "cata_catch.h"
#include "bionics.h"
#include "bionics_ui.h"
#include "bodygraph.h"
#include "bodypart.h"
#include "cata_scope_helpers.h"
#include "clzones.h"
#include "coordinates.h"
#include "game.h"
#include "fake_messages.h"
#include "help.h"
#include "input.h"
#include "input_context.h"
#include "input_enums.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_category.h"
#include "magic.h"
#include "map.h"
#include "map_helpers.h"
#include "map_item_stack.h"
#include "map_selector.h"
#include "martialarts.h"
#include "medical_ui.h"
#include "mission.h"
#include "mod_manager.h"
#include "monster.h"
#include "morale.h"
#include "morale_types.h"
#include "pickup.h"
#include "mutation.h"
#include "mutation_ui.h"
#include "npc.h"
#include "npc_opinion.h"
#include "omdata.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "player_activity.h"
#include "player_display.h"
#include "popup.h"
#include "proficiency.h"
#include "profession.h"
#include "player_helpers.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "scenario.h"
#include "point.h"
#include "skill.h"
#include "skill_ui.h"
#include "string_formatter.h"
#include "trade_ui.h"
#include "units_utility.h"
#include "type_id.h"
#include "veh_interact.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "worldfactory.h"

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

    input_context overmap( "OVERMAP", keyboard_mode::keycode );
    overmap.register_action( "LIST_NOTES" );
    overmap.register_action( "ACCESSIBILITY_KNOWN_PLACES" );
    const input_event alt_l( { keymod_t::alt }, 'l', input_event_t::keyboard_code );
    const input_event shift_l( { keymod_t::shift }, 'l', input_event_t::keyboard_code );
    CHECK( overmap.input_to_action( alt_l ) == "ACCESSIBILITY_KNOWN_PLACES" );
    CHECK( overmap.input_to_action( shift_l ) == "LIST_NOTES" );

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

TEST_CASE( "real_time_progress_obeys_silence_interval_deduplication_and_repeat",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    auto now = std::chrono::steady_clock::time_point{};
    accessibility::set_clock_for_testing( [&now]() {
        return now;
    } );
    accessibility::announce_status( "Loading" );
    accessibility::progress_reporter progress;
    progress.update( "Loading 10%" );
    now += std::chrono::milliseconds( 4999 );
    CHECK_FALSE( accessibility::poll_progress() );
    now += std::chrono::milliseconds( 1 );
    CHECK( accessibility::poll_progress() );
    REQUIRE( speech->messages.size() == 2 );
    CHECK( speech->messages.back() == "Loading 10%" );
    CHECK( speech->cancels == 0 );
    now += std::chrono::seconds( 20 );
    progress.update( "Loading 10%" );
    CHECK( speech->messages.size() == 2 );
    progress.update( "Loading 20%" );
    REQUIRE( speech->messages.size() == 3 );
    progress.update( "Loading 30%" );
    now += std::chrono::seconds( 4 );
    accessibility::note_player_input();
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( speech->messages.back() == "Loading 30%" );
    const int cancels_after_repeat = speech->cancels;
    // Silencing NVDA externally does not make the same text eligible again.
    now += std::chrono::seconds( 30 );
    CHECK_FALSE( accessibility::poll_progress() );
    CHECK( speech->messages.size() == 4 );
    progress.update( "Loading 40%" );
    CHECK( speech->messages.size() == 5 );
    CHECK( speech->cancels == cancels_after_repeat );

    accessibility::announce_focus( "Stop loading? Yes" );
    now += std::chrono::seconds( 30 );
    CHECK_FALSE( accessibility::poll_progress() );
    // A prompt suspends progress until the operation actually resumes.
    progress.update( "Loading 50%" );
    CHECK( speech->messages.back() == "Loading 50%" );
    accessibility::set_progress_announcement_interval( 0 );
    progress.update( "Loading 60%" );
    now += std::chrono::hours( 1 );
    CHECK_FALSE( accessibility::poll_progress() );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( speech->messages.back() == "Loading 60%" );
}

TEST_CASE( "progress_sources_end_with_their_owner_and_prioritize_warnings",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    auto now = std::chrono::steady_clock::time_point{};
    accessibility::set_clock_for_testing( [&now]() {
        return now;
    } );
    {
        accessibility::progress_reporter progress;
        progress.update( "Reading 10%" );
        now += std::chrono::seconds( 5 );
        accessibility::queue_gameplay_announcement( "Danger!",
                accessibility::gameplay_speech::meaningful );
        CHECK_FALSE( accessibility::poll_progress() );
        REQUIRE( speech->messages.size() == 1 );
        CHECK( speech->messages.back() == "Danger!" );
        now += std::chrono::seconds( 5 );
        CHECK( accessibility::poll_progress() );
        accessibility::set_progress_announcement_interval( 10 );
        progress.update( "Reading 20%" );
        now += std::chrono::seconds( 9 );
        CHECK_FALSE( accessibility::poll_progress() );
        now += std::chrono::seconds( 1 );
        CHECK( accessibility::poll_progress() );
        accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::off );
        progress.update( "Reading 30%" );
        now += std::chrono::seconds( 10 );
        CHECK_FALSE( accessibility::poll_progress() );
    }
    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::on );
    CHECK_FALSE( accessibility::poll_progress() );
    CHECK( speech->messages.size() == 3 );
}

TEST_CASE( "static_progress_popups_speak_one_start_and_rate_limited_updates",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    auto now = std::chrono::steady_clock::time_point{};
    accessibility::set_clock_for_testing( [&now]() {
        return now;
    } );
    {
        static_popup saving;
        saving.progress_message( "%s", "Saving 10%" );
        saving.progress_message( "%s", "Saving 20%" );
        REQUIRE( speech->messages.size() == 1 );
        CHECK( speech->messages.back() == "Saving" );
        CHECK( accessibility::last_announcement() == "Saving 20%" );
        now += std::chrono::seconds( 5 );
        CHECK( accessibility::poll_progress() );
        CHECK( speech->messages.back() == "Saving 20%" );
        saving.progress_message( "%s", "Saving 20%" );
        CHECK( speech->messages.size() == 2 );
    }
    now += std::chrono::seconds( 5 );
    CHECK_FALSE( accessibility::poll_progress() );
}

TEST_CASE( "activity_message_capture_keeps_results_without_turn_speech",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    {
        accessibility::game_message_speech_capture result( true );
        CHECK( accessibility::announce_game_message( "Finished waiting" ) );
        CHECK( accessibility::announce_game_message( "Finished waiting" ) );
        {
            accessibility::game_message_speech_suppression quiet;
            CHECK_FALSE( accessibility::announce_game_message( "Suppressed" ) );
        }
        CHECK_FALSE( accessibility::flush_gameplay_announcements() );
        CHECK( speech->messages.empty() );
        CHECK( result.messages() == std::vector<std::string> { "Finished waiting" } );
    }
    CHECK( accessibility::announce_game_message( "Normal gameplay" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages == std::vector<std::string> { "Normal gameplay." } );
}

TEST_CASE( "activity_wait_start_progress_completion_and_cancel_are_batched", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    set_test_message_observer( []( const std::string & message ) {
        accessibility::announce_game_message( message );
    } );
    accessibility_gameplay::reset_activity_speech();
    on_out_of_scope restore( []() {
        set_test_message_observer( {} );
        accessibility_gameplay::reset_activity_speech();
        accessibility::reset_backend_after_testing();
        clear_avatar();
    } );
    accessibility::set_progress_announcement_interval( 0 );
    you.assign_activity( activity_id( "ACT_WAIT" ), to_moves<int>( 2_minutes ) );
    REQUIRE( speech->messages.size() == 1 );
    CHECK( speech->messages[0].find( "Started: waiting" ) != std::string::npos );
    CHECK( speech->messages[0].find( "2 minutes" ) != std::string::npos );
    you.activity.have_fire = false;
    you.set_moves( 100 );
    you.activity.do_turn( you );
    CHECK( speech->messages.size() == 1 );
    CHECK( accessibility::last_announcement().find( "remaining" ) != std::string::npos );
    CHECK( accessibility::repeat_last_announcement() );
    REQUIRE( speech->messages.size() == 2 );
    CHECK( speech->messages.back().find( "remaining" ) != std::string::npos );
    you.activity.moves_left = 100;
    you.set_moves( 100 );
    you.activity.do_turn( you );
    CHECK_FALSE( you.activity );
    CHECK( speech->messages.size() == 2 );
    CHECK( accessibility::flush_gameplay_announcements() );
    REQUIRE( speech->messages.size() == 3 );
    CHECK( speech->messages.back() == "You finish waiting." );
    CHECK( accessibility::last_announcement() == "You finish waiting." );

    you.assign_activity( activity_id( "ACT_HACKSAW" ), to_moves<int>( 1_hours ) );
    accessibility_gameplay::activity_interruption_reason( "Zombie spotted!" );
    you.cancel_activity();
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages.back().find( "Stopped:" ) != std::string::npos );
    CHECK( speech->messages.back().find( "Zombie spotted!" ) != std::string::npos );
    you.assign_activity( activity_id( "ACT_HACKSAW" ), to_moves<int>( 1_hours ) );
    CHECK( speech->messages.back().find( "Resumed:" ) != std::string::npos );

    // Successful actor cleanup uses cancel_activity, as crafting does.
    {
        accessibility_gameplay::activity_turn_speech turn( you );
        accessibility_gameplay::activity_completed( you );
        you.cancel_activity();
        accessibility::announce_game_message( "You craft a knife." );
    }
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages.back() == "You craft a knife." );
}

TEST_CASE( "reading_failure_and_chapter_results_are_semantic_activity_outcomes", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    set_test_message_observer( []( const std::string & message ) {
        accessibility::announce_game_message( message );
    } );
    accessibility_gameplay::reset_activity_speech();
    on_out_of_scope restore( []() {
        set_test_message_observer( {} );
        accessibility_gameplay::reset_activity_speech();
        accessibility::reset_backend_after_testing();
        clear_avatar();
    } );
    accessibility::set_progress_announcement_interval( 0 );
    item_location book = you.i_add( item( itype_id( "child_book" ) ) );
    item_location ereader;
    REQUIRE( book );
    you.identify( *book );
    read_activity_actor actor( 10_minutes, book, ereader );
    you.assign_activity( actor );
    REQUIRE( speech->messages.size() == 1 );
    CHECK( speech->messages.back().find( book->tname() ) != std::string::npos );
    CHECK( speech->messages.back().find( "10 minutes" ) != std::string::npos );

    SECTION( "Reading stops with the actual vision failure" ) {
        {
            accessibility::game_message_speech_suppression setup;
            you.add_effect( efftype_id( "blind" ), 1_hours );
        }
        you.set_moves( 100 );
        you.activity.do_turn( you );
        CHECK_FALSE( you.activity );
        CHECK( accessibility::flush_gameplay_announcements() );
        REQUIRE( speech->messages.size() == 2 );
        CHECK( speech->messages.back() == "It's too dark to read!" );
        CHECK( speech->messages.back().find( "Finished" ) == std::string::npos );
    }
    SECTION( "A chapter handler reports its final result once" ) {
        {
            accessibility_gameplay::activity_turn_speech turn( you );
            turn.finish_cycle();
            book->set_flag( flag_id( "CAN_USE_IN_DARK" ) );
            actor.start( you.activity, you );
            actor.finish( you.activity, you );
        }
        CHECK_FALSE( you.activity );
        CHECK( accessibility::flush_gameplay_announcements() );
        REQUIRE( speech->messages.size() == 2 );
        CHECK( speech->messages.back().find( "You finish reading." ) != std::string::npos );
        CHECK( speech->messages.back().find( "Stopped:" ) == std::string::npos );
    }
    SECTION( "Continuous chapter results wait until the activity ends" ) {
        {
            accessibility_gameplay::activity_turn_speech turn( you );
            turn.finish_cycle();
            accessibility::announce_game_message( "You learned a recipe." );
        }
        CHECK_FALSE( accessibility::flush_gameplay_announcements() );
        CHECK( speech->messages.size() == 1 );
        {
            accessibility_gameplay::activity_turn_speech turn( you );
            turn.finish_cycle();
            accessibility::announce_game_message( "You finish reading." );
            you.activity.set_to_null();
        }
        CHECK( accessibility::flush_gameplay_announcements() );
        CHECK( speech->messages.back() == "You learned a recipe. You finish reading." );
    }
}

TEST_CASE( "sleep_start_repeat_wakeup_and_unsuccessful_attempt_are_announced", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    set_test_message_observer( []( const std::string & message ) {
        accessibility::announce_game_message( message );
    } );
    accessibility_gameplay::reset_activity_speech();
    on_out_of_scope restore( []() {
        set_test_message_observer( {} );
        accessibility_gameplay::reset_activity_speech();
        accessibility::reset_backend_after_testing();
        clear_avatar();
    } );
    accessibility::set_progress_announcement_interval( 0 );
    you.assign_activity( try_sleep_activity_actor( 10_minutes ) );
    REQUIRE( speech->messages.size() == 1 );
    {
        accessibility_gameplay::activity_turn_speech turn( you );
        you.fall_asleep( 1_hours );
    }
    CHECK( accessibility::flush_gameplay_announcements() );
    REQUIRE( speech->messages.size() == 2 );
    CHECK( speech->messages.back().find( "You fall asleep." ) != std::string::npos );
    CHECK( speech->messages.back().find( "You fall asleep.",
                                         speech->messages.back().find( "You fall asleep." ) + 1 ) == std::string::npos );
    accessibility_gameplay::update_activity_progress( you );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( speech->messages.back() == "Sleeping" );
    you.wake_up();
    you.wake_up();
    you.remove_effect( efftype_id( "sleep" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages.back().find( "You wake up." ) != std::string::npos );
    CHECK( speech->messages.back().find( "You wake up.",
                                         speech->messages.back().find( "You wake up." ) + 1 ) == std::string::npos );
    CHECK( speech->messages.size() == 4 );
    try_sleep_activity_actor attempt( 10_minutes );
    you.assign_activity( attempt );
    {
        accessibility_gameplay::activity_turn_speech turn( you );
        turn.finish_cycle();
        attempt.finish( you.activity, you );
    }
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages.back().find( "You try to sleep, but can't." ) != std::string::npos );
}

TEST_CASE( "continuous_activity_actor_replacement_keeps_one_start_and_aggregates_results",
           "[accessibility]" )
{
    clear_avatar();
    avatar &you = get_avatar();
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    accessibility_gameplay::reset_activity_speech();
    on_out_of_scope restore( []() {
        accessibility_gameplay::reset_activity_speech();
        accessibility::reset_backend_after_testing();
        clear_avatar();
    } );
    accessibility::set_progress_announcement_interval( 0 );
    you.assign_activity( activity_id( "ACT_HACKSAW" ), to_moves<int>( 1_hours ) );
    REQUIRE( speech->messages.size() == 1 );
    {
        accessibility_gameplay::activity_turn_speech turn( you );
        accessibility_gameplay::activity_completed( you );
        you.cancel_activity();
        accessibility::announce_game_message( "You craft the first knife." );
        you.assign_activity( activity_id( "ACT_HACKSAW" ), to_moves<int>( 1_hours ) );
    }
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages.size() == 1 );
    {
        accessibility_gameplay::activity_turn_speech turn( you );
        accessibility_gameplay::activity_completed( you );
        you.cancel_activity();
        accessibility::announce_game_message( "You craft the second knife." );
    }
    CHECK( accessibility::flush_gameplay_announcements() );
    REQUIRE( speech->messages.size() == 2 );
    CHECK( speech->messages.back() == "You craft the first knife. You craft the second knife." );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( speech->messages.size() == 3 );
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
                                 "", "Load", accessibility::format_world_row( "Empty", 0 ) ) );
    accessibility::announce( accessibility::format_main_menu_focus(
                                 "", "World", accessibility::format_world_row( "Survival", 3 ) ) );
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
                                 "Main menu", "World", accessibility::format_world_row( "Survival", 3 ) ) );

    REQUIRE( capture_ptr->messages.size() == 11 );
    CHECK( capture_ptr->messages[0] == "Cataclysm: Dark Days Ahead. Version: 0.I. "
           "Tip of the day: Find shelter. Bugs? Use MOTD to report them. "
           "Main menu. New Game. Custom Character. "
           "Customize the character." );
    CHECK( capture_ptr->messages[1] == "Preset Character. Choose a template." );
    CHECK( capture_ptr->messages[2] ==
           "Tip of the day. Find shelter. Bugs? Use MOTD to report them." );
    CHECK( capture_ptr->messages[3] == "Load. Empty, no characters." );
    CHECK( capture_ptr->messages[4] == "World. Survival, 3 characters." );
    CHECK( capture_ptr->messages[5] == "Load. No worlds to load." );
    CHECK( capture_ptr->messages[6] == "MOTD. First MOTD line." );
    CHECK( capture_ptr->messages[7] == long_line );
    CHECK( capture_ptr->messages[8] == long_line );
    CHECK( capture_ptr->messages[9] == "Credits. Author: Ada." );
    CHECK( capture_ptr->messages[10] == "Main menu. World. Survival, 3 characters." );

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

TEST_CASE( "tutorial_document_expands_current_close_and_gameplay_bindings", "[accessibility]" )
{
    std::string lesson = "Press <keybind:HELP_DOCUMENT:QUIT> to close this lesson.\n"
                         "Move east with <keybind:RIGHT>.";
    replace_keybind_tag( lesson );
    CHECK( lesson.find( "<keybind:" ) == std::string::npos );
    const auto fragments = accessibility::format_document_paragraphs( lesson );
    REQUIRE( fragments.size() == 2 );
    input_context document( "HELP_DOCUMENT" );
    const auto close_keys = document.keys_bound_to( "QUIT", -1, false, false );
    REQUIRE_FALSE( close_keys.empty() );
    CHECK( fragments.front().find( close_keys.front().long_description() ) != std::string::npos );
    CHECK( fragments.back().find( "Move east with " ) == 0 );
}

TEST_CASE( "tutorial_document_repeat_and_log_do_not_reread_the_entire_lesson",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    const std::string lesson = "First instruction.\nSecond instruction.\nMove east to continue.";
    const auto fragments = accessibility::format_document_paragraphs( lesson );
    REQUIRE( fragments.size() == 3 );
    accessibility::announce( accessibility::format_document_focus( "Tutorial", fragments.front() ) );
    accessibility::announce( fragments.back() );
    CHECK( accessibility::repeat_last_announcement() );
    {
        accessibility::game_message_speech_suppression quiet;
        CHECK_FALSE( accessibility::announce_game_message( lesson ) );
    }
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    CHECK( accessibility::last_announcement() == fragments.back() );
    CHECK( accessibility::repeat_last_announcement() );
    const std::vector<std::string> expected = {
        "Tutorial. First instruction.", "Move east to continue.",
        "Move east to continue.", "Move east to continue."
    };
    CHECK( capture_ptr->messages == expected );
    CHECK( capture_ptr->cancels == 0 );
    accessibility::reset_backend_after_testing();
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

TEST_CASE( "target_focus_names_target_obstacle_and_creatures_in_line_of_fire",
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
    const tripoint_bub_ms target_pos = origin + tripoint( 4, 0, 0 );
    const std::vector<tripoint_bub_ms> trajectory = {
        origin + tripoint::east, origin + tripoint( 2, 0, 0 ), origin + tripoint( 3, 0, 0 ), target_pos
    };
    monster &target = spawn_test_monster( "mon_zombie", target_pos );
    spawn_test_monster( "mon_zombie", origin + tripoint::east );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );

    accessibility::target_focus focus = accessibility_gameplay::target_focus( here, viewer, origin,
                                        target_pos, trajectory, &target, nullptr, true );
    CHECK( focus.target == "zombie" );
    CHECK( focus.attitude == "Hostile" );
    CHECK( focus.location == "4 tiles east" );
    CHECK( focus.tile.back() == "4 tiles east" );
    CHECK( focus.blocked_by.empty() );
    CHECK( focus.in_line_of_fire == std::vector<std::string> { "zombie" } );

    const std::vector<std::string> details = accessibility_gameplay::target_details( here, viewer,
            &target, nullptr );
    REQUIRE( !details.empty() );
    CHECK( details.front().rfind( "zombie. Attitude: Hostile.", 0 ) == 0 );

    // A wall between the creature in line of fire and the target blocks the shot.
    here.ter_set( origin + tripoint( 2, 0, 0 ), ter_str_id( "t_wall" ) );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    focus = accessibility_gameplay::target_focus( here, viewer, origin, target_pos, trajectory,
            &target, nullptr, true );
    CHECK( focus.blocked_by == here.tername( origin + tripoint( 2, 0, 0 ) ) );
    CHECK( focus.in_line_of_fire.size() == 1 );

    focus = accessibility_gameplay::target_focus( here, viewer, origin, target_pos, trajectory,
            &target, nullptr, false );
    CHECK( focus.blocked_by.empty() );
    CHECK( focus.in_line_of_fire.empty() );

    // Without a target, the cursor tile is described as look-around does.
    focus = accessibility_gameplay::target_focus( here, viewer, origin, origin, {}, nullptr, nullptr,
            false );
    CHECK( focus.target.empty() );
    CHECK( focus.tile.back() == "here" );

    // The wall hides the target, which is then only sensed.
    CHECK( accessibility_gameplay::target_focus( here, viewer, origin, target_pos, trajectory,
            &target, nullptr, false ).target == "sensed creature" );
    CHECK( accessibility_gameplay::target_details( here, viewer, &target, nullptr ) ==
           std::vector<std::string> { "You sense a creature here, but cannot see it." } );
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

TEST_CASE( "crafting_speech_names_missing_requirements_and_disassembly_tools", "[accessibility]" )
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
    avatar &you = get_avatar();
    map &here = get_map();
    const recipe &soldering_iron = recipe_id( "test_soldering_iron" ).obj();

    you.invalidate_crafting_inventory();
    std::vector<accessibility::requirement_group> groups = accessibility_gameplay::requirement_groups(
                soldering_iron.simple_requirements(), you.crafting_inventory(),
                soldering_iron.get_component_filter(), 1, true );
    REQUIRE( groups.size() == 7 );
    CHECK( accessibility::count_missing_requirement_groups( groups ) == 7 );
    std::vector<std::string> lines;
    for( const accessibility::requirement_group &group : groups ) {
        lines.push_back( accessibility::format_requirement_group( group ) );
    }
    CHECK( lines[0] == "Tools: 1 tool with cutting of 2 or more, missing." );
    CHECK( lines[1] == "Tools: 1 tool with screw driving of 1 or more, missing." );
    CHECK( lines[4] == "Components: 1 scrap metal, missing." );
    // Without the knowledge of the primary skill, the recipe lacks one more requirement.
    CHECK( recipe_accessibility_row( soldering_iron, you ) == "TEST soldering iron, 7 missing." );

    here.add_item( you.pos_bub( here ), item( itype_id( "scrap" ), calendar::turn, 1 ) );
    here.add_item( you.pos_bub( here ), item( itype_id( "e_scrap" ) ) );
    here.add_item( you.pos_bub( here ), item( itype_id( "e_scrap" ) ) );
    you.invalidate_crafting_inventory();
    groups = accessibility_gameplay::requirement_groups( soldering_iron.simple_requirements(),
             you.crafting_inventory(), soldering_iron.get_component_filter(), 1, true );
    REQUIRE( groups.size() == 7 );
    CHECK( accessibility::count_missing_requirement_groups( groups ) == 5 );
    // Available alternatives are listed first.
    CHECK( accessibility::format_requirement_group( groups[2] ) ==
           "Components: 2 electronic scraps (have 2), have; or 1 glow plug, missing." );
    CHECK( accessibility::format_requirement_group( groups[4] ) ==
           "Components: 1 scrap metal (have 1), have." );
    // Twice the batch needs more scrap than the avatar has.
    groups = accessibility_gameplay::requirement_groups( soldering_iron.simple_requirements(),
             you.crafting_inventory(), soldering_iron.get_component_filter(), 2, true );
    CHECK( accessibility::format_requirement_group( groups[4] ) ==
           "Components: 2 scrap metal (have 1), missing." );
    CHECK( recipe_accessibility_row( soldering_iron, you, 2 ).rfind( "Batch size 2, ", 0 ) == 0 );

    const std::vector<std::string> details = recipe_accessibility_details( soldering_iron, you );
    REQUIRE( !details.empty() );
    CHECK( details.front() == "5 missing." );
    const auto has_line = [&details]( const std::string & line ) {
        return std::find( details.begin(), details.end(), line ) != details.end();
    };
    CHECK( has_line( "Tools: 1 tool with cutting of 2 or more, missing." ) );
    CHECK( has_line( "Components: 1 scrap metal (have 1), have." ) );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.rfind( "Primary skill: ", 0 ) == 0;
    } ) );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.rfind( "Time to complete: ", 0 ) == 0;
    } ) );

    // Disassembly lists what it returns and the tools it needs.
    const recipe &uncraft = recipe_dictionary::get_uncraft( itype_id( "test_soldering_iron" ) );
    REQUIRE( !uncraft.is_null() );
    const std::vector<std::string> disassembly = accessibility_gameplay::disassembly_details(
                uncraft, "2 electronic scrap", "20 minutes", you.crafting_inventory() );
    REQUIRE( disassembly.size() >= 3 );
    CHECK( disassembly[0] == "Returns: 2 electronic scrap." );
    CHECK( disassembly[1] == "Time: 20 minutes." );
    CHECK( disassembly[2] == "Tools: 1 tool with cutting of 2 or more, missing." );
    CHECK( std::none_of( disassembly.begin(), disassembly.end(), []( const std::string & line ) {
        return line.rfind( "Components:", 0 ) == 0;
    } ) );
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

TEST_CASE( "character_sheet_speaks_tabs_rows_categories_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    const int stats = 0;
    const int encumbrance = 1;
    const int skills = 3;
    const int traits = 4;
    const int bionics = 5;
    const int effects = 6;
    const int proficiencies = 7;

    you.str_max = 10;
    you.set_str_bonus( -2 );
    std::vector<std::string> rows = character_sheet_accessibility_rows( you, stats );
    REQUIRE( rows.size() == 9 );
    CHECK( rows[0] == "Strength 8, base 10." );
    CHECK( rows[8].rfind( "Blood type, ", 0 ) == 0 );
    CHECK( character_sheet_accessibility_tab( you, stats ) == "Stats. 9 items. Strength 8, base 10." );
    const std::string summary = character_sheet_accessibility_summary( you, stats, 0 );
    CHECK( summary.find( you.get_name() ) == 0 );
    CHECK( summary.find( "Stats. 1 of 9. Strength 8, base 10." ) != std::string::npos );
    std::vector<std::string> details = character_sheet_accessibility_details( you, stats, 0 );
    REQUIRE( details.size() >= 3 );
    CHECK( details[0] == "Strength 8, base 10. 1 of 9." );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.rfind( "Base HP: ", 0 ) == 0;
    } ) );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }

    // Encumbrance rows merge matching left and right parts as the sheet does.
    rows = character_sheet_accessibility_rows( you, encumbrance );
    const std::string torso_row = "torso, encumbrance 0, warmth comfortable.";
    const auto torso = std::find( rows.begin(), rows.end(), torso_row );
    REQUIRE( torso != rows.end() );
    CHECK( std::any_of( rows.begin(), rows.end(), []( const std::string & row ) {
        return row.rfind( "arms, encumbrance 0", 0 ) == 0;
    } ) );
    details = character_sheet_accessibility_details( you, encumbrance,
              static_cast<unsigned>( torso - rows.begin() ) );
    REQUIRE( !details.empty() );
    CHECK( details[0].rfind( "torso, encumbrance 0, layering 0, total 0, warmth ", 0 ) == 0 );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.find( " score: " ) != std::string::npos;
    } ) );

    // Skill rows name their category when focus enters it; details read the skill.
    const skill_id melee( "melee" );
    you.set_skill_level( melee, 2 );
    you.set_knowledge_level( melee, 3 );
    const std::vector<HeaderSkill> headers = get_HeaderSkills( Skill::get_skills_sorted_by(
    []( const Skill & a, const Skill & b ) {
        return a.get_sort_rank() < b.get_sort_rank();
    } ) );
    rows = character_sheet_accessibility_rows( you, skills );
    size_t row = 0;
    bool category_start = false;
    unsigned melee_line = 0;
    for( unsigned line = 0; line < headers.size(); ++line ) {
        if( headers[line].is_header ) {
            category_start = true;
            continue;
        }
        REQUIRE( row < rows.size() );
        const std::string category = SkillDisplayType::get_skill_type(
                                         headers[line].skill->display_category() ).display_string();
        CHECK( ( rows[row].rfind( category + ". ", 0 ) == 0 ) == category_start );
        if( headers[line].skill->ident() == melee ) {
            CHECK( rows[row].find( "melee 3, 0%, rusty, training." ) != std::string::npos );
            melee_line = line;
        }
        category_start = false;
        ++row;
    }
    CHECK( row == rows.size() );
    CHECK( character_sheet_accessibility_tab( you, skills ).rfind(
               string_format( "Skills. %d items. ", static_cast<int>( rows.size() ) ), 0 ) == 0 );
    REQUIRE( melee_line > 0 );
    details = character_sheet_accessibility_details( you, skills, melee_line );
    REQUIRE( !details.empty() );
    CHECK( details[0].find( "melee 3, 0%, rusty, training." ) != std::string::npos );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.rfind( "Practical level: 2", 0 ) == 0;
    } ) );

    // Traits, proficiencies, and effects by name and state; empty tabs say so.
    CHECK( character_sheet_accessibility_tab( you, traits ) == "Traits. No items." );
    const trait_id good_hearing( "GOODHEARING" );
    you.set_mutation( good_hearing );
    rows = character_sheet_accessibility_rows( you, traits );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == string_format( "%s, positive.", good_hearing->name() ) );
    details = character_sheet_accessibility_details( you, traits, 0 );
    REQUIRE( details.size() >= 2 );
    CHECK( details[0] == rows[0] + " 1 of 1." );

    you.add_proficiency( proficiency_id( "prof_knapping" ), true );
    rows = character_sheet_accessibility_rows( you, proficiencies );
    CHECK( std::find( rows.begin(), rows.end(), "Basic Knapping, known." ) != rows.end() );

    const std::string bionics_tab = character_sheet_accessibility_tab( you, bionics );
    CHECK( bionics_tab.rfind( "Bionics. Power ", 0 ) == 0 );
    CHECK( bionics_tab.find( "No items." ) != std::string::npos );

    you.add_effect( efftype_id( "downed" ), 1_minutes );
    rows = character_sheet_accessibility_rows( you, effects );
    CHECK( std::find( rows.begin(), rows.end(), "Downed." ) != rows.end() );
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

TEST_CASE( "bionics_screen_speaks_tabs_rows_power_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();

    CHECK( bionics_accessibility_tab( you, true ) == "Active bionics. No items." );
    CHECK( bionics_accessibility_tab( you, false ) == "Passive bionics. No items." );

    const bionic_id power_storage( "bio_power_storage" );
    const bionic_id night_vision( "bio_night_vision" );
    you.add_bionic( power_storage );
    you.add_bionic( night_vision );
    you.set_power_level( 0_kJ );

    // Without power the activatable bionic says why it cannot be activated.
    std::vector<std::string> rows = bionics_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == "Implanted Night Vision, off, 10 J to activate, 10 J per turn. "
           "Unavailable: not enough power." );
    CHECK( bionics_accessibility_tab( you, true ) == "Active bionics. 1 item. " + rows[0] );
    rows = bionics_accessibility_rows( you, false );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == "Power Storage." );
    CHECK( bionics_accessibility_tab( you, false ) == "Passive bionics. 1 item. Power Storage." );

    you.set_power_level( 50_kJ );
    rows = bionics_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == "Implanted Night Vision, off, 10 J to activate, 10 J per turn." );

    // Activation and deactivation change the spoken state.
    std::optional<bionic *> bio = you.find_bionic_by_type( night_vision );
    REQUIRE( bio );
    REQUIRE( you.activate_bionic( **bio ) );
    rows = bionics_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == "Implanted Night Vision, on, 10 J to activate, 10 J per turn." );
    REQUIRE( you.deactivate_bionic( **bio ) );
    rows = bionics_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0].rfind( "Implanted Night Vision, off, ", 0 ) == 0 );

    const std::string summary = bionics_accessibility_summary( you, true, 0, false );
    CHECK( summary.rfind( "Bionic power ", 0 ) == 0 );
    CHECK( summary.find( " of 100 kJ." ) != std::string::npos );
    CHECK( summary.find( "Activating. Active bionics. 1 of 1. Key " ) != std::string::npos );
    CHECK( bionics_accessibility_summary( you, false, 0, true ).find(
               "Examining. Passive bionics. 1 of 1." ) != std::string::npos );

    const std::vector<std::string> details = bionics_accessibility_details( you, true, 0 );
    REQUIRE( details.size() >= 3 );
    CHECK( details[0].rfind( rows[0] + " 1 of 1. Key ", 0 ) == 0 );
    CHECK( details[1] == "Power usage: 10 J act, 10 J/turn, OFF." );
    CHECK( details[2] == "When active, this bionic will amplify existing light, allowing you to "
           "see in the dark." );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }
    CHECK( bionics_accessibility_details( you, true, 1 ).empty() );
}

TEST_CASE( "mutations_screen_speaks_columns_rows_resources_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();

    CHECK( mutations_accessibility_tab( you, true ) == "Active mutations. No items." );

    const trait_id photophore( "BIOLUM1" );
    const trait_id good_hearing( "GOODHEARING" );
    you.set_mutation( photophore );
    you.set_mutation( good_hearing );
    you.set_stored_kcal( you.get_healthy_kcal() );

    std::vector<std::string> rows = mutations_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    const std::string photophore_row = string_format( "%s, off, 9 kcal, cooldown ",
                                       you.mutation_name( photophore ) );
    CHECK( rows[0].rfind( photophore_row, 0 ) == 0 );
    CHECK( rows[0].find( "Unavailable" ) == std::string::npos );
    CHECK( mutations_accessibility_tab( you, true ) == "Active mutations. 1 item. " + rows[0] );
    rows = mutations_accessibility_rows( you, false );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == string_format( "%s.", you.mutation_name( good_hearing ) ) );

    // A starting trait and a hidden sprite are spoken, not shown only by color.
    you.my_traits.insert( good_hearing );
    you.cached_mutations[good_hearing].show_sprite = false;
    rows = mutations_accessibility_rows( you, false );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == string_format( "%s, starting trait, sprite hidden.",
                                     you.mutation_name( good_hearing ) ) );

    // Too hungry to activate: the row names the missing resource.
    you.set_stored_kcal( you.get_healthy_kcal() / 2 );
    rows = mutations_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0].rfind( photophore_row, 0 ) == 0 );
    CHECK( rows[0].find( ". Unavailable: not enough kcal." ) != std::string::npos );

    you.cached_mutations[photophore].powered = true;
    rows = mutations_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0].rfind( string_format( "%s, on, 9 kcal, cooldown ",
                                         you.mutation_name( photophore ) ), 0 ) == 0 );
    CHECK( rows[0].find( "Unavailable" ) == std::string::npos );

    CHECK( mutations_accessibility_summary( you, true, 0, false ).rfind(
               "Activating. Active mutations. 1 of 1. ", 0 ) == 0 );
    CHECK( mutations_accessibility_summary( you, false, 0, true ) ==
           "Examining. Passive mutations. 1 of 1. No key." );

    const std::vector<std::string> details = mutations_accessibility_details( you, photophore );
    REQUIRE( details.size() >= 2 );
    CHECK( details[0].rfind( rows[0] + " Active mutations. 1 of 1. ", 0 ) == 0 );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.find( "A photophore has grown from your head" ) != std::string::npos;
    } ) );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }
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

TEST_CASE( "spell_menu_speaks_rows_unavailable_reason_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    const spell_id pew( "test_spell_pew" );
    you.magic->learn_spell( pew, you, true );
    spell &sp = you.magic->get_spell( pew );

    you.magic->set_mana( 10000 );
    const std::string castable = spell_accessibility_row( sp, you, false );
    CHECK( castable.rfind( "Pew, Pew, Classless, castable, ", 0 ) == 0 );
    CHECK( castable.find( " mana, " ) != std::string::npos );
    CHECK( castable.find( "Unavailable" ) == std::string::npos );
    const std::string favorite = spell_accessibility_row( sp, you, true );
    CHECK( favorite.size() > 11 );
    CHECK( favorite.compare( favorite.size() - 11, 11, ", favorite." ) == 0 );

    // Without mana the row says why the menu disables the spell.
    you.magic->set_mana( 0 );
    const std::string unavailable = spell_accessibility_row( sp, you, false );
    CHECK( unavailable.rfind( "Pew, Pew, Classless, ", 0 ) == 0 );
    CHECK( unavailable.find( "castable" ) == std::string::npos );
    CHECK( unavailable.find( ". Unavailable: not enough mana." ) != std::string::npos );

    const std::vector<std::string> details = spell_accessibility_details( sp, you, false, 'a' );
    REQUIRE( details.size() >= 5 );
    CHECK( details[0] == unavailable + " Key a." );
    CHECK( details[1] == "Classless" );
    CHECK( details[2] == "You aim your finger at your opponent and make 'Pew, pew' sounds." );
    const auto has_line = [&details]( const std::string & prefix ) {
        return std::any_of( details.begin(), details.end(), [&prefix]( const std::string & line ) {
            return line.rfind( prefix, 0 ) == 0;
        } );
    };
    CHECK( has_line( "Spell Level: " ) );
    CHECK( has_line( "Not Enough mana: " ) );
    CHECK( has_line( "Casting Time: " ) );
    CHECK( has_line( "Valid Targets: " ) );
    CHECK( has_line( "Range: " ) );
    CHECK( has_line( "Damage: " ) );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }
    CHECK( spell_accessibility_details( sp, you, false, 0 )[0] == unavailable + " No key." );
}

TEST_CASE( "martial_style_details_speak_buffs_techniques_and_weapons", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    item hammer( itype_id( "hammer" ) );
    REQUIRE( you.wield( hammer ) );
    you.wear_item( item( itype_id( "test_backpack" ) ), false, false );
    // Carried weapons are shown only by color; the document names them.
    you.i_add( item( itype_id( "test_weapon1" ) ) );
    REQUIRE( you.get_wielded_item() );
    REQUIRE( you.get_wielded_item()->typeId() == itype_id( "hammer" ) );

    const std::vector<std::string> lines =
        martial_art_accessibility_details( matype_id( "test_style_ma1" ) );
    const auto has_line = [&lines]( const std::string & text ) {
        return std::any_of( lines.begin(), lines.end(), [&text]( const std::string & line ) {
            return line.find( text ) != std::string::npos;
        } );
    };
    REQUIRE( !lines.empty() );
    CHECK( lines[0] == "General info" );
    CHECK( has_line( "This style can be used with all weapons." ) );
    CHECK( has_line( "Buffs (" ) );
    CHECK( has_line( "Miss buffs" ) );
    CHECK( has_line( "Techniques (" ) );
    CHECK( has_line( "Technique: " ) );
    CHECK( has_line( "Weapons (" ) );
    CHECK( has_line( "test melee weapon 1 (carried)" ) );
    for( const std::string &line : lines ) {
        CHECK( line.find( "<color" ) == std::string::npos );
        CHECK( line.find( "</" ) == std::string::npos );
    }
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

TEST_CASE( "morale_window_reads_total_sources_and_focus_as_a_document", "[accessibility]" )
{
    player_morale morale;
    std::vector<std::string> document = morale.accessibility_document( 10, 0, 0 );
    REQUIRE( document.size() == 3 );
    CHECK( document[0] == "Total morale: 0." );
    CHECK( document[1] == "Nothing affects your morale." );
    CHECK( document[2] == "Focus trends towards: 10." );

    morale.add( morale_type( "morale_food_good" ), 20 );
    morale.add( morale_type( "morale_food_bad" ), -10 );
    document = morale.accessibility_document( 50, 5, 0 );
    REQUIRE( document.size() >= 6 );
    CHECK( document[0].rfind( "Total morale: +", 0 ) == 0 );
    CHECK( document[1] == "Total positive morale, +20." );
    CHECK( document[2].find( ", 100%." ) != std::string::npos );
    CHECK( document[3] == "Total negative morale, -10." );
    CHECK( document[4].find( ", 100%." ) != std::string::npos );
    CHECK( document[5] == "Pain level: -5." );
    CHECK( document.back() == "Focus trends towards: 50." );
}

TEST_CASE( "body_status_speaks_parts_health_nested_graphs_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    const bodypart_id torso( "torso" );
    you.set_part_hp_cur( torso, you.get_part_hp_max( torso ) / 2 );
    // Speech keeps the bar's precision: half health may show as four tenths.
    const std::string health = accessibility::format_health_bar( get_hp_bar(
                                   you.get_part_hp_cur( torso ), you.get_part_hp_max( torso ) ).first );
    CHECK( ( health == "health 4 of 10" || health == "health 5 of 10" ) );

    const std::vector<std::string> rows = bodygraph_accessibility_rows( you );
    REQUIRE( !rows.empty() );
    const std::string torso_name = uppercase_first_letter( torso->name.translated() );
    const auto torso_row = std::find_if( rows.begin(), rows.end(), [&]( const std::string & row ) {
        return row.rfind( torso_name + ",", 0 ) == 0;
    } );
    REQUIRE( torso_row != rows.end() );
    CHECK( *torso_row == torso_name + ", " + health + ", has details." );
    CHECK( std::count_if( rows.begin(), rows.end(), []( const std::string & row ) {
        return row.find( "health 10 of 10, has details." ) != std::string::npos;
    } ) > 0 );
    CHECK( bodygraph_accessibility_open( you ).rfind( string_format( "Body status. %d items. ",
            static_cast<int>( rows.size() ) ), 0 ) == 0 );

    // A nested graph is named after its body part.
    const bodypart_id arm_l( "arm_l" );
    const std::string nested = bodygraph_accessibility_open( you, bodygraph_id( "arm_l" ) );
    CHECK( nested.rfind( uppercase_first_letter( arm_l->name.translated() ) + ". ", 0 ) == 0 );

    const std::vector<std::string> details = bodygraph_accessibility_details( you,
            bodygraph_id::NULL_ID(), static_cast<int>( torso_row - rows.begin() ) );
    REQUIRE( details.size() >= 4 );
    CHECK( details[0].rfind( *torso_row + " ", 0 ) == 0 );
    CHECK( std::find( details.begin(), details.end(), "Health: " + health + "." ) != details.end() );
    CHECK( std::find( details.begin(), details.end(), "Encumbrance: 0." ) != details.end() );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }
}

TEST_CASE( "medical_menu_speaks_columns_conditions_stats_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    const bodypart_id torso( "torso" );

    const std::string open = medical_accessibility_open( you );
    CHECK( open.rfind( "Medical. ", 0 ) == 0 );
    CHECK( open.find( "HEALTH. " ) != std::string::npos );
    CHECK( medical_accessibility_column( you, 1 ) == "EFFECTS. 1 item. None." );

    you.add_effect( efftype_id( "bleed" ), 10_minutes, torso );
    std::vector<std::string> rows = medical_accessibility_rows( you, 0 );
    const std::string torso_name = uppercase_first_letter( body_part_name( torso, 1 ) );
    const auto torso_row = std::find_if( rows.begin(), rows.end(), [&]( const std::string & row ) {
        return row.rfind( torso_name + ",", 0 ) == 0;
    } );
    REQUIRE( torso_row != rows.end() );
    CHECK( torso_row->rfind( torso_name + ", health 10 of 10, ", 0 ) == 0 );
    CHECK( torso_row->find( "<color" ) == std::string::npos );
    CHECK( torso_row->find( '[' ) == std::string::npos );

    const int torso_index = static_cast<int>( torso_row - rows.begin() );
    const std::vector<std::string> details = medical_accessibility_details( you, 0, torso_index );
    REQUIRE( details.size() >= 3 );
    CHECK( details[0].find( string_format( "%d of %d", torso_index + 1,
                                           static_cast<int>( rows.size() ) ) ) != std::string::npos );
    CHECK( std::find( details.begin(), details.end(),
                      to_upper_case( torso_name ) + " STATS." ) != details.end() );

    rows = medical_accessibility_rows( you, 2 );
    REQUIRE( rows.size() == 6 );
    CHECK( rows[2] == string_format( "Strength %d, base %d.", you.get_str(), you.get_str_base() ) );
    CHECK( rows[0].rfind( "Base Move Cost: ", 0 ) == 0 );
}

TEST_CASE( "proficiencies_window_speaks_categories_progress_filter_and_details",
           "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    const proficiency_id knapping( "prof_knapping" );
    const proficiency_id leather( "prof_leatherworking_basic" );
    you.add_proficiency( knapping, true );
    you.set_proficiency_practice( leather, leather->time_to_learn() / 2 );

    const std::vector<std::string> rows = proficiencies_window_accessibility_rows( you, 0 );
    REQUIRE( rows.size() == 2 );
    CHECK( rows[0] == knapping->name() + ", known." );
    CHECK( rows[1] == string_format( "%s, learning, %d%%.", leather->name(),
                                     static_cast<int>( std::floor( you.get_proficiency_practice( leather ) * 100 ) ) ) );
    CHECK( proficiencies_window_accessibility_tab( you, 0 ) == "ALL. 2 items. " + rows[0] );

    const std::vector<std::string> details = proficiencies_window_accessibility_details( you, 0, 1 );
    REQUIRE( details.size() >= 6 );
    CHECK( details[0] == rows[1] + " 2 of 2." );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.rfind( "Progress: ", 0 ) == 0;
    } ) );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.rfind( "Description: ", 0 ) == 0;
    } ) );

    CHECK( proficiencies_window_accessibility_filter( you, "l:*" ) ==
           "Filter: l:*. 1 matching item. " + rows[0] );
}

TEST_CASE( "sort_armor_speaks_conflicts_neighbours_summary_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    const bodypart_id torso( "torso" );
    you.wear_item( item( itype_id( "longshirt" ) ), false );
    you.wear_item( item( itype_id( "hoodie" ) ), false );
    you.wear_item( item( itype_id( "sweatshirt" ) ), false );

    // The hoodie and sweatshirt share a layer; the list shows it only by color.
    const std::vector<std::string> rows = you.worn.sort_armor_accessibility_rows( you, torso );
    REQUIRE( rows.size() == 3 );
    CHECK( rows[0].find( "layering conflict" ) == std::string::npos );
    CHECK( rows[1].find( "layering conflict" ) != std::string::npos );
    CHECK( rows[2].find( "layering conflict" ) != std::string::npos );
    CHECK( you.worn.sort_armor_accessibility_tab( you, torso ).find( ". 3 items. " + rows[0] ) !=
           std::string::npos );
    CHECK( you.worn.sort_armor_accessibility_tab( you, bodypart_str_id::NULL_ID() ).rfind(
               "All. 3 items. ", 0 ) == 0 );

    // Moving reports the neighbouring items at each end and in the middle.
    CHECK( you.worn.sort_armor_accessibility_position( torso, 0 ).rfind( "innermost, above ",
            0 ) == 0 );
    CHECK( you.worn.sort_armor_accessibility_position( torso, 1 ).rfind( "above ", 0 ) == 0 );
    CHECK( you.worn.sort_armor_accessibility_position( torso, 2 ).rfind( "outermost, below ",
            0 ) == 0 );

    const std::string summary = you.worn.sort_armor_accessibility_summary( you, torso );
    CHECK( summary.find( "3 items. " ) != std::string::npos );
    CHECK( summary.find( "encumbrance " ) != std::string::npos );
    CHECK( summary.find( "layering conflict" ) != std::string::npos );
    CHECK( summary.find( string_format( "Total encumbrance %d, layering %d.",
                                        you.get_part_encumbrance( torso ), you.get_part_layer_penalty( torso ) ) ) !=
           std::string::npos );

    const std::vector<std::string> details = you.worn.sort_armor_accessibility_details( you, torso,
            1 );
    REQUIRE( details.size() > 3 );
    CHECK( details[0] == rows[1] + " 2 of 3." );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.find( "Wearing multiple items" ) != std::string::npos;
    } ) );
    CHECK( std::find( details.begin(), details.end(), "Encumbrance and Warmth." ) != details.end() );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.find( "covered by this item" ) != std::string::npos;
    } ) );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }
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

TEST_CASE( "dialogue_response_rows_speak_key_consequence_and_unavailable_reason",
           "[accessibility]" )
{
    map &here = get_map();
    clear_avatar();
    clear_map();
    const on_out_of_scope cleanup( []() {
        clear_map();
        clear_avatar();
    } );
    avatar &you = get_avatar();
    you.setpos( here, tripoint_bub_ms( 15, 15, 0 ) );
    g->faction_manager_ptr->create_if_needed();
    const character_id npc_id = here.place_npc( point_bub_ms( 25, 25 ),
                                string_id<npc_template>( "test_talker" ) );
    g->load_npcs();
    npc *beta = g->find_npc( npc_id );
    REQUIRE( beta != nullptr );

    dialogue d( get_talker_for( you ), get_talker_for( *beta ) );
    d.add_topic( "TALK_TEST_SHOW_ALWAYS2" );
    d.gen_responses( d.topic_stack.back() );
    REQUIRE( d.responses.size() == 2 );
    const talk_data disabled = d.responses[0].create_option_line( d,
                               input_event( 'a', input_event_t::keyboard_char ) );
    CHECK( disabled.spoken == "a, You should see this response but not be able to select it "
           "(except in debug mode) because. Unavailable: zero isn't greater than one." );
    // The displayed row keeps the reason after the text.
    CHECK( remove_color_tags( disabled.text ).find( " -- [zero isn't greater than one.]" ) !=
           std::string::npos );
    CHECK( d.responses[1].create_option_line( d,
            input_event( 'b', input_event_t::keyboard_char ) ).spoken == "b, All done." );

    d.add_topic( "TALK_TEST_EFFECTS" );
    d.gen_responses( d.topic_stack.back() );
    REQUIRE( d.responses.size() == 19 );
    CHECK( d.responses[12].create_option_line( d,
            input_event( 'm', input_event_t::keyboard_char ) ).spoken ==
           "m, This is a multi-effect response. May provoke an attack." );
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

TEST_CASE( "trade_panes_speak_party_rows_marks_balance_and_status", "[accessibility]" )
{
    map &here = get_map();
    clear_avatar();
    clear_map();
    const on_out_of_scope cleanup( []() {
        clear_map();
        clear_avatar();
    } );
    avatar &you = get_avatar();
    you.setpos( here, tripoint_bub_ms( 15, 15, 0 ) );
    g->faction_manager_ptr->create_if_needed();
    you.worn.wear_item( you, item( itype_id( "backpack" ) ), false, false );
    item_location own_hammer = you.i_add( item( itype_id( "hammer" ) ) );
    REQUIRE( own_hammer );
    own_hammer->set_owner( you );

    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "thug" );
    clear_character( trader );
    trader.set_attitude( NPCATT_NULL );
    trader.op_of_u = npc_opinion();
    trader.personality.altruism = 0;
    REQUIRE( !trader.will_exchange_items_freely() );
    REQUIRE( trader.max_credit_extended() == 0 );
    const tripoint_bub_ms trader_pos = trader.pos_bub();
    item &goods = here.add_item( trader_pos, item( itype_id( "hammer" ) ) );
    goods.set_owner( trader );
    const item_location goods_loc( map_cursor( trader_pos ), &goods );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    trade_ui trade( you, trader );
    // Lay the panes out like the other test selectors instead of at half the screen size.
    trade.get_active_pane().resize( point( -1, -1 ), point( -1, -1 ) );
    trade_selector &trader_pane = trade.get_active_pane();
    REQUIRE( trader_pane.highlight_one_of( { goods_loc } ) );
    trader_pane.announce_accessibility_focus();
    REQUIRE( capture_ptr->messages.size() == 1 );
    const std::string opening = capture_ptr->messages.back();
    CAPTURE( opening );
    CHECK( opening.find( "Trade: " + trader.get_name() + ". Balance: credit $0.00, acceptable." ) ==
           0 );
    CHECK( opening.find( "Trader's goods. 1 item." ) != std::string::npos );
    CHECK( opening.find( "hammer" ) != std::string::npos );
    CHECK( opening.find( " each." ) != std::string::npos );
    CHECK( opening.find( "Your goods" ) == std::string::npos );

    // A redraw without input stays silent.
    trader_pane.announce_accessibility_focus();
    CHECK( capture_ptr->messages.size() == 1 );

    trader_pane.toggle_entry( trader_pane.get_active_column().get_highlighted(), 1 );
    trader_pane.announce_accessibility_selection();
    REQUIRE( capture_ptr->messages.size() == 2 );
    const std::string marked = capture_ptr->messages.back();
    CAPTURE( marked );
    CHECK( marked.find( "hammer" ) == 0 );
    CHECK( marked.find( "Marked. Balance: you owe $" ) != std::string::npos );
    CHECK( marked.find( ", trader refuses." ) != std::string::npos );

    const std::string status = trader_pane.accessibility_status();
    CAPTURE( status );
    CHECK( status.find( "Trade: " ) == 0 );
    CHECK( status.find( "trader refuses. Buying: $" ) != std::string::npos );
    CHECK( status.find( "Selling: $0.00." ) != std::string::npos );
    CHECK( status.find( "Trader's goods." ) != std::string::npos );
    CHECK( status.find( "1 entry selected" ) != std::string::npos );
    CHECK( status.find( "1 of 1" ) != std::string::npos );

    trade.pushevent( trade_ui::event::SWITCH );
    trade.process_events();
    trade_selector &your_pane = trade.get_active_pane();
    your_pane.resize( point( -1, -1 ), point( -1, -1 ) );
    REQUIRE( &your_pane != &trader_pane );
    REQUIRE( your_pane.highlight_one_of( { own_hammer } ) );
    your_pane.announce_accessibility_focus();
    REQUIRE( capture_ptr->messages.size() == 3 );
    const std::string switched = capture_ptr->messages.back();
    CAPTURE( switched );
    CHECK( switched.find( "Your goods." ) == 0 );
    CHECK( switched.find( "Trader's goods" ) == std::string::npos );
    CHECK( switched.find( "Balance" ) == std::string::npos );
    CHECK( switched.find( "hammer" ) != std::string::npos );

    your_pane.toggle_entry( your_pane.get_active_column().get_highlighted(), 1 );
    your_pane.announce_accessibility_selection();
    REQUIRE( capture_ptr->messages.size() == 4 );
    CHECK( capture_ptr->messages.back().find( "Marked. Balance: " ) != std::string::npos );
    your_pane.toggle_entry( your_pane.get_active_column().get_highlighted(), 0 );
    your_pane.announce_accessibility_selection();
    CHECK( capture_ptr->messages.back().find( "hammer. Unmarked. Balance: you owe $" ) == 0 );

    // Switching back names the trader's pane again without the opening summary.
    trade.pushevent( trade_ui::event::SWITCH );
    trade.process_events();
    REQUIRE( &trade.get_active_pane() == &trader_pane );
    const size_t before_return = capture_ptr->messages.size();
    trader_pane.announce_accessibility_focus();
    REQUIRE( capture_ptr->messages.size() == before_return + 1 );
    const std::string returned = capture_ptr->messages.back();
    CAPTURE( returned );
    CHECK( returned.find( "Trader's goods. 1 item." ) == 0 );
    CHECK( returned.find( "Marked" ) != std::string::npos );

    CHECK( accessibility::repeat_last_announcement() );
    CHECK( capture_ptr->messages.back() == returned );

    accessibility::reset_backend_after_testing();
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

TEST_CASE( "missions_screen_speaks_categories_rows_tracking_and_details", "[accessibility]" )
{
    using accessibility::mission_category;
    static const mission_type_id test_mission( "TEST_MISSION_GOAL_CONDITION2" );
    avatar &you = get_avatar();
    clear_avatar();
    clear_npcs();
    you.reset_all_missions();
    mission::clear_all();
    const time_point previous_turn = calendar::turn;
    calendar::turn = calendar::turn_zero + 3_days;
    const on_out_of_scope cleanup( [&you, previous_turn]() {
        calendar::turn = previous_turn;
        you.reset_all_missions();
        mission::clear_all();
        clear_avatar();
    } );
    const auto assign = [&you]() {
        mission *miss = mission::reserve_new( test_mission, character_id() );
        if( miss->get_assigned_player_id() == you.getID() ) {
            // See mission_test.cpp: optimized builds can give the avatar this id.
            miss->set_assigned_player_id( character_id( -2 ) );
        }
        miss->assign( you );
        return miss;
    };
    mission *first = assign();
    mission *second = assign();
    const std::string name = first->name();
    first->set_deadline( calendar::turn + 2_days );
    first->set_target( you.pos_abs_omt() + tripoint_rel_omt( 0, -5, 0 ) );

    REQUIRE( mission_accessibility_list( mission_category::active ).size() == 2 );
    REQUIRE( you.get_active_mission() == second );
    const std::string deadline = to_string_approx( 2_days );
    CHECK( mission_accessibility_row( *first, mission_category::active ) ==
           name + ", deadline in " + deadline + "." );
    CHECK( mission_accessibility_row( *second, mission_category::active ) ==
           name + ", current objective." );

    // Setting the current objective moves the tracked state.
    you.set_active_mission( *first );
    CHECK( mission_accessibility_row( *first, mission_category::active ) ==
           name + ", current objective, deadline in " + deadline + "." );
    CHECK( mission_accessibility_row( *second, mission_category::active ) == name + "." );

    const std::vector<std::string> details = mission_accessibility_details( *first,
            mission_category::active, 1, 2 );
    REQUIRE( details.size() >= 4 );
    CHECK( details[0] == name + ", current objective, deadline in " + deadline +
           ". Active. 1 of 2." );
    CHECK( details[1] == "Objective: Checks to see if the player has a specific item." );
    CHECK( std::any_of( details.begin(), details.end(), []( const std::string & line ) {
        return line.rfind( "Deadline: ", 0 ) == 0;
    } ) );
    CHECK( std::any_of( details.begin(), details.end(), [&deadline]( const std::string & line ) {
        return line == "Time remaining: " + deadline + ".";
    } ) );
    CHECK( details.back().rfind( "Target: 5 map tiles north, ", 0 ) == 0 );
    CHECK( mission_accessibility_details( *second, mission_category::active, 2,
                                          2 ).back() == "No known location." );

    // A failed mission moves to the failed tab with the time since it failed.
    second->fail();
    CHECK( mission_accessibility_list( mission_category::active ).size() == 1 );
    REQUIRE( mission_accessibility_list( mission_category::failed ).size() == 1 );
    CHECK( mission_accessibility_row( *second, mission_category::failed ).rfind(
               name + ", failed ", 0 ) == 0 );
    CHECK( mission_accessibility_details( *second, mission_category::failed, 1,
                                          1 )[0].rfind( name + ", failed ", 0 ) == 0 );

    // A completed mission speaks the time since it was completed.
    first->wrap_up();
    calendar::turn += 1_hours;
    CHECK( mission_accessibility_list( mission_category::active ).empty() );
    REQUIRE( mission_accessibility_list( mission_category::completed ).size() == 1 );
    const std::string completed = name + ", completed " + to_string_approx( 1_hours ) + " ago.";
    CHECK( mission_accessibility_row( *first, mission_category::completed ) == completed );
    CHECK( mission_accessibility_details( *first, mission_category::completed, 1,
                                          1 )[0] == completed + " Completed. 1 of 1." );
}

TEST_CASE( "overmap_formatters_speak_offset_markers_places_and_routes",
           "[accessibility][nogame]" )
{
    CHECK( accessibility::format_overmap_offset( 2, -3, 0 ) == "3 north 2 east" );
    CHECK( accessibility::format_overmap_offset( 0, 0, 0 ) == "here" );
    CHECK( accessibility::format_overmap_offset( -5, 0, -1 ) == "5 west, 1 level below" );
    CHECK( accessibility::format_overmap_offset( 0, 4, 2 ) == "4 south, 2 levels above" );
    CHECK( accessibility::format_overmap_level( -1 ) == "Level -1" );

    accessibility::overmap_tile tile;
    tile.terrain = "forest";
    tile.dx = 2;
    tile.dy = -3;
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
    unknown.dy = 1;
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

TEST_CASE( "overmap_cursor_speaks_known_tiles_places_and_routes", "[accessibility]" )
{
    static const mission_type_id test_mission( "TEST_MISSION_GOAL_CONDITION2" );
    static const oter_str_id oter_field( "field" );
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    avatar &you = get_avatar();
    clear_avatar();
    you.reset_all_missions();
    mission::clear_all();
    const tripoint_abs_omt origin = you.pos_abs_omt();
    const tripoint_abs_omt north = origin + tripoint_rel_omt( 0, -1, 0 );
    const tripoint_abs_omt north2 = origin + tripoint_rel_omt( 0, -2, 0 );
    const tripoint_abs_omt hidden = origin + tripoint_rel_omt( 3, 0, 0 );
    const tripoint_abs_omt below = hidden + tripoint_rel_omt( 0, 0, -1 );
    const std::vector<tripoint_abs_omt> changed = { north, north2, hidden };
    std::vector<std::pair<oter_id, om_vision_level>> previous;
    previous.reserve( changed.size() );
    for( const tripoint_abs_omt &pos : changed ) {
        previous.emplace_back( overmap_buffer.ter( pos ), overmap_buffer.seen( pos ) );
    }
    const on_out_of_scope cleanup( [&]() {
        overmap_buffer.delete_note( hidden );
        for( size_t index = 0; index < changed.size(); ++index ) {
            overmap_buffer.ter_set( changed[index], previous[index].first );
            overmap_buffer.set_seen( changed[index], previous[index].second );
        }
        you.omt_path.clear();
        you.reset_all_missions();
        mission::clear_all();
        clear_avatar();
        accessibility::reset_backend_after_testing();
    } );
    overmap_buffer.ter_set( north, oter_field.id() );
    overmap_buffer.ter_set( north2, oter_field.id() );
    overmap_buffer.set_seen( north, om_vision_level::full );
    overmap_buffer.set_seen( north2, om_vision_level::full );
    overmap_buffer.set_seen( hidden, om_vision_level::unseen );
    overmap_buffer.add_note( hidden, "B:Base" );
    overmap_buffer.mark_note_dangerous( hidden, 2, true );

    // Unexplored terrain stays hidden; the player's own note is known.
    const accessibility::overmap_tile unseen = accessibility_gameplay::overmap_tile( you, hidden );
    CHECK( unseen.terrain.empty() );
    CHECK( unseen.note == "Base" );
    CHECK( unseen.danger_radius == 2 );
    CHECK( unseen.dx == 3 );
    const std::string field = accessibility_gameplay::overmap_tile( you, north ).terrain;
    REQUIRE_FALSE( field.empty() );

    accessibility_gameplay::overmap_speech speech;
    speech.open( you, "", origin );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages.back().rfind( "Overmap. ", 0 ) == 0 );
    CHECK( capture_ptr->messages.back().find( ", here." ) != std::string::npos );
    speech.update( you, north );
    CHECK( capture_ptr->messages.back() == field + ", 1 north." );
    speech.update( you, north2 );
    CHECK( capture_ptr->messages.back() == field + "." );
    // An unchanged cursor speaks nothing.
    speech.update( you, north2 );
    CHECK( capture_ptr->messages.size() == 3 );
    speech.update( you, hidden );
    CHECK( capture_ptr->messages.back() ==
           "unexplored, note: Base, dangerous area, radius 2, 3 east." );
    speech.update( you, below );
    CHECK( capture_ptr->messages.back().rfind( "Level " + std::to_string( below.z() ) + ". ",
            0 ) == 0 );
    CHECK( capture_ptr->messages.back().find( "3 east, 1 level below." ) != std::string::npos );
    speech.announce( you, hidden );
    CHECK( capture_ptr->messages.back().rfind( "Level " + std::to_string( hidden.z() ) + ". ",
            0 ) == 0 );
    speech.announce( you, hidden );
    CHECK( capture_ptr->messages.back() ==
           "unexplored, note: Base, dangerous area, radius 2, 3 east." );
    // A position spoken elsewhere, such as a search result, is not repeated.
    const size_t spoken = capture_ptr->messages.size();
    speech.sync( you, north );
    speech.update( you, north );
    CHECK( capture_ptr->messages.size() == spoken );
    CHECK( accessibility_gameplay::overmap_note_row( you, hidden ) ==
           "Base, 3 map tiles east, dangerous area, radius 2, unexplored." );

    // Mission targets and the route are markers on the tile and in the places list.
    mission *miss = mission::reserve_new( test_mission, character_id() );
    if( miss->get_assigned_player_id() == you.getID() ) {
        miss->set_assigned_player_id( character_id( -2 ) );
    }
    miss->assign( you );
    miss->set_target( north );
    CHECK( accessibility_gameplay::overmap_tile( you, north ).missions ==
           std::vector<std::string> { miss->name() } );
    you.omt_path = { north2, north, origin };
    CHECK( accessibility_gameplay::overmap_tile( you, north2 ).route_destination );
    CHECK( accessibility_gameplay::overmap_tile( you, north ).on_route );
    accessibility_gameplay::announce_overmap_route( you, north2, "Enter" );
    CHECK( capture_ptr->messages.back() == "Route to " + field +
           ", 2 map tiles north. 2 map tiles of travel. Press Enter again to travel." );
    CHECK( accessibility_gameplay::overmap_route_state( you ) ==
           "Route: " + field + ", 2 map tiles north." );
    speech.summary( you, north2 );
    CHECK( capture_ptr->messages.back() == "Overmap. Cursor: " + field +
           ", route destination, 2 north. Level " + std::to_string( north2.z() ) +
           ". Route: " + field + ", 2 map tiles north." );

    const std::vector<std::string> details = accessibility_gameplay::overmap_tile_details( you,
            north, "Weather: sunny" );
    REQUIRE( details.size() >= 4 );
    CHECK( details.front() == field + ", mission target: " + miss->name() +
           ", on route, 1 north." );
    CHECK( std::find( details.begin(), details.end(),
                      "Current objective from the cursor: here." ) != details.end() );
    CHECK( std::find( details.begin(), details.end(), "Weather: sunny" ) != details.end() );
    CHECK( details.back() == "Level " + std::to_string( north.z() ) + "." );
    const std::vector<std::string> hidden_details = accessibility_gameplay::overmap_tile_details(
                you, hidden, "" );
    // Unexplored tiles describe no terrain beyond the cursor line.
    CHECK( hidden_details.front() == "unexplored, note: Base, dangerous area, radius 2, 3 east." );
    CHECK( std::none_of( hidden_details.begin(), hidden_details.end(), []( const std::string & line ) {
        return line.find( "make out" ) != std::string::npos;
    } ) );

    const std::vector<std::pair<tripoint_abs_omt, accessibility::known_place>> places =
                accessibility_gameplay::known_overmap_places( you );
    const auto row_at = [&places]( const tripoint_abs_omt & pos ) {
        for( const auto &entry : places ) {
            if( entry.first == pos ) {
                return accessibility::format_known_place_row( entry.second );
            }
        }
        return std::string();
    };
    CHECK( row_at( hidden ) == "unexplored, 3 map tiles east, note: Base, dangerous area, radius 2" );
    CHECK( row_at( north ) == field + ", 1 map tile north, mission target: " + miss->name() +
           ", current objective" );
    CHECK( row_at( north2 ) == field + ", 2 map tiles north, route destination" );
    for( size_t index = 1; index < places.size(); ++index ) {
        CHECK( places[index - 1].second.distance <= places[index].second.distance );
    }
    for( const auto &entry : places ) {
        if( entry.second.city || entry.second.camp ) {
            // Only cities and camps the player has seen are listed.
            CHECK( overmap_buffer.seen_more_than( entry.first, om_vision_level::outlines ) );
        }
    }

    you.omt_path.clear();
    accessibility_gameplay::announce_overmap_route( you, hidden, "Enter" );
    CHECK( capture_ptr->messages.back() == "No route to unexplored." );
    CHECK( accessibility_gameplay::overmap_route_state( you ) == "No route selected." );
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

TEST_CASE( "zones_manager_speaks_focus_edits_order_and_return_focus", "[accessibility]" )
{
    static const zone_type_id zone_food( "LOOT_FOOD" );
    static const zone_type_id zone_unsorted( "LOOT_UNSORTED" );
    static const zone_type_id zone_custom( "LOOT_CUSTOM" );
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    clear_map();
    clear_avatar();
    clear_zones();
    const on_out_of_scope cleanup( []() {
        clear_zones();
        accessibility::reset_backend_after_testing();
    } );

    avatar &you = get_avatar();
    zone_manager &mgr = zone_manager::get_manager();
    const tripoint_abs_ms origin = you.pos_abs();
    const faction_id fac = you.get_faction()->id;
    mgr.add( "Pantry", zone_food, fac, false, true, origin + tripoint_rel_ms( -1, -6, 0 ),
             origin + tripoint_rel_ms( 1, -4, 0 ), nullptr, true );
    mgr.add( "Sorting", zone_unsorted, fac, false, true, origin, origin, nullptr, true );

    faction_id faction = your_fac;
    REQUIRE( faction == fac );
    std::vector<zone_manager::ref_zone_data> zones = mgr.get_zones( faction );
    REQUIRE( zones.size() == 2 );
    REQUIRE( zones[0].get().get_name() == "Pantry" );
    int index = 0;
    bool show_all = false;
    accessibility_gameplay::zones_manager_speech speech( you, zones, index, show_all, faction );
    const auto act = [&]( const std::string & action, const std::function<void()> &change ) {
        speech.before();
        change();
        speech.after( action, false );
    };
    const auto nothing = []() {};

    const std::string pantry = "Pantry, " + mgr.get_name_from_type( zone_food ) +
                               ", enabled, 5 tiles north.";
    const std::string sorting = "Sorting, " + mgr.get_name_from_type( zone_unsorted ) +
                                ", enabled, here.";
    CHECK( accessibility::format_zone_row( accessibility_gameplay::zone_row( you,
                                           zones[0].get() ) ) == pantry );
    speech.open();
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages.back() == "Zones manager, 2 zones. " + pantry );

    // Focus moves speak the row; a wrap says where focus went.
    act( "DOWN", [&]() {
        index = 1;
    } );
    CHECK( capture_ptr->messages.back() == sorting );
    act( "DOWN", [&]() {
        index = 0;
    } );
    CHECK( capture_ptr->messages.back() == "Top. " + pantry );
    act( "UP", [&]() {
        index = 1;
    } );
    CHECK( capture_ptr->messages.back() == "Bottom. " + sorting );

    // Blink timeouts and closing are silent; a canceled close returns focus.
    const size_t spoken = capture_ptr->messages.size();
    act( "TIMEOUT", nothing );
    speech.before();
    speech.after( "QUIT", true );
    CHECK( capture_ptr->messages.size() == spoken );
    act( "QUIT", nothing );
    CHECK( capture_ptr->messages.back() == sorting );
    act( "HELP_KEYBINDINGS", nothing );
    CHECK( capture_ptr->messages.size() == spoken + 2 );

    // State changes speak the new state.
    act( "DISABLE_ZONE", [&]() {
        zones[1].get().set_enabled( false );
    } );
    CHECK( capture_ptr->messages.back() == "Sorting disabled." );
    act( "ENABLE_ZONE", [&]() {
        zones[1].get().set_enabled( true );
    } );
    CHECK( capture_ptr->messages.back() == "Sorting enabled." );
    act( "TOGGLE_ZONE_DISPLAY", [&]() {
        zones[1].get().toggle_display();
    } );
    CHECK( capture_ptr->messages.back() == "Sorting shown on map." );
    act( "TOGGLE_ZONE_DISPLAY", [&]() {
        zones[1].get().toggle_display();
    } );
    CHECK( capture_ptr->messages.back() == "Sorting hidden from map." );
    act( "ENABLE_PERSONAL_ZONES", nothing );
    CHECK( capture_ptr->messages.back() == "No personal zones changed." );

    // Reordering speaks the new position.
    act( "MOVE_ZONE_UP", [&]() {
        mgr.swap( zones[1], zones[0] );
        zones = mgr.get_zones( faction );
        index = 0;
    } );
    CHECK( capture_ptr->messages.back() == "Sorting, 1 of 2." );
    REQUIRE( zones[1].get().get_name() == "Pantry" );

    // A new zone speaks its size and row; canceled creation returns focus.
    act( "ADD_ZONE", [&]() {
        shared_ptr_fast<zone_options> options = zone_options::create( zone_custom );
        dynamic_cast<loot_options &>( *options ).set_mark( "apple" );
        mgr.add( "Apples", zone_custom, fac, false, true, origin + tripoint_rel_ms( 2, -1, 0 ),
                 origin + tripoint_rel_ms( 6, 1, 0 ), options, true );
        zones = mgr.get_zones( faction );
        index = 2;
    } );
    REQUIRE( zones.size() == 3 );
    const std::string apples = "Apples, " + mgr.get_name_from_type( zone_custom ) +
                               ", enabled, 4 tiles east.";
    CHECK( capture_ptr->messages.back() == "Zone 5 by 3. " + apples );
    act( "ADD_ZONE", nothing );
    CHECK( capture_ptr->messages.back() == apples );

    // Edits speak the changed options, size, or position.
    act( "CONFIRM", [&]() {
        dynamic_cast<loot_options &>( zones[2].get().get_options() ).set_mark( "pear" );
    } );
    CHECK( capture_ptr->messages.back() == apples + " Options: Filter: pear." );
    act( "CONFIRM", [&]() {
        zones[2].get().set_position( { origin + tripoint_rel_ms( 3, 0, 0 ), origin + tripoint_rel_ms( 5, 0, 0 ) } );
    } );
    CHECK( capture_ptr->messages.back() == "Zone 3 by 1. " + apples );
    act( "CONFIRM", [&]() {
        zones[2].get().set_position( { origin + tripoint_rel_ms( 3, 1, 0 ), origin + tripoint_rel_ms( 5, 1, 0 ) } );
    } );
    CHECK( capture_ptr->messages.back().rfind( "Zone moved. Apples, ", 0 ) == 0 );
    act( "CONFIRM", nothing );
    CHECK( capture_ptr->messages.back().rfind( "Apples, ", 0 ) == 0 );

    // Details and the summary are available on request.
    act( "ACCESSIBILITY_DETAILS", nothing );
    CHECK( capture_ptr->messages.back().find( " 3 of 3. Zone 3 by 1. Not shown on map. "
            "Options: Filter: pear." ) != std::string::npos );
    act( "ACCESSIBILITY_STATUS", nothing );
    CHECK( capture_ptr->messages.back().rfind( "Zones manager. Hiding distant zones. 3 of 3. Apples, ",
            0 ) == 0 );
    const std::string summary = capture_ptr->messages.back();
    act( "ACCESSIBILITY_REPEAT_LAST", nothing );
    CHECK( capture_ptr->messages.back() == summary );

    // Changing the filter speaks the filter, count, and row.
    act( "SHOW_ALL_ZONES", [&]() {
        show_all = true;
        index = 0;
    } );
    CHECK( capture_ptr->messages.back() == "Showing all zones, 3 zones. " + sorting );

    // Removal names the removed zone and the new focus.
    act( "REMOVE_ZONE", [&]() {
        mgr.remove( zones[0] );
        zones = mgr.get_zones( faction );
        index = 0;
    } );
    CHECK( capture_ptr->messages.back() == "Removed Sorting. " + pantry );
    act( "REMOVE_ZONE", [&]() {
        mgr.remove( zones[1] );
        mgr.remove( zones[0] );
        zones = mgr.get_zones( faction );
        index = 0;
    } );
    CHECK( capture_ptr->messages.back() == "Removed Pantry. No zones." );
    act( "ACCESSIBILITY_STATUS", nothing );
    CHECK( capture_ptr->messages.back() == "Zones manager. Showing all zones. No zones." );
    act( "DOWN", nothing );
    CHECK( capture_ptr->messages.back() == "Zones manager, no zones." );
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

TEST_CASE( "driving_speaks_entry_controls_obstacles_and_status", "[accessibility]" )
{
    static const vproto_id vehicle_prototype_car( "car" );
    static const ter_str_id ter_t_wall( "t_wall" );
    avatar &you = get_avatar();
    map &here = get_map();
    clear_avatar();
    clear_map();
    clear_vehicles();
    const time_point previous_time = calendar::turn;
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    const on_out_of_scope cleanup( [&]() {
        you.controlling_vehicle = false;
        if( you.in_vehicle ) {
            here.unboard_vehicle( you.pos_bub() );
        }
        accessibility::reset_game_state();
        accessibility::reset_backend_after_testing();
        clear_vehicles();
        clear_map();
        clear_avatar();
        set_time( previous_time );
    } );
    accessibility::reset_game_state();
    accessibility::enable_game_messages( true );
    set_time_to_day();

    const tripoint_bub_ms origin( 60, 60, 0 );
    vehicle *veh_ptr = here.add_vehicle( vehicle_prototype_car, origin, 0_degrees, 100, 0 );
    REQUIRE( veh_ptr != nullptr );
    vehicle &veh = *veh_ptr;
    std::optional<tripoint_bub_ms> controls;
    for( const vpart_reference &vp : veh.get_avail_parts( "CONTROLS" ) ) {
        controls = vp.pos_bub( here );
        break;
    }
    REQUIRE( controls.has_value() );
    int front = INT_MIN;
    int back = INT_MAX;
    for( const tripoint_abs_ms &point : veh.get_points() ) {
        front = std::max( front, here.get_bub( point ).x() );
        back = std::min( back, here.get_bub( point ).x() );
    }

    // Stepping onto the controls names the vehicle and the controls.
    const tripoint_abs_ms outside = here.get_abs( *controls + tripoint_rel_ms( 0, -5, 0 ) );
    you.setpos( here, *controls );
    accessibility_gameplay::announce_avatar_moved( outside, you, here );
    REQUIRE( accessibility::flush_gameplay_announcements() );
    CHECK( capture_ptr->messages.back().rfind( veh.name + ", ", 0 ) == 0 );
    CHECK( capture_ptr->messages.back().find( "controls" ) != std::string::npos );
    here.board_vehicle( *controls, &you );
    REQUIRE( you.in_vehicle );
    you.controlling_vehicle = true;
    veh.engine_on = true;
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    here.update_visibility_cache( origin.z() );

    const std::string units = get_option<std::string>( "USE_METRIC_SPEEDS" );
    const auto speed = []( int velocity ) {
        return static_cast<int>( convert_velocity( velocity, VU_VEHICLE ) );
    };

    // Alt+S while driving speaks the driving state instead of opening the review.
    REQUIRE( accessibility_gameplay::announce_driving_status( here, you ) );
    const std::string status = capture_ptr->messages.back();
    CHECK( status == accessibility::format_vehicle_status(
               accessibility_gameplay::vehicle_status( here, veh ) ) );
    CHECK( status.rfind( veh.name + ". Speed 0 " + units + ", cruise 0 " + units +
                         ". Heading east, 90 degrees. Engine on.", 0 ) == 0 );
    CHECK( status.find( '%' ) != std::string::npos );
    CHECK( accessibility::last_announcement() == status );

    // Control changes speak the new cruise speed and steering heading.
    {
        const accessibility_gameplay::vehicle_control_speech speech( here, veh );
        veh.cruise_thrust( here, 400 );
        speech.finish( -1 );
    }
    REQUIRE( accessibility::flush_gameplay_announcements() );
    CHECK( capture_ptr->messages.back() == "Cruise " + std::to_string( speed(
                veh.cruise_velocity ) ) + " " + units + "." );
    {
        const accessibility_gameplay::vehicle_control_speech speech( here, veh );
        veh.turn( 15_degrees );
        speech.finish( 0 );
    }
    REQUIRE( accessibility::flush_gameplay_announcements() );
    CHECK( capture_ptr->messages.back() == "Heading east, 105 degrees." );
    {
        // A request past the limit says the cruise speed is at its maximum.
        veh.cruise_velocity = veh.max_velocity( here );
        const accessibility_gameplay::vehicle_control_speech speech( here, veh );
        veh.cruise_thrust( here, 400 );
        speech.finish( -1 );
    }
    REQUIRE( accessibility::flush_gameplay_announcements() );
    CHECK( capture_ptr->messages.back() == "Cruise " + std::to_string( speed(
                veh.max_velocity( here ) ) ) + " " + units + ", above safe speed, maximum." );
    {
        // Nothing changed: nothing is spoken.
        const accessibility_gameplay::vehicle_control_speech speech( here, veh );
        speech.finish( 0 );
    }
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    veh.cruise_velocity = 0;
    veh.turn_dir = veh.face.dir();

    // A visible obstacle on the path is spoken once, and again after it re-enters.
    const tripoint_bub_ms ahead( front + 3, controls->y(), 0 );
    here.ter_set( ahead, ter_t_wall );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    here.update_visibility_cache( origin.z() );
    const std::string wall = here.tername( ahead );
    size_t spoken = capture_ptr->messages.size();
    accessibility_gameplay::before_player_input( here, you );
    REQUIRE( capture_ptr->messages.size() == spoken + 1 );
    CHECK( capture_ptr->messages.back() == wall + " ahead, 3 tiles." );
    CHECK( accessibility::last_announcement() == capture_ptr->messages.back() );
    spoken = capture_ptr->messages.size();
    accessibility_gameplay::before_player_input( here, you );
    CHECK( capture_ptr->messages.size() == spoken );

    // Reversing looks behind the vehicle; the wall ahead re-enters the path later.
    const tripoint_bub_ms behind( back - 2, controls->y(), 0 );
    here.ter_set( behind, ter_t_wall );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    here.update_visibility_cache( origin.z() );
    veh.cruise_velocity = -400;
    CHECK( accessibility_gameplay::vehicle_obstacle_transition( here, you ) ==
           wall + " behind, 2 tiles." );
    veh.cruise_velocity = 0;
    CHECK( accessibility_gameplay::vehicle_obstacle_transition( here, you ) ==
           wall + " ahead, 3 tiles." );

    // Letting go of the controls ends the checks; an unseen obstacle is not spoken.
    you.controlling_vehicle = false;
    CHECK_FALSE( accessibility_gameplay::vehicle_obstacle_transition( here, you ).has_value() );
    CHECK_FALSE( accessibility_gameplay::announce_driving_status( here, you ) );
    player_wear_blindfold();
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    here.update_visibility_cache( origin.z() );
    you.controlling_vehicle = true;
    CHECK_FALSE( accessibility_gameplay::vehicle_obstacle_transition( here, you ).has_value() );
}

TEST_CASE( "vehicle_workshop_formatters_speak_location_condition_and_requirements",
           "[accessibility][nogame]" )
{
    accessibility::vehicle_grid_position position;
    position.min_x = -1;
    position.max_x = 1;
    position.min_y = -1;
    position.max_y = 1;
    position.x = 1;
    position.y = -1;
    CHECK( accessibility::format_vehicle_location( position ) == "front left" );
    CHECK( accessibility::format_vehicle_position_details( position ) ==
           "Row 1 of 3 from the front, column 1 of 3 from the left." );
    position.x = 0;
    position.y = 0;
    CHECK( accessibility::format_vehicle_location( position ) == "center" );
    position.y = 1;
    CHECK( accessibility::format_vehicle_location( position ) == "right" );
    position.x = -1;
    position.y = 0;
    CHECK( accessibility::format_vehicle_location( position ) == "rear" );
    position.x = 3;
    CHECK( accessibility::format_vehicle_location( position ) == "outside, front" );
    CHECK( accessibility::format_vehicle_position_details( position ) ==
           "2 tiles in front of the vehicle, column 2 of 3 from the left." );
    // A vehicle one tile wide has no left or right side.
    position.min_y = 0;
    position.max_y = 0;
    position.x = 1;
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

TEST_CASE( "vehicle_examination_speaks_part_list_tiles_tasks_and_stats", "[accessibility]" )
{
    static const vproto_id vehicle_prototype_car( "car" );
    map &here = get_map();
    clear_avatar();
    clear_map();
    clear_vehicles();
    const time_point previous_time = calendar::turn;
    const on_out_of_scope cleanup( [&]() {
        clear_vehicles();
        clear_map();
        clear_avatar();
        set_time( previous_time );
    } );
    set_time_to_day();

    const tripoint_bub_ms origin( 60, 60, 0 );
    vehicle *veh_ptr = here.add_vehicle( vehicle_prototype_car, origin, 0_degrees, 100, 0 );
    REQUIRE( veh_ptr != nullptr );
    vehicle &veh = *veh_ptr;
    get_avatar().setpos( here, origin + tripoint_rel_ms( 0, -4, 0 ) );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );

    // The part list runs from the front to the rear.
    const std::vector<int> parts = accessibility_gameplay::vehicle_part_list( veh );
    REQUIRE( parts.size() > 2 );
    for( size_t i = 1; i < parts.size(); ++i ) {
        CHECK( veh.part( parts[i - 1] ).mount.x() >= veh.part( parts[i] ).mount.x() );
    }
    const accessibility::vehicle_grid_position front =
        accessibility_gameplay::vehicle_grid_position( veh, veh.part( parts.front() ).mount );
    CHECK( front.x == front.max_x );

    std::vector<int> repairable;
    for( const int index : parts ) {
        if( veh.part( index ).info().is_repairable() && veh.part( index ).damage_percent() == 0 ) {
            repairable.push_back( index );
        }
    }
    REQUIRE( repairable.size() >= 2 );
    const int damaged = repairable[0];
    const int intact = repairable[1];
    vehicle_part &damaged_part = veh.part( damaged );
    veh.set_hp( damaged_part, damaged_part.info().durability / 2, true );
    const std::string damage = "damaged " + std::to_string( static_cast<int>( std::lround(
                                   damaged_part.damage_percent() * 100 ) ) ) + "%";
    REQUIRE( damaged_part.damage_percent() > 0 );
    REQUIRE_FALSE( damaged_part.is_broken() );

    // Rows name the part, its location, and its damage in words.
    const std::string name = damaged_part.name( false );
    const std::string location = accessibility::format_vehicle_location(
                                     accessibility_gameplay::vehicle_grid_position( veh, damaged_part.mount ) );
    const std::string row = accessibility::format_vehicle_part_row(
                                accessibility_gameplay::vehicle_part_row( veh, damaged_part ) );
    CHECK( row == name + ", " + location + ", " + damage );

    // Moving the grid cursor to the part's tile speaks the location and all parts there.
    const std::string tile = veh_interact::accessible_tile_for_testing( here, veh, damaged );
    CHECK( tile == accessibility_gameplay::vehicle_tile_description( veh, damaged_part.mount,
            false ) );
    CHECK( tile.rfind( location + ": ", 0 ) == 0 );
    CHECK( tile.find( name + ", " + damage ) != std::string::npos );
    const std::vector<std::string> details = accessibility_gameplay::vehicle_part_details( veh,
            damaged_part );
    REQUIRE( details.size() >= 2 );
    CHECK( details[0] == row + "." );
    CHECK( details[1].rfind( "Row ", 0 ) == 0 );

    // The task menu offers repair with its unmet requirements and the reason in words.
    const std::vector<std::pair<std::string, std::vector<std::string>>> tasks =
        veh_interact::accessible_tasks_for_testing( here, veh, damaged );
    REQUIRE( tasks.size() >= 4 );
    CHECK( tasks[0].first.rfind( "Repair. Unavailable: ", 0 ) == 0 );
    CHECK( tasks[0].first.find( "missing" ) != std::string::npos );
    CHECK( std::any_of( tasks[0].second.begin(), tasks[0].second.end(), []( const std::string & line ) {
        return line.rfind( "Time required: ", 0 ) == 0;
    } ) );
    CHECK( std::any_of( tasks[0].second.begin(), tasks[0].second.end(), []( const std::string & line ) {
        return line.rfind( "Skills required: ", 0 ) == 0;
    } ) );
    CHECK( tasks[1].first.rfind( "Remove", 0 ) == 0 );
    CHECK( std::any_of( tasks[1].second.begin(), tasks[1].second.end(), [&name]( const std::string &
    line ) {
        return line.find( "Removing the " + name ) != std::string::npos;
    } ) );
    CHECK( tasks[2].first == "Install here." );
    CHECK( tasks.back().first == "Label." );
    CHECK( tasks.back().second.empty() );

    // An intact part cannot be repaired, and the reason says so.
    const std::vector<std::pair<std::string, std::vector<std::string>>> intact_tasks =
        veh_interact::accessible_tasks_for_testing( here, veh, intact );
    REQUIRE_FALSE( intact_tasks.empty() );
    CHECK( intact_tasks[0].first == "Repair. Unavailable: not damaged." );

    // Alt+S speaks the vehicle statistics in words.
    const std::string stats = veh_interact::accessible_stats_for_testing( here, veh );
    CHECK( stats.rfind( veh.name + ". Safe speed ", 0 ) == 0 );
    CHECK( stats.find( "Mass: " ) != std::string::npos );
    CHECK( stats.find( "Status: " ) != std::string::npos );
    CHECK( stats.find( ": " + name + "." ) != std::string::npos );

    // The part list has its own key in the examination screen.
    input_context workshop( "VEH_INTERACT", keyboard_mode::keycode );
    workshop.register_action( "ACCESSIBILITY_PART_LIST" );
    workshop.register_action( "ACCESSIBILITY_STATUS" );
    workshop.register_action( "RELABEL" );
    CHECK( workshop.input_to_action( input_event( { keymod_t::alt }, 'l',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_PART_LIST" );
    CHECK( workshop.input_to_action( input_event( { keymod_t::alt }, 's',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_STATUS" );
    CHECK( workshop.input_to_action( input_event( {}, 'a', input_event_t::keyboard_code ) ) ==
           "RELABEL" );
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

TEST_CASE( "character_creation_rows_speak_selection_availability_and_conflicts",
           "[accessibility]" )
{
    static const string_id<scenario> scenario_evacuee( "evacuee" );
    static const profession_id profession_unemployed( "unemployed" );
    static const profession_id hobby_hates_books( "hates_books" );
    static const profession_id hobby_loves_books( "loves_books" );
    static const trait_id trait_HATES_BOOKS( "HATES_BOOKS" );
    avatar &you = get_avatar();
    clear_avatar();
    const scenario *previous_scenario = get_scenario();
    const profession *previous_profession = you.prof;
    const on_out_of_scope cleanup( [&you, previous_scenario, previous_profession]() {
        set_scenario( previous_scenario );
        you.prof = previous_profession;
        you.hobbies.clear();
        clear_avatar();
    } );
    you.male = true;
    set_scenario( &scenario_evacuee.obj() );

    // The CITY_START scenario is unavailable without cities; the list shows it only by color.
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_scenario_row(
                scenario_evacuee.obj(), you, true, true, 0 ) ) == "Evacuee, 0 points, selected." );
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_scenario_row(
                scenario_evacuee.obj(), you, false, false, 0 ) ) ==
           "Evacuee, selected. Unavailable: not available with this world's city size." );

    you.prof = &profession_unemployed.obj();
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_profession_row(
                profession_unemployed.obj(), you, true, 0 ) ) ==
           profession_unemployed->gender_appropriate_name( true ) + ", 0 points, selected." );

    // A background whose trait conflicts with a chosen background cannot be picked.
    you.hobbies.clear();
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_background_row(
                hobby_loves_books.obj(), you, true, 10 ) ) == "Reading, 1 point." );
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_background_row(
                hobby_loves_books.obj(), you, true, -5 ) ) == "Reading, 1 point. Not enough points." );
    you.hobbies.insert( &hobby_hates_books.obj() );
    you.toggle_trait_deps( trait_HATES_BOOKS );
    REQUIRE( you.has_trait( trait_HATES_BOOKS ) );
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_background_row(
                hobby_loves_books.obj(), you, false, 0 ) ) ==
           "Reading. Unavailable: Bookworm conflicts with background Book Hating." );
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_background_row(
                hobby_hates_books.obj(), you, true, 0 ) ) == "Book Hating, earns 1 point, selected." );

    // The shared accessibility keys reach the character creation lists without replacing sort.
    for( const std::string category : {
             "NEW_CHAR_POINTS", "NEW_CHAR_SCENARIOS", "NEW_CHAR_PROFESSIONS"
         } ) {
        CAPTURE( category );
        input_context ctxt( category, keyboard_mode::keycode );
        ctxt.register_action( "ACCESSIBILITY_DETAILS" );
        ctxt.register_action( "ACCESSIBILITY_STATUS" );
        ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
        ctxt.register_action( "SORT" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'd',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_DETAILS" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 's',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_STATUS" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'r',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_REPEAT_LAST" );
        if( category != "NEW_CHAR_POINTS" ) {
            CHECK( ctxt.input_to_action( input_event( {}, 's', input_event_t::keyboard_code ) ) == "SORT" );
        }
    }
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

TEST_CASE( "character_creation_trait_rows_speak_locks_conflicts_and_prohibitions",
           "[accessibility]" )
{
    static const string_id<scenario> scenario_evacuee( "evacuee" );
    static const string_id<scenario> scenario_medieval( "medieval" );
    static const profession_id profession_doctor( "doctor" );
    static const trait_id trait_FASTLEARNER( "FASTLEARNER" );
    static const trait_id trait_SLOWLEARNER( "SLOWLEARNER" );
    static const trait_id trait_FASTREADER( "FASTREADER" );
    static const trait_id trait_PROF_MED( "PROF_MED" );
    avatar &you = get_avatar();
    clear_avatar();
    const scenario *previous_scenario = get_scenario();
    const profession *previous_profession = you.prof;
    const on_out_of_scope cleanup( [&you, previous_scenario, previous_profession]() {
        set_scenario( previous_scenario );
        you.prof = previous_profession;
        clear_avatar();
    } );
    set_scenario( &scenario_evacuee.obj() );
    you.prof = profession::generic();

    CHECK( accessibility::format_character_trait_row( accessibility_gameplay::character_trait_row(
                trait_FASTLEARNER, you, true ) ) == "Fast Learner, 3 points." );
    you.toggle_trait_deps( trait_FASTLEARNER );
    REQUIRE( you.has_trait( trait_FASTLEARNER ) );
    CHECK( accessibility::format_character_trait_row( accessibility_gameplay::character_trait_row(
                trait_FASTLEARNER, you, true ) ) == "Fast Learner, 3 points, selected." );
    // The list shows a conflicting trait only in gray.
    CHECK( accessibility::format_character_trait_row( accessibility_gameplay::character_trait_row(
                trait_SLOWLEARNER, you, true ) ) ==
           "Slow Learner, earns 3 points, conflicts with Fast Learner." );
    CHECK( accessibility::format_character_trait_row( accessibility_gameplay::character_trait_row(
                trait_SLOWLEARNER, you, false ) ) == "Slow Learner, conflicts with Fast Learner." );

    // Removing a profession trait is refused only after Enter; the row says so first.
    you.prof = &profession_doctor.obj();
    you.toggle_trait_deps( trait_PROF_MED );
    REQUIRE( you.has_trait( trait_PROF_MED ) );
    const accessibility::character_trait_row locked = accessibility_gameplay::character_trait_row(
                trait_PROF_MED, you, true );
    CHECK( locked.selected );
    CHECK( locked.locked_by == "profession" );

    set_scenario( &scenario_medieval.obj() );
    const accessibility::character_trait_row forbidden = accessibility_gameplay::character_trait_row(
                trait_FASTREADER, you, true );
    CHECK_FALSE( forbidden.selected );
    CHECK( forbidden.forbidden_by == "scenario" );

    // The shared accessibility keys reach the remaining tabs without replacing their keys.
    for( const std::string category : {
             "NEW_CHAR_STATS", "NEW_CHAR_TRAITS", "NEW_CHAR_SKILLS", "NEW_CHAR_DESCRIPTION"
         } ) {
        CAPTURE( category );
        input_context ctxt( category, keyboard_mode::keycode );
        ctxt.register_action( "ACCESSIBILITY_DETAILS" );
        ctxt.register_action( "ACCESSIBILITY_STATUS" );
        ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
        ctxt.register_action( "SORT" );
        ctxt.register_action( "RANDOMIZE_CHAR_NAME" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'd',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_DETAILS" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 's',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_STATUS" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'r',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_REPEAT_LAST" );
        if( category == "NEW_CHAR_TRAITS" ) {
            CHECK( ctxt.input_to_action( input_event( {}, 's', input_event_t::keyboard_code ) ) == "SORT" );
        }
        if( category == "NEW_CHAR_DESCRIPTION" ) {
            CHECK( ctxt.input_to_action( input_event( {}, 'n', input_event_t::keyboard_code ) ) ==
                   "RANDOMIZE_CHAR_NAME" );
        }
    }
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

TEST_CASE( "mod_manager_rows_speak_state_dependencies_and_load_order", "[accessibility]" )
{
    mod_ui mods( world_generator->get_mod_manager() );
    const mod_id dda( "dda" );
    const mod_id magiclysm( "magiclysm" );
    const mod_id aftershock( "aftershock" );
    const mod_id exoplanet( "aftershock_exoplanet" );
    const mod_id mind_over_matter( "mindovermatter" );
    const mod_id missing( "accessibility_missing_mod" );
    REQUIRE( dda.is_valid() );
    REQUIRE( magiclysm.is_valid() );
    REQUIRE( aftershock.is_valid() );
    REQUIRE( exoplanet.is_valid() );
    REQUIRE( mind_over_matter.is_valid() );
    const std::string content = magiclysm->category.second.translated();

    // Adding a mod also adds its dependencies; the speech names both.
    std::vector<mod_id> active;
    CHECK( accessibility_gameplay::mod_add_refusal( mods, magiclysm, active ).empty() );
    mods.try_add( magiclysm, active );
    REQUIRE( active == std::vector<mod_id> { dda, magiclysm } );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            magiclysm, active, true ) ) == "Magiclysm, active, " + content + "." );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            magiclysm, active, false ) ) == "Magiclysm, " + content + "." );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            aftershock, active, true ) ) == "Aftershock, obsolete, OBSOLETE MODS." );
    CHECK( accessibility_gameplay::mod_add_refusal( mods, magiclysm, active ) ==
           "already in the load order" );
    CHECK( accessibility_gameplay::mod_add_refusal( mods, missing, active ) == "not installed" );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            missing, active, false ) ) == "accessibility_missing_mod, not installed, MISSING MODS." );
    CHECK( accessibility_gameplay::mod_unavailable_reason( mods, magiclysm ).empty() );

    // Moves are refused with the reason the visual +/- marks only imply.
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 0, active, true ) == "already first" );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 0, active, false ) ==
           "the core mod loads first" );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 1, active, true ) ==
           "Dark Days Ahead loads first" );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 1, active, false ) == "already last" );
    mods.try_add( aftershock, active );
    REQUIRE( active == std::vector<mod_id> { dda, magiclysm, aftershock } );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 2, active, true ).empty() );
    size_t selection = 2;
    mods.try_shift( '-', selection, active );
    CHECK( selection == 1 );
    CHECK( accessibility::format_mod_moved( selection + 1, active.size() ) == "Position 2 of 3." );

    // A mod conflicting with the load order is marked like the visual list.
    const std::vector<mod_id> conflicting { dda, exoplanet };
    CHECK( accessibility_gameplay::world_mod_row( mods, mind_over_matter, conflicting,
            true ).incompatible );
    CHECK_FALSE( accessibility_gameplay::world_mod_row( mods, mind_over_matter, active,
                 true ).incompatible );

    // Removing the core mod also removes its dependents.
    std::vector<mod_id> removal = active;
    mods.try_rem( 0, removal );
    CHECK( removal.empty() );

    // The shared accessibility keys reach the world screens without replacing their keys.
    for( const std::string category : {
             "PICK_WORLD_DIALOG", "WORLDGEN_CONFIRM_DIALOG", "MODMANAGER_DIALOG", "DEFAULT"
         } ) {
        CAPTURE( category );
        input_context ctxt( category, keyboard_mode::keycode );
        ctxt.register_action( "ACCESSIBILITY_DETAILS" );
        ctxt.register_action( "ACCESSIBILITY_STATUS" );
        ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
        ctxt.register_action( "ADVANCED_SETTINGS" );
        ctxt.register_action( "RESET" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'd',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_DETAILS" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 's',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_STATUS" );
        CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'r',
                                     input_event_t::keyboard_code ) ) == "ACCESSIBILITY_REPEAT_LAST" );
        if( category == "WORLDGEN_CONFIRM_DIALOG" ) {
            CHECK( ctxt.input_to_action( input_event( { keymod_t::shift }, 's',
                                         input_event_t::keyboard_code ) ) == "ADVANCED_SETTINGS" );
            CHECK( ctxt.input_to_action( input_event( { keymod_t::shift }, 'r',
                                         input_event_t::keyboard_code ) ) == "RESET" );
        }
    }
    // The accessible world name input keeps the randomize key.
    input_context text_ctxt( "STRING_INPUT", keyboard_mode::keychar );
    text_ctxt.register_action( "TEXT.CONFIRM" );
    text_ctxt.register_action( "PICK_RANDOM_WORLDNAME" );
    text_ctxt.register_action( "ANY_INPUT" );
    CHECK( text_ctxt.input_to_action( input_event( '*', input_event_t::keyboard_char ) ) ==
           "PICK_RANDOM_WORLDNAME" );
}
