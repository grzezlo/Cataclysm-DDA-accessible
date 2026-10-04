#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
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
#include "display.h"
#include "faction.h"
#include "accessibility_test_helpers.h"
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
#include "ui_extended_description.h"
#include "veh_interact.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather.h"
#include "worldfactory.h"

// Expected speech strings mirror the spoken output, which uses single spaces after sentences.
// NOLINTBEGIN(cata-text-style)

static const bionic_id bio_night_vision( "bio_night_vision" );
static const bionic_id bio_power_storage( "bio_power_storage" );

static const bodygraph_id bodygraph_arm_l( "arm_l" );

static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_downed( "downed" );

static const faction_id faction_free_merchants( "free_merchants" );

static const itype_id itype_2x4( "2x4" );
static const itype_id itype_backpack( "backpack" );
static const itype_id itype_e_scrap( "e_scrap" );
static const itype_id itype_hammer( "hammer" );
static const itype_id itype_hoodie( "hoodie" );
static const itype_id itype_longshirt( "longshirt" );
static const itype_id itype_rock( "rock" );
static const itype_id itype_scrap( "scrap" );
static const itype_id itype_sweatshirt( "sweatshirt" );
static const itype_id itype_test_backpack( "test_backpack" );
static const itype_id itype_test_soldering_iron( "test_soldering_iron" );
static const itype_id itype_test_weapon1( "test_weapon1" );

static const matype_id test_style_ma1( "test_style_ma1" );

static const mission_type_id mission_TEST_MISSION_GOAL_CONDITION2( "TEST_MISSION_GOAL_CONDITION2" );

static const mod_id MOD_INFORMATION_accessibility_missing_mod( "accessibility_missing_mod" );
static const mod_id MOD_INFORMATION_aftershock( "aftershock" );
static const mod_id MOD_INFORMATION_aftershock_exoplanet( "aftershock_exoplanet" );
static const mod_id MOD_INFORMATION_dda( "dda" );
static const mod_id MOD_INFORMATION_magiclysm( "magiclysm" );
static const mod_id MOD_INFORMATION_mindovermatter( "mindovermatter" );

static const morale_type morale_food_bad( "morale_food_bad" );
static const morale_type morale_food_good( "morale_food_good" );

static const profession_id profession_doctor( "doctor" );
static const profession_id profession_hates_books( "hates_books" );
static const profession_id profession_loves_books( "loves_books" );
static const profession_id profession_unemployed( "unemployed" );

static const proficiency_id proficiency_prof_knapping( "prof_knapping" );
static const proficiency_id proficiency_prof_leatherworking_basic( "prof_leatherworking_basic" );

static const recipe_id recipe_test_soldering_iron( "test_soldering_iron" );

static const skill_id skill_melee( "melee" );

static const spell_id spell_test_spell_pew( "test_spell_pew" );

static const string_id<npc_template> npc_template_test_talker( "test_talker" );

static const string_id<scenario> scenario_evacuee( "evacuee" );
static const string_id<scenario> scenario_medieval( "medieval" );

static const trait_id trait_BIOLUM1( "BIOLUM1" );
static const trait_id trait_FASTLEARNER( "FASTLEARNER" );
static const trait_id trait_FASTREADER( "FASTREADER" );
static const trait_id trait_GOODHEARING( "GOODHEARING" );
static const trait_id trait_HATES_BOOKS( "HATES_BOOKS" );
static const trait_id trait_PROF_MED( "PROF_MED" );
static const trait_id trait_SLOWLEARNER( "SLOWLEARNER" );

static const vproto_id vehicle_prototype_car( "car" );

static const zone_type_id zone_type_LOOT_CUSTOM( "LOOT_CUSTOM" );
static const zone_type_id zone_type_LOOT_FOOD( "LOOT_FOOD" );
static const zone_type_id zone_type_LOOT_UNSORTED( "LOOT_UNSORTED" );

namespace
{

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

TEST_CASE( "popup_and_extended_description_register_repeat_without_consuming_raw_keys",
           "[accessibility][nogame]" )
{
    const on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    accessibility::set_backend_for_testing( std::make_unique<capture_backend>() );
    for( const bool speaking : {
             true, false
         } ) {
        accessibility::set_screen_reader_mode( speaking ? accessibility::screen_reader_mode::on :
                                               accessibility::screen_reader_mode::off );
        for( const input_event_t type : {
                 input_event_t::keyboard_code, input_event_t::keyboard_char
             } ) {
            const input_event repeat( { keymod_t::alt }, 'r', type );
            query_popup informational;
            informational.context( "POPUP_WAIT" ).allow_cancel( true );
            CHECK( informational.make_input_context().input_to_action( repeat ) ==
                   "ACCESSIBILITY_REPEAT_LAST" );
            CHECK( informational.make_input_context().is_registered_action( "QUIT" ) );

            query_popup choices;
            choices.context( "YESNO" ).option( "YES" ).option( "NO" );
            CHECK( choices.make_input_context().input_to_action( repeat ) ==
                   "ACCESSIBILITY_REPEAT_LAST" );

            query_popup raw;
            raw.context( "POPUP_WAIT" ).allow_anykey( true );
            CHECK( raw.make_input_context().input_to_action( repeat ) == "ERROR" );
            CHECK( raw.make_input_context().is_registered_action( "ANY_INPUT" ) );
            raw.allow_cancel( true );
            CHECK( raw.make_input_context().input_to_action( repeat ) == "ERROR" );

            input_context extended = extended_description_input_context();
            CHECK( extended.input_to_action( repeat ) == "ACCESSIBILITY_REPEAT_LAST" );
            CHECK( extended.is_registered_action( "NEXT_TAB" ) );
            CHECK( extended.is_registered_action( "QUIT" ) );
        }
    }
}

TEST_CASE( "pumped_repeat_honors_bindings_and_interrupts_once_per_request",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    const on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    accessibility::announce( "Loading a long save" );
    const int initial_cancels = speech->cancels;
    const input_event repeat( { keymod_t::alt }, 'r', input_event_t::keyboard_code );
    const std::vector<input_event> bindings = inp_mngr.get_input_for_action(
                "ACCESSIBILITY_REPEAT_LAST" );
    REQUIRE( dispatch_accessibility_repeat( repeat, bindings ) );
    REQUIRE( dispatch_accessibility_repeat( repeat, bindings ) );
    CHECK( speech->cancels == initial_cancels + 2 );
    CHECK( speech->messages == std::vector<std::string> { "Loading a long save",
            "Loading a long save", "Loading a long save"
                                                        } );

    CHECK_FALSE( dispatch_accessibility_repeat( input_event(), bindings ) );
    CHECK_FALSE( dispatch_accessibility_repeat( input_event( 0, input_event_t::timeout ), bindings ) );
    CHECK_FALSE( dispatch_accessibility_repeat(
                     input_event( { keymod_t::ctrl }, 'r', input_event_t::keyboard_code ), bindings ) );
    CHECK( speech->messages.size() == 3 );
    accessibility::announce( "Same input generation" );
    CHECK( speech->cancels == initial_cancels + 2 );

    const input_event remapped( { keymod_t::alt }, 'q', input_event_t::keyboard_char );
    const std::vector<input_event> custom = { remapped };
    CHECK_FALSE( dispatch_accessibility_repeat( repeat, custom ) );
    CHECK( dispatch_accessibility_repeat( remapped, custom ) );
    CHECK( speech->cancels == initial_cancels + 3 );
    CHECK( speech->messages.back() == "Same input generation" );
    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::off );
    CHECK( dispatch_accessibility_repeat( remapped, custom ) );
    CHECK( speech->messages.size() == 5 );
    CHECK( speech->cancels == initial_cancels + 3 );
}

TEST_CASE( "numeric_option_text_requires_a_complete_finite_number", "[accessibility][nogame]" )
{
    auto option = get_options().get_option( "TURN_DURATION" );
    option.setValue( 2.0f );
    for( const std::string text : {
             "1.5garbage", "1,5", "1.5 2", "", " \t", "1e999",
             "nan", "NaN", "inf", "-infinity", "1.5.6"
         } ) {
        INFO( text );
        CHECK( option.setNumericInput( text ) == "Not a number" );
        CHECK( option.value_as<float>() == 2.0f );
    }
    const std::vector<std::pair<std::string, float>> valid = {
        { "1", 1.0f }, { "1.5", 1.5f }, { "+2.5", 2.5f }, { "-0", 0.0f },
        { "1e-1", 0.1f }, { "  1.5 \t", 1.5f }, { ".5", 0.5f }
    };
    for( const auto &entry : valid ) {
        INFO( entry.first );
        CHECK( option.setNumericInput( entry.first ).empty() );
        CHECK( option.value_as<float>() == Approx( entry.second ) );
    }
    // Valid out-of-range numbers retain the existing setter's default fallback.
    CHECK( option.setNumericInput( "-1.5" ).empty() );
    CHECK( option.value_as<float>() == 0.0f );
    CHECK( option.setNumericInput( "100" ).empty() );
    CHECK( option.value_as<float>() == 0.0f );

    auto integer = get_options().get_option( "MUSIC_VOLUME" );
    integer.setValue( 25 );
    CHECK( integer.setNumericInput( "12garbage" ) == "Not a number" );
    CHECK( integer.value_as<int>() == 25 );
    CHECK( integer.setNumericInput( "12" ).empty() );
    CHECK( integer.value_as<int>() == 12 );
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

    for( const char *accessibility_action : {
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
           "Help.  23 topics.  Use Up and Down to browse, hotkey or Enter to open, Escape to exit.  a: Introduction" );
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

TEST_CASE( "comparison_selector_announces_first_mark_and_focus_revisit", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    item &first_item = here.add_item_or_charges( position, item( itype_hammer ) );
    item &second_item = here.add_item_or_charges( position, item( itype_2x4 ) );
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
    const item_location held = you.i_add( item( itype_hammer ) );
    REQUIRE( held );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    item &ground_item = here.add_item_or_charges( position, item( itype_rock ) );
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
    you.worn.wear_item( you, item( itype_backpack ), false, false );
    const item_location hammer = you.i_add( item( itype_hammer ) );
    const item_location rock = you.i_add( item( itype_rock ) );
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

TEST_CASE( "inventory_selector_reopened_after_item_action_speaks_only_focus",
           "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const item_location held = you.i_add( item( itype_hammer ) );
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

TEST_CASE( "multiselect_speaks_selection_state_totals_and_status", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_backpack ), false, false );
    item_location rock;
    for( int i = 0; i < 3; ++i ) {
        rock = you.i_add( item( itype_rock ) );
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
    you.worn.wear_item( you, item( itype_backpack ), false, false );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    here.add_item_or_charges( position, item( itype_hammer ) );
    here.add_item_or_charges( position, item( itype_rock ) );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    // A transfer from an earlier test waits for the next advanced inventory
    // announcement; consume it so this test starts without a result.
    {
        advanced_inventory stale;
        stale.init();
        stale.announce_accessibility();
    }
    capture_ptr->messages.clear();

    advanced_inventory advinv;
    advinv.init();
    set_transfer_panes( advinv, "ITEMS_CE", AIM_CENTER, "ITEMS_INVENTORY", AIM_INVENTORY );
    const advanced_inventory_pane &ground = advinv.get_pane( advinv.get_src() );
    REQUIRE( ground.items.size() == 2 );

    advinv.announce_accessibility();
    REQUIRE( !capture_ptr->messages.empty() );
    const std::string opening = capture_ptr->messages.back();
    INFO( opening );
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

TEST_CASE( "advanced_inventory_restores_parent_focus_once_after_child_return",
           "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_backpack ), false, false );
    you.i_add( item( itype_rock ) );
    get_map().add_item_or_charges( you.pos_bub(), item( itype_hammer ) );
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    const on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
        clear_avatar();
        clear_map();
    } );

    advanced_inventory advinv;
    advinv.init();
    // Consume any pending transfer result from a previous test.
    advinv.announce_accessibility();
    for( const bool carried : {
             false, true
         } ) {
        set_transfer_panes( advinv, carried ? "ITEMS_INVENTORY" : "ITEMS_CE",
                            carried ? AIM_INVENTORY : AIM_CENTER,
                            carried ? "ITEMS_CE" : "ITEMS_INVENTORY",
                            carried ? AIM_CENTER : AIM_INVENTORY );
        advinv.get_pane( advinv.get_src() ).scroll_to_start();
        advinv.announce_accessibility();
        speech->messages.clear();
        const advanced_inventory_pane &pane = advinv.get_pane( advinv.get_src() );
        REQUIRE( !pane.items.empty() );
        REQUIRE( pane.index >= 0 );
        REQUIRE( pane.index < static_cast<int>( pane.items.size() ) );
        REQUIRE( !pane.items[pane.index].items.empty() );
        const std::string expected_name = pane.items[pane.index].items.front()->display_name();
        accessibility::announce( "Later fragment in a child item document" );
        advinv.resume_accessibility_focus();
        advinv.resume_accessibility_focus();
        advinv.announce_accessibility();
        REQUIRE( speech->messages.size() == 2 );
        const std::string row = speech->messages.back();
        CHECK( row.find( "Advanced inventory" ) == std::string::npos );
        CHECK( row.find( expected_name ) != std::string::npos );
        advinv.announce_accessibility();
        CHECK( speech->messages.size() == 2 );
        CHECK( accessibility::repeat_last_announcement() );
        CHECK( speech->messages.back() == row );

        accessibility::announce( "Keybinding help" );
        advinv.process_action( "HELP_KEYBINDINGS" );
        const size_t before_return = speech->messages.size();
        advinv.announce_accessibility();
        CHECK( speech->messages.size() == before_return + 1 );
        CHECK( speech->messages.back() == row );

        for( const std::string action : {
                 "ACCESSIBILITY_DETAILS", "ACCESSIBILITY_STATUS"
             } ) {
            advinv.process_action( action );
            const std::string requested = accessibility::last_announcement();
            const size_t count = speech->messages.size();
            advinv.announce_accessibility();
            CHECK( speech->messages.size() == count );
            CHECK( accessibility::last_announcement() == requested );
        }
        // A real row-state change and return are combined into one announcement.
        accessibility::announce( "Child document" );
        advinv.resume_accessibility_focus();
        advinv.process_action( "TOGGLE_FAVORITE" );
        advinv.recalc_pane( advinv.get_src() );
        const size_t before_change = speech->messages.size();
        advinv.announce_accessibility();
        CHECK( speech->messages.size() == before_change + 1 );
        advinv.announce_accessibility();
        CHECK( speech->messages.size() == before_change + 1 );
    }
}

TEST_CASE( "comparison_detail_reentry_keeps_item_paragraph_and_repeat_aligned",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    const on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    const std::vector<std::string> first = { "First A", "First B" };
    const std::vector<std::string> second = { "Second A", "Second B", "Second C" };
    for( const bool toggle_out : {
             false, true
         } ) {
        accessibility::inventory_comparison_cursor cursor;
        cursor.toggle_details();
        CHECK( cursor.move( first.size(), 1 ) );
        cursor.select_item( 1 );
        CHECK( cursor.move( second.size(), 1 ) );
        if( toggle_out ) {
            cursor.toggle_details();
        } else {
            cursor.leave_details();
        }
        CHECK_FALSE( cursor.in_details() );
        CHECK( cursor.move( 4, 2 ) );
        cursor.toggle_details();
        CHECK( cursor.selected_item() == 1 );
        CHECK( cursor.position() == 1 );
        const auto focus = accessibility::inventory_comparison_detail_focus(
                               cursor, "First item", "Second item", first, second );
        CHECK( focus.first == "Second item" );
        CHECK( focus.second == "Second B" );
        accessibility::announce( accessibility::format_document_focus( focus.first, focus.second ) );
        CHECK( accessibility::repeat_last_announcement() );
        CHECK( speech->messages.back() == "Second item. Second B" );
        CHECK( cursor.move( second.size(), 1 ) );
        CHECK( accessibility::inventory_comparison_detail_focus(
                   cursor, "First item", "Second item", first, second ).second == "Second C" );
        cursor.select_item( 0 );
        CHECK( cursor.position() == 1 );
        CHECK_FALSE( cursor.move( first.size(), 1 ) );
        CHECK( accessibility::inventory_comparison_detail_focus(
                   cursor, "First item", "Second item", first, second ).second == "First B" );
        cursor.leave_details();
        CHECK( cursor.position() == 2 );
    }
    accessibility::inventory_comparison_cursor empty;
    empty.toggle_details();
    for( const int item : {
             0, 1
         } ) {
        empty.select_item( item );
        CHECK_FALSE( empty.move( 0, 1 ) );
        const auto focus = accessibility::inventory_comparison_detail_focus(
                               empty, "First item", "Second item", {}, {} );
        CHECK( focus.first == ( item == 0 ? "First item" : "Second item" ) );
        CHECK( focus.second == "No details available" );
    }
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

TEST_CASE( "pickup_selector_rows_speak_ownership_and_destination", "[accessibility]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.worn.wear_item( you, item( itype_backpack ), false, false );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    item &own_rock = here.add_item_or_charges( position, item( itype_rock ) );
    own_rock.set_owner( you );
    item &merchant_hammer = here.add_item_or_charges( position, item( itype_hammer ) );
    merchant_hammer.set_owner( faction_free_merchants );
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
    you.worn.wear_item( you, item( itype_backpack ), false, false );
    const item_location rock = you.i_add( item( itype_rock ) );
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
    const recipe &soldering_iron = recipe_test_soldering_iron.obj();

    you.invalidate_crafting_inventory();
    std::vector<accessibility::requirement_group> groups = accessibility_gameplay::requirement_groups(
                soldering_iron.simple_requirements(), you.crafting_inventory(),
                soldering_iron.get_component_filter(), 1, true );
    REQUIRE( groups.size() == 7 );
    CHECK( accessibility::count_missing_requirement_groups( groups ) == 7 );
    CHECK( accessibility_gameplay::missing_requirement_groups( soldering_iron.simple_requirements(),
            you.crafting_inventory(), soldering_iron.get_component_filter(), 1 ) == 7 );
    std::vector<std::string> lines;
    lines.reserve( groups.size() );
    for( const accessibility::requirement_group &group : groups ) {
        lines.push_back( accessibility::format_requirement_group( group ) );
    }
    CHECK( lines[0] == "Tools: 1 tool with cutting of 2 or more, missing." );
    CHECK( lines[1] == "Tools: 1 tool with screw driving of 1 or more, missing." );
    CHECK( lines[4] == "Components: 1 scrap metal, missing." );
    // Without the knowledge of the primary skill, the recipe lacks one more requirement.
    CHECK( recipe_accessibility_row( soldering_iron, you ) == "TEST soldering iron, 7 missing." );

    here.add_item( you.pos_bub( here ), item( itype_scrap, calendar::turn, 1 ) );
    here.add_item( you.pos_bub( here ), item( itype_e_scrap ) );
    here.add_item( you.pos_bub( here ), item( itype_e_scrap ) );
    you.invalidate_crafting_inventory();
    groups = accessibility_gameplay::requirement_groups( soldering_iron.simple_requirements(),
             you.crafting_inventory(), soldering_iron.get_component_filter(), 1, true );
    REQUIRE( groups.size() == 7 );
    CHECK( accessibility::count_missing_requirement_groups( groups ) == 5 );
    CHECK( accessibility_gameplay::missing_requirement_groups( soldering_iron.simple_requirements(),
            you.crafting_inventory(), soldering_iron.get_component_filter(), 1 ) == 5 );
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
    CHECK( accessibility_gameplay::missing_requirement_groups( soldering_iron.simple_requirements(),
            you.crafting_inventory(), soldering_iron.get_component_filter(), 2 ) ==
           accessibility::count_missing_requirement_groups( groups ) );
    CHECK( recipe_accessibility_row( soldering_iron, you, 2 ).rfind( "Batch size 2, ", 0 ) == 0 );

    const std::vector<std::string> details = recipe_accessibility_details( soldering_iron, you );
    REQUIRE( !details.empty() );
    CHECK( details.front() == "5 missing." );
    const auto has_line = [&details]( const std::string & line ) {
        return std::find( details.begin(), details.end(), line ) != details.end();
    };
    CHECK( has_line( "Tools: 1 tool with cutting of 2 or more, missing." ) );
    CHECK( has_line( "Components: 1 scrap metal (have 1), have." ) );
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.rfind( "Primary skill: ", 0 ) == 0;
    } ) );
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.rfind( "Time to complete: ", 0 ) == 0;
    } ) );

    // Disassembly lists what it returns and the tools it needs.
    const recipe &uncraft = recipe_dictionary::get_uncraft( itype_test_soldering_iron );
    REQUIRE( !uncraft.is_null() );
    const std::vector<std::string> disassembly = accessibility_gameplay::disassembly_details(
                uncraft, "2 electronic scrap", "20 minutes", you.crafting_inventory() );
    REQUIRE( disassembly.size() >= 3 );
    CHECK( disassembly[0] == "Returns: 2 electronic scrap." );
    CHECK( disassembly[1] == "Time: 20 minutes." );
    CHECK( disassembly[2] == "Tools: 1 tool with cutting of 2 or more, missing." );
    CHECK( std::none_of( disassembly.begin(), disassembly.end(), []( std::string_view line ) {
        return line.rfind( "Components:", 0 ) == 0;
    } ) );
}

TEST_CASE( "encumbrance_warmth_words_follow_the_warmth_colors", "[accessibility]" )
{
    clear_avatar();
    avatar &you = get_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    const std::vector<std::pair<units::temperature, std::string>> bands = {
        { BODYTEMP_SCORCHING + 1_C_delta, "scorching" }, { BODYTEMP_SCORCHING, "very hot" },
        { BODYTEMP_VERY_HOT, "warm" }, { BODYTEMP_HOT, "comfortable" },
        { BODYTEMP_COLD, "chilly" }, { BODYTEMP_VERY_COLD, "very cold" },
        { BODYTEMP_FREEZING, "freezing" }
    };
    const std::vector<nc_color> colors = {
        c_red, c_light_red, c_yellow, c_green, c_light_blue, c_cyan, c_blue
    };
    for( size_t band = 0; band < bands.size(); ++band ) {
        CAPTURE( bands[band].second );
        you.set_part_temp_conv( body_part_torso, bands[band].first );
        CHECK( display::bodytemp_band( you, body_part_torso ) == static_cast<int>( band ) );
        CHECK( display::bodytemp_color( you, body_part_torso ) == colors[band] );
        // The detailed rows name the band after the warmth number.
        const std::vector<std::string> rows = encumbrance_accessibility_rows( you, nullptr );
        CHECK( std::any_of( rows.begin(), rows.end(), [&]( const std::string & row ) {
            return row.rfind( "torso, ", 0 ) == 0 &&
                   ( row.find( ", " + bands[band].second + "." ) != std::string::npos ||
                     row.find( ", " + bands[band].second + "," ) != std::string::npos );
        } ) );
    }
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
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
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
    CHECK( std::any_of( rows.begin(), rows.end(), []( std::string_view row ) {
        return row.rfind( "arms, encumbrance 0", 0 ) == 0;
    } ) );
    details = character_sheet_accessibility_details( you, encumbrance,
              static_cast<unsigned>( torso - rows.begin() ) );
    REQUIRE( !details.empty() );
    CHECK( details[0].rfind( "torso, encumbrance 0, layering 0, total 0, warmth ", 0 ) == 0 );
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.find( " score: " ) != std::string::npos;
    } ) );

    // Skill rows name their category when focus enters it; details read the skill.
    you.set_skill_level( skill_melee, 2 );
    you.set_knowledge_level( skill_melee, 3 );
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
        if( headers[line].skill->ident() == skill_melee ) {
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
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.rfind( "Practical level: 2", 0 ) == 0;
    } ) );

    // Traits, proficiencies, and effects by name and state; empty tabs say so.
    CHECK( character_sheet_accessibility_tab( you, traits ) == "Traits. No items." );
    you.set_mutation( trait_GOODHEARING );
    rows = character_sheet_accessibility_rows( you, traits );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == string_format( "%s, positive.", trait_GOODHEARING->name() ) );
    details = character_sheet_accessibility_details( you, traits, 0 );
    REQUIRE( details.size() >= 2 );
    CHECK( details[0] == rows[0] + " 1 of 1." );

    you.add_proficiency( proficiency_prof_knapping, true );
    rows = character_sheet_accessibility_rows( you, proficiencies );
    CHECK( std::find( rows.begin(), rows.end(), "Basic Knapping, known." ) != rows.end() );

    const std::string bionics_tab = character_sheet_accessibility_tab( you, bionics );
    CHECK( bionics_tab.rfind( "Bionics. Power ", 0 ) == 0 );
    CHECK( bionics_tab.find( "No items." ) != std::string::npos );

    you.add_effect( effect_downed, 1_minutes );
    rows = character_sheet_accessibility_rows( you, effects );
    CHECK( std::find( rows.begin(), rows.end(), "Downed." ) != rows.end() );
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

    you.add_bionic( bio_power_storage );
    you.add_bionic( bio_night_vision );
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
    std::optional<bionic *> bio = you.find_bionic_by_type( bio_night_vision );
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

    you.set_mutation( trait_BIOLUM1 );
    you.set_mutation( trait_GOODHEARING );
    you.set_stored_kcal( you.get_healthy_kcal() );

    std::vector<std::string> rows = mutations_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    const std::string photophore_row = string_format( "%s, off, 9 kcal, cooldown ",
                                       you.mutation_name( trait_BIOLUM1 ) );
    CHECK( rows[0].rfind( photophore_row, 0 ) == 0 );
    CHECK( rows[0].find( "Unavailable" ) == std::string::npos );
    CHECK( mutations_accessibility_tab( you, true ) == "Active mutations. 1 item. " + rows[0] );
    rows = mutations_accessibility_rows( you, false );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == string_format( "%s.", you.mutation_name( trait_GOODHEARING ) ) );

    // A starting trait and a hidden sprite are spoken, not shown only by color.
    you.my_traits.insert( trait_GOODHEARING );
    you.cached_mutations[trait_GOODHEARING].show_sprite = false;
    rows = mutations_accessibility_rows( you, false );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0] == string_format( "%s, starting trait, sprite hidden.",
                                     you.mutation_name( trait_GOODHEARING ) ) );

    // Too hungry to activate: the row names the missing resource.
    you.set_stored_kcal( you.get_healthy_kcal() / 2 );
    rows = mutations_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0].rfind( photophore_row, 0 ) == 0 );
    CHECK( rows[0].find( ". Unavailable: not enough kcal." ) != std::string::npos );

    you.cached_mutations[trait_BIOLUM1].powered = true;
    rows = mutations_accessibility_rows( you, true );
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0].rfind( string_format( "%s, on, 9 kcal, cooldown ",
                                         you.mutation_name( trait_BIOLUM1 ) ), 0 ) == 0 );
    CHECK( rows[0].find( "Unavailable" ) == std::string::npos );

    CHECK( mutations_accessibility_summary( you, true, 0, false ).rfind(
               "Activating. Active mutations. 1 of 1. ", 0 ) == 0 );
    CHECK( mutations_accessibility_summary( you, false, 0, true ) ==
           "Examining. Passive mutations. 1 of 1. No key." );

    const std::vector<std::string> details = mutations_accessibility_details( you, trait_BIOLUM1 );
    REQUIRE( details.size() >= 2 );
    CHECK( details[0].rfind( rows[0] + " Active mutations. 1 of 1. ", 0 ) == 0 );
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.find( "A photophore has grown from your head" ) != std::string::npos;
    } ) );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }
}

TEST_CASE( "spell_menu_speaks_rows_unavailable_reason_and_details", "[accessibility]" )
{
    clear_avatar();
    const on_out_of_scope cleanup( []() {
        clear_avatar();
    } );
    avatar &you = get_avatar();
    you.magic->learn_spell( spell_test_spell_pew, you, true );
    spell &sp = you.magic->get_spell( spell_test_spell_pew );

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
    const auto has_line = [&details]( const std::string_view prefix ) {
        return std::any_of( details.begin(), details.end(), [&prefix]( std::string_view line ) {
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
    item hammer( itype_hammer );
    REQUIRE( you.wield( hammer ) );
    you.wear_item( item( itype_test_backpack ), false, false );
    // Carried weapons are shown only by color; the document names them.
    you.i_add( item( itype_test_weapon1 ) );
    REQUIRE( you.get_wielded_item() );
    REQUIRE( you.get_wielded_item()->typeId() == itype_hammer );

    const std::vector<std::string> lines =
        martial_art_accessibility_details( test_style_ma1 );
    const auto has_line = [&lines]( const std::string_view text ) {
        return std::any_of( lines.begin(), lines.end(), [&text]( std::string_view line ) {
            return line.find( text ) != std::string_view::npos;
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

TEST_CASE( "morale_window_reads_total_sources_and_focus_as_a_document", "[accessibility]" )
{
    player_morale morale;
    std::vector<std::string> document = morale.accessibility_document( 10, 0, 0 );
    REQUIRE( document.size() == 3 );
    CHECK( document[0] == "Total morale: 0." );
    CHECK( document[1] == "Nothing affects your morale." );
    CHECK( document[2] == "Focus trends towards: 10." );

    morale.add( morale_food_good, 20 );
    morale.add( morale_food_bad, -10 );
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
    const auto torso_row = std::find_if( rows.begin(), rows.end(), [&]( std::string_view row ) {
        return row.rfind( torso_name + ",", 0 ) == 0;
    } );
    REQUIRE( torso_row != rows.end() );
    CHECK( *torso_row == torso_name + ", " + health + ", has details." );
    CHECK( std::count_if( rows.begin(), rows.end(), []( std::string_view row ) {
        return row.find( "health 10 of 10, has details." ) != std::string::npos;
    } ) > 0 );
    CHECK( bodygraph_accessibility_open( you ).rfind( string_format( "Body status. %d items. ",
            static_cast<int>( rows.size() ) ), 0 ) == 0 );

    // A nested graph is named after its body part.
    const bodypart_id arm_l( "arm_l" );
    const std::string nested = bodygraph_accessibility_open( you, bodygraph_arm_l );
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

    you.add_effect( effect_bleed, 10_minutes, torso );
    std::vector<std::string> rows = medical_accessibility_rows( you, 0 );
    const std::string torso_name = uppercase_first_letter( body_part_name( torso, 1 ) );
    const auto torso_row = std::find_if( rows.begin(), rows.end(), [&]( std::string_view row ) {
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
    you.add_proficiency( proficiency_prof_knapping, true );
    you.set_proficiency_practice( proficiency_prof_leatherworking_basic,
                                  proficiency_prof_leatherworking_basic->time_to_learn() / 2 );

    // clear_avatar() keeps proficiencies practiced by earlier tests in the
    // learning list at zero progress, so locate the rows this test adds.
    const std::vector<std::string> rows = proficiencies_window_accessibility_rows( you, 0 );
    const std::string leather_row = string_format( "%s, learning, %d%%.",
                                    proficiency_prof_leatherworking_basic->name(),
                                    static_cast<int>( std::floor( you.get_proficiency_practice( proficiency_prof_leatherworking_basic )
                                            * 100 ) ) );
    const auto leather_it = std::find( rows.begin(), rows.end(), leather_row );
    REQUIRE( leather_it != rows.end() );
    const int leather_index = static_cast<int>( leather_it - rows.begin() );
    // Known proficiencies are listed first.
    CHECK( rows[0] == proficiency_prof_knapping->name() + ", known." );
    CHECK( proficiencies_window_accessibility_tab( you, 0 ) ==
           string_format( "ALL. %d items. ", static_cast<int>( rows.size() ) ) + rows[0] );

    const std::vector<std::string> details = proficiencies_window_accessibility_details( you, 0,
            leather_index );
    REQUIRE( details.size() >= 6 );
    CHECK( details[0] == string_format( "%s %d of %d.", leather_row, leather_index + 1,
                                        static_cast<int>( rows.size() ) ) );
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.rfind( "Progress: ", 0 ) == 0;
    } ) );
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
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
    you.wear_item( item( itype_longshirt ), false );
    you.wear_item( item( itype_hoodie ), false );
    you.wear_item( item( itype_sweatshirt ), false );

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
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.find( "Wearing multiple items" ) != std::string::npos;
    } ) );
    CHECK( std::find( details.begin(), details.end(), "Encumbrance and Warmth." ) != details.end() );
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
        return line.find( "covered by this item" ) != std::string::npos;
    } ) );
    for( const std::string &line : details ) {
        CHECK( line.find( "<color" ) == std::string::npos );
    }
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
                                npc_template_test_talker );
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
    you.worn.wear_item( you, item( itype_backpack ), false, false );
    item_location own_hammer = you.i_add( item( itype_hammer ) );
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
    item &goods = here.add_item( trader_pos, item( itype_hammer ) );
    goods.set_owner( trader );
    const item_location goods_loc( map_cursor( trader_pos ), &goods );

    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    trade_ui trade( you, trader );
    // Lay the panes out like the other test selectors instead of at half the screen size.
    trade.get_active_pane().resize( point::north_west, point::north_west );
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
    your_pane.resize( point::north_west, point::north_west );
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

TEST_CASE( "missions_screen_speaks_categories_rows_tracking_and_details", "[accessibility]" )
{
    using accessibility::mission_category;
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
        mission *miss = mission::reserve_new( mission_TEST_MISSION_GOAL_CONDITION2, character_id() );
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
    CHECK( std::any_of( details.begin(), details.end(), []( std::string_view line ) {
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

TEST_CASE( "zones_manager_speaks_focus_edits_order_and_return_focus", "[accessibility]" )
{
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
    mgr.add( "Pantry", zone_type_LOOT_FOOD, fac, false, true, origin + tripoint_rel_ms( -1, -6, 0 ),
             origin + tripoint_rel_ms( 1, -4, 0 ), nullptr, true );
    mgr.add( "Sorting", zone_type_LOOT_UNSORTED, fac, false, true, origin, origin, nullptr, true );

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

    const std::string pantry = "Pantry, " + mgr.get_name_from_type( zone_type_LOOT_FOOD ) +
                               ", enabled, 5 tiles north.";
    const std::string sorting = "Sorting, " + mgr.get_name_from_type( zone_type_LOOT_UNSORTED ) +
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
        shared_ptr_fast<zone_options> options = zone_options::create( zone_type_LOOT_CUSTOM );
        dynamic_cast<loot_options &>( *options ).set_mark( "apple" );
        mgr.add( "Apples", zone_type_LOOT_CUSTOM, fac, false, true, origin + tripoint_rel_ms( 2, -1, 0 ),
                 origin + tripoint_rel_ms( 6, 1, 0 ), options, true );
        zones = mgr.get_zones( faction );
        index = 2;
    } );
    REQUIRE( zones.size() == 3 );
    const std::string apples = "Apples, " + mgr.get_name_from_type( zone_type_LOOT_CUSTOM ) +
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
    // input_context::handle_input() answers repeat-last; the screen speaks nothing more.
    const size_t spoken_before_repeat = capture_ptr->messages.size();
    act( "ACCESSIBILITY_REPEAT_LAST", nothing );
    CHECK( capture_ptr->messages.size() == spoken_before_repeat );

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

TEST_CASE( "vehicle_examination_speaks_part_list_tiles_tasks_and_stats", "[accessibility]" )
{
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
    CHECK( front.tile.x == front.max.x );

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
    CHECK( std::any_of( tasks[0].second.begin(), tasks[0].second.end(), []( std::string_view line ) {
        return line.rfind( "Time required: ", 0 ) == 0;
    } ) );
    CHECK( std::any_of( tasks[0].second.begin(), tasks[0].second.end(), []( std::string_view line ) {
        return line.rfind( "Skills required: ", 0 ) == 0;
    } ) );
    CHECK( tasks[1].first.rfind( "Remove", 0 ) == 0 );
    CHECK( std::any_of( tasks[1].second.begin(),
    tasks[1].second.end(), [&name]( std::string_view line ) {
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

TEST_CASE( "character_creation_rows_speak_selection_availability_and_conflicts",
           "[accessibility]" )
{
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
                profession_loves_books.obj(), you, true, 10 ) ) == "Reading, 1 point." );
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_background_row(
                profession_loves_books.obj(), you, true, -5 ) ) == "Reading, 1 point. Not enough points." );
    you.hobbies.insert( &profession_hates_books.obj() );
    you.toggle_trait_deps( trait_HATES_BOOKS );
    REQUIRE( you.has_trait( trait_HATES_BOOKS ) );
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_background_row(
                profession_loves_books.obj(), you, false, 0 ) ) ==
           "Reading. Unavailable: Bookworm conflicts with background Book Hating." );
    CHECK( accessibility::format_character_choice_row( accessibility_gameplay::character_background_row(
                profession_hates_books.obj(), you, true, 0 ) ) == "Book Hating, earns 1 point, selected." );

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

TEST_CASE( "character_creation_trait_rows_speak_locks_conflicts_and_prohibitions",
           "[accessibility]" )
{
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

TEST_CASE( "mod_manager_rows_speak_state_dependencies_and_load_order", "[accessibility]" )
{
    mod_ui mods( world_generator->get_mod_manager() );
    REQUIRE( MOD_INFORMATION_dda.is_valid() );
    REQUIRE( MOD_INFORMATION_magiclysm.is_valid() );
    REQUIRE( MOD_INFORMATION_aftershock.is_valid() );
    REQUIRE( MOD_INFORMATION_aftershock_exoplanet.is_valid() );
    REQUIRE( MOD_INFORMATION_mindovermatter.is_valid() );
    const std::string content = MOD_INFORMATION_magiclysm->category.second.translated();

    // Adding a mod also adds its dependencies; the speech names both.
    std::vector<mod_id> active;
    CHECK( accessibility_gameplay::mod_add_refusal( mods, MOD_INFORMATION_magiclysm, active ).empty() );
    mods.try_add( MOD_INFORMATION_magiclysm, active );
    REQUIRE( active == std::vector<mod_id> { MOD_INFORMATION_dda, MOD_INFORMATION_magiclysm } );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            MOD_INFORMATION_magiclysm, active, true ) ) == "Magiclysm, active, " + content + "." );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            MOD_INFORMATION_magiclysm, active, false ) ) == "Magiclysm, " + content + "." );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            MOD_INFORMATION_aftershock, active, true ) ) == "Aftershock, obsolete, OBSOLETE MODS." );
    CHECK( accessibility_gameplay::mod_add_refusal( mods, MOD_INFORMATION_magiclysm, active ) ==
           "already in the load order" );
    CHECK( accessibility_gameplay::mod_add_refusal( mods, MOD_INFORMATION_accessibility_missing_mod,
            active ) == "not installed" );
    CHECK( accessibility::format_world_mod_row( accessibility_gameplay::world_mod_row( mods,
            MOD_INFORMATION_accessibility_missing_mod, active,
            false ) ) == "accessibility_missing_mod, not installed, MISSING MODS." );
    CHECK( accessibility_gameplay::mod_unavailable_reason( mods, MOD_INFORMATION_magiclysm ).empty() );

    // Moves are refused with the reason the visual +/- marks only imply.
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 0, active, true ) == "already first" );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 0, active, false ) ==
           "the core mod loads first" );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 1, active, true ) ==
           "Dark Days Ahead loads first" );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 1, active, false ) == "already last" );
    mods.try_add( MOD_INFORMATION_aftershock, active );
    REQUIRE( active == std::vector<mod_id> { MOD_INFORMATION_dda, MOD_INFORMATION_magiclysm, MOD_INFORMATION_aftershock } );
    CHECK( accessibility_gameplay::mod_move_refusal( mods, 2, active, true ).empty() );
    size_t selection = 2;
    mods.try_shift( '-', selection, active );
    CHECK( selection == 1 );
    CHECK( accessibility::format_mod_moved( selection + 1, active.size() ) == "Position 2 of 3." );

    // A mod conflicting with the load order is marked like the visual list.
    const std::vector<mod_id> conflicting { MOD_INFORMATION_dda, MOD_INFORMATION_aftershock_exoplanet };
    CHECK( accessibility_gameplay::world_mod_row( mods, MOD_INFORMATION_mindovermatter, conflicting,
            true ).incompatible );
    CHECK_FALSE( accessibility_gameplay::world_mod_row( mods, MOD_INFORMATION_mindovermatter, active,
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

TEST_CASE( "rule_test_lists_speak_title_matches_and_position", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    input_context ctxt( "AUTO_PICKUP_TEST", keyboard_mode::keycode );
    ctxt.register_navigate_ui_list();
    const std::vector<std::string> matches { "arrow", "wooden arrow" };
    const std::string title = "2 items match: *arrow";
    const std::string note = "Won't display content or suffix matches";
    accessibility_gameplay::match_list_speech( ctxt, title, note, matches, 0, std::string(), 0 );
    CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 's',
                                 input_event_t::keyboard_code ) ) == "ACCESSIBILITY_STATUS" );
    accessibility_gameplay::match_list_speech( ctxt, title, note, matches, 1, "DOWN", 0 );
    accessibility_gameplay::match_list_speech( ctxt, title, note, matches, 0, "DOWN", 1 );
    accessibility_gameplay::match_list_speech( ctxt, title, note, matches, 0, "ACCESSIBILITY_STATUS",
            0 );
    accessibility_gameplay::match_list_speech( ctxt, title, note, matches, 0, "ACCESSIBILITY_DETAILS",
            0 );
    // Other keys stay silent.
    accessibility_gameplay::match_list_speech( ctxt, title, note, matches, 0, "HELP_KEYBINDINGS", 0 );

    REQUIRE( capture_ptr->messages.size() == 5 );
    CHECK( capture_ptr->messages[0] == "2 items match: *arrow. arrow." );
    CHECK( capture_ptr->messages[1] == "wooden arrow." );
    CHECK( capture_ptr->messages[2] == "Top. arrow." );
    CHECK( capture_ptr->messages[3] == "2 items match: *arrow. 1 of 2." );
    CHECK( capture_ptr->messages[4] == "Won't display content or suffix matches" );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "options_rule_and_panel_screens_keep_their_keys_with_accessibility_keys",
           "[accessibility]" )
{
    const std::map<std::string, std::vector<std::pair<char, std::string>>> contexts {
        { "OPTIONS", {} },
        { "SAFEMODE", { { 's', "SWITCH_SAFEMODE_OPTION" }, { 'd', "DISABLE_RULE" }, { 'e', "ENABLE_RULE" } } },
        { "AUTO_PICKUP", { { 's', "SWITCH_AUTO_PICKUP_OPTION" }, { 'd', "DISABLE_RULE" }, { 't', "TEST_RULE" } } },
        { "PANEL_MGMT", { { 's', "MOVE_PANEL" } } },
        { "SAFEMODE_TEST", {} },
        { "AUTO_PICKUP_TEST", {} }
    };
    for( const auto &context : contexts ) {
        CAPTURE( context.first );
        for( const keyboard_mode mode : {
                 keyboard_mode::keycode, keyboard_mode::keychar
             } ) {
            input_context ctxt( context.first, mode );
            ctxt.register_action( "ACCESSIBILITY_DETAILS" );
            ctxt.register_action( "ACCESSIBILITY_STATUS" );
            ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
            for( const std::pair<char, std::string> &key : context.second ) {
                ctxt.register_action( key.second );
            }
            const input_event_t type = mode == keyboard_mode::keycode ? input_event_t::keyboard_code :
                                       input_event_t::keyboard_char;
            CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'd', type ) ) ==
                   "ACCESSIBILITY_DETAILS" );
            CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 's', type ) ) ==
                   "ACCESSIBILITY_STATUS" );
            CHECK( ctxt.input_to_action( input_event( { keymod_t::alt }, 'r', type ) ) ==
                   "ACCESSIBILITY_REPEAT_LAST" );
            for( const std::pair<char, std::string> &key : context.second ) {
                CHECK( ctxt.input_to_action( input_event( key.first, type ) ) == key.second );
            }
        }
    }
}

// NOLINTEND(cata-text-style)
