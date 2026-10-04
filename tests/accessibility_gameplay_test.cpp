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
#include "sounds.h"
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

// Expected speech strings mirror the spoken output, which uses single spaces after sentences.
// NOLINTBEGIN(cata-text-style)

static const activity_id ACT_HACKSAW( "ACT_HACKSAW" );
static const activity_id ACT_WAIT( "ACT_WAIT" );

static const efftype_id effect_blind( "blind" );
static const efftype_id effect_sleep( "sleep" );

static const flag_id json_flag_CAN_USE_IN_DARK( "CAN_USE_IN_DARK" );

static const furn_str_id furn_f_table( "f_table" );

static const itype_id itype_backpack( "backpack" );
static const itype_id itype_child_book( "child_book" );
static const itype_id itype_hammer( "hammer" );
static const itype_id itype_rock( "rock" );

static const mission_type_id mission_TEST_MISSION_GOAL_CONDITION2( "TEST_MISSION_GOAL_CONDITION2" );

static const oter_str_id oter_field( "field" );

static const ter_str_id ter_t_door_c( "t_door_c" );
static const ter_str_id ter_t_floor( "t_floor" );
static const ter_str_id ter_t_wall( "t_wall" );

static const vproto_id vehicle_prototype_car( "car" );

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
    you.assign_activity( ACT_WAIT, to_moves<int>( 2_minutes ) );
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

    you.assign_activity( ACT_HACKSAW, to_moves<int>( 1_hours ) );
    accessibility_gameplay::activity_interruption_reason( "Zombie spotted!" );
    you.cancel_activity();
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages.back().find( "Stopped:" ) != std::string::npos );
    CHECK( speech->messages.back().find( "Zombie spotted!" ) != std::string::npos );
    you.assign_activity( ACT_HACKSAW, to_moves<int>( 1_hours ) );
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
    item_location book = you.i_add( item( itype_child_book ) );
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
            you.add_effect( effect_blind, 1_hours );
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
            book->set_flag( json_flag_CAN_USE_IN_DARK );
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
    you.remove_effect( effect_sleep );
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
    you.assign_activity( ACT_HACKSAW, to_moves<int>( 1_hours ) );
    REQUIRE( speech->messages.size() == 1 );
    {
        accessibility_gameplay::activity_turn_speech turn( you );
        accessibility_gameplay::activity_completed( you );
        you.cancel_activity();
        accessibility::announce_game_message( "You craft the first knife." );
        you.assign_activity( ACT_HACKSAW, to_moves<int>( 1_hours ) );
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

TEST_CASE( "nearby_list_speech_speaks_context_moves_summary_and_refocus",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    const std::vector<std::string> names = { "rock", "stick", "hammer" };
    int row = 0;
    const auto entry = [&]() {
        accessibility::nearby_list_entry result;
        result.name = names[row];
        result.result_index = row + 1;
        result.result_count = 3;
        return result;
    };
    accessibility_gameplay::nearby_list_speech list( "Nearby items" );

    // Opening speaks the context and the focus once.
    list.update( row, 1, 3, 0, true, entry );
    REQUIRE( speech->messages.size() == 1 );
    CHECK( speech->messages.back().rfind( "Nearby items. 3 results.", 0 ) == 0 );
    CHECK( speech->messages.back().find( "rock" ) != std::string::npos );
    list.update( row, 1, 3, 0, true, entry );
    CHECK( speech->messages.size() == 1 );

    // A move speaks the row, with "Top" after wrapping.
    row = 2;
    list.update( row, 1, 3, -1, true, entry );
    REQUIRE( speech->messages.size() == 2 );
    CHECK( speech->messages.back().rfind( "Bottom. hammer", 0 ) == 0 );
    row = 0;
    list.update( row, 1, 3, 1, true, entry );
    REQUIRE( speech->messages.size() == 3 );
    CHECK( speech->messages.back().rfind( "Top. rock", 0 ) == 0 );

    // The summary, a refocus, and a changed context are each spoken once.
    list.request_summary();
    list.update( row, 1, 3, 0, true, entry );
    REQUIRE( speech->messages.size() == 4 );
    CHECK( speech->messages.back() ==
           accessibility::format_nearby_list_summary( "Nearby items", 3, entry() ) );
    list.refocus();
    list.update( row, 1, 3, 0, true, entry );
    REQUIRE( speech->messages.size() == 5 );
    CHECK( speech->messages.back().rfind( "rock", 0 ) == 0 );
    list.context_changed( "Sorted by name" );
    list.update( row, 1, 3, 0, true, entry );
    REQUIRE( speech->messages.size() == 6 );
    CHECK( speech->messages.back().rfind( "Nearby items. Sorted by name. 3 results.", 0 ) == 0 );
    list.update( row, 1, 3, 0, true, entry );
    CHECK( speech->messages.size() == 6 );
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
    const item rock( itype_rock, calendar::turn );
    here.add_item_or_charges( origin + tripoint::east, rock );
    here.add_item_or_charges( origin + tripoint::south, rock );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );

    const std::vector<map_item_stack> visible_items = g->find_nearby_items( 10 );
    const auto rock_stack = std::find_if( visible_items.begin(), visible_items.end(),
    [&]( const map_item_stack & stack ) {
        return stack.example != nullptr && stack.example->typeId() == itype_rock;
    } );
    REQUIRE( rock_stack != visible_items.end() );
    CHECK( rock_stack->totalcount == 2 );
    CHECK( rock_stack->vIG.size() == 2 );

    player_wear_blindfold();
    REQUIRE( viewer.is_blind() );
    CHECK( g->find_nearby_items( 10 ).empty() );
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
    here.ter_set( door, ter_t_door_c );
    here.furn_set( table, furn_f_table );
    here.add_item_or_charges( table, item( itype_rock, calendar::turn ) );
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
    here.ter_set( route[2], ter_t_floor );
    here.ter_set( route[3], ter_t_floor );
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
        you.worn.wear_item( you, item( itype_backpack ), false, false );
        pick_up( "hammer", 2 );
        REQUIRE( !capture_ptr->messages.empty() );
        CHECK( capture_ptr->messages.back() == "Picked up: 2 hammers into backpack." );
        CHECK( accessibility::repeat_last_announcement() );
        CHECK( capture_ptr->messages.back() == "Picked up: 2 hammers into backpack." );
    }

    SECTION( "stacks counted in charges name the container" ) {
        you.worn.wear_item( you, item( itype_backpack ), false, false );
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
        item( itype_hammer ), item( itype_rock ), item( itype_hammer ),
        item( itype_rock )
    } );
    accessibility::flush_gameplay_announcements();
    REQUIRE( !capture_ptr->messages.empty() );
    CHECK( capture_ptr->messages.back() ==
           string_format( "Dropped on %s: 2 hammers, rocks (2).", here.name( you.pos_bub() ) ) );

    accessibility::reset_backend_after_testing();
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
    here.ter_set( origin + tripoint( 2, 0, 0 ), ter_t_wall );
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

TEST_CASE( "overmap_cursor_speaks_known_tiles_places_and_routes", "[accessibility]" )
{
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
    // overmapbuffer::set_seen never lowers vision, and earlier tests may have
    // revealed these tiles.
    const auto force_seen = []( const tripoint_abs_omt & pos, const om_vision_level vision ) {
        const overmap_with_local_coords om_loc = overmap_buffer.get_om_global( pos );
        om_loc.om->set_seen( om_loc.local, vision, true );
    };
    std::vector<std::pair<oter_id, om_vision_level>> previous;
    previous.reserve( changed.size() );
    for( const tripoint_abs_omt &pos : changed ) {
        const oter_id previous_ter = overmap_buffer.ter( pos );
        const om_vision_level previous_vision = overmap_buffer.seen( pos );
        previous.emplace_back( previous_ter, previous_vision );
    }
    const on_out_of_scope cleanup( [&]() {
        overmap_buffer.delete_note( hidden );
        for( size_t index = 0; index < changed.size(); ++index ) {
            overmap_buffer.ter_set( changed[index], previous[index].first );
            force_seen( changed[index], previous[index].second );
        }
        you.omt_path.clear();
        you.reset_all_missions();
        mission::clear_all();
        clear_avatar();
        accessibility::reset_backend_after_testing();
    } );
    overmap_buffer.ter_set( north, oter_field.id() );
    overmap_buffer.ter_set( north2, oter_field.id() );
    force_seen( north, om_vision_level::full );
    force_seen( north2, om_vision_level::full );
    force_seen( hidden, om_vision_level::unseen );
    overmap_buffer.add_note( hidden, "B:Base" );
    overmap_buffer.mark_note_dangerous( hidden, 2, true );

    // Unexplored terrain stays hidden; the player's own note is known.
    const accessibility::overmap_tile unseen = accessibility_gameplay::overmap_tile( you, hidden );
    CHECK( unseen.terrain.empty() );
    CHECK( unseen.note == "Base" );
    CHECK( unseen.danger_radius == 2 );
    CHECK( unseen.offset.x == 3 );
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
    mission *miss = mission::reserve_new( mission_TEST_MISSION_GOAL_CONDITION2, character_id() );
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
    CHECK( std::none_of( hidden_details.begin(), hidden_details.end(), []( std::string_view line ) {
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

TEST_CASE( "driving_speaks_entry_controls_obstacles_and_status", "[accessibility]" )
{
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

    // An obstacle behind the known one on the same line is not on the path.
    here.ter_set( ahead + tripoint_rel_ms( 2, 0, 0 ), ter_t_wall );
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    here.update_visibility_cache( origin.z() );
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

TEST_CASE( "look_sound_markers_match_bounds_and_same_level_precedence", "[accessibility]" )
{
    map &here = get_map();
    const tripoint_bub_ms position( 60, 60, 0 );
    std::map<int, std::string> markers;
    int lookups = 0;
    const auto lookup = [&]( const tripoint_bub_ms & p ) {
        ++lookups;
        CHECK( p.xy() == position.xy() );
        const auto found = markers.find( p.z() );
        return found == markers.end() ? std::string() : found->second;
    };
    CHECK( accessibility_gameplay::perceived_sound_parts( here, position, lookup ).empty() );
    markers = { { position.z() - 2, "distant rumble" }, { position.z() - 1, "steps" },
        { position.z() + 1, "crash" }, { position.z() + 2, "a sound" }
    };
    CHECK( accessibility_gameplay::perceived_sound_parts( here, position, lookup ) ==
           std::vector<std::string> { "From below you heard distant rumble",
                                      "From below you heard steps", "From above you heard crash", "From above you heard a sound"
                                    } );
    markers[position.z()] = "local knock";
    lookups = 0;
    CHECK( accessibility_gameplay::perceived_sound_parts( here, position, lookup ) ==
           std::vector<std::string> { "From here you heard local knock" } );
    CHECK( lookups == 1 );
    lookups = 0;
    CHECK( accessibility_gameplay::perceived_sound_parts( here, tripoint_bub_ms( -1, -1,
            position.z() ), lookup ).empty() );
    CHECK( lookups == 0 );
}

TEST_CASE( "look_cursor_reports_real_perceived_markers_without_unseen_creatures",
           "[accessibility]" )
{
    clear_avatar();
    clear_map();
    sounds::reset_sounds();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = you.pos_bub( here );
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    const on_out_of_scope cleanup( []() {
        sounds::reset_sounds();
        accessibility::reset_backend_after_testing();
        clear_avatar();
        clear_map();
    } );
    player_wear_blindfold();
    here.invalidate_map_cache( origin.z() );
    here.build_map_cache( origin.z(), true );
    here.update_visibility_cache( origin.z() );
    const visibility_variables &cache = here.get_visibility_variables_cache();
    const tripoint_bub_ms hidden = origin + tripoint_rel_ms( 8, 0, 0 );
    spawn_test_monster( "mon_zombie", hidden );
    const std::string no_marker = accessibility::format_semantic_sequence(
                                      accessibility_gameplay::look_parts( here, you, origin, hidden, cache ) );
    CHECK( no_marker.find( "zombie" ) == std::string::npos );
    CHECK( no_marker.find( "you heard" ) == std::string::npos );

    for( const bool empty_description : {
             false, true
         } ) {
        sounds::reset_sounds();
        const tripoint_bub_ms source = origin + tripoint_rel_ms( 2, 0, 0 );
        if( empty_description ) {
            sounds::add_footstep( source, 100, 2, nullptr, "" );
        } else {
            sounds::sound( source, 100, sounds::sound_t::order, std::string( "a test knock" ), true );
        }
        sounds::process_sound_markers( &you );
        const std::vector<tripoint_bub_ms> markers = sounds::get_footstep_markers();
        REQUIRE( markers.size() == 1 );
        const tripoint_bub_ms marker = markers.front();
        const std::string description = empty_description ? "a sound" : "a test knock";
        CHECK( sounds::sound_at( marker ) == description );
        input_context ctxt( "LOOK" );
        accessibility_gameplay::announce_look_cursor( here, you, ctxt, origin, marker, cache, false,
                false );
        const std::string announced = accessibility::last_announcement();
        CHECK( announced.find( "From here you heard " + description ) != std::string::npos );
        CHECK( announced.find( "zombie" ) == std::string::npos );
        const size_t count = speech->messages.size();
        CHECK( accessibility::repeat_last_announcement() );
        CHECK( speech->messages.size() == count + 1 );
        CHECK( speech->messages.back() == announced );
        sounds::reset_markers();
        const std::string reset = accessibility::format_semantic_sequence(
                                      accessibility_gameplay::look_parts( here, you, origin, marker, cache ) );
        CHECK( reset.find( "you heard" ) == std::string::npos );
    }
}

// NOLINTEND(cata-text-style)
