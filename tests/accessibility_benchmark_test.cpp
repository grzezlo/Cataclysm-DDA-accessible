#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "accessibility.h"
#include "accessibility_gameplay.h"
#include "accessibility_test_helpers.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "coordinates.h"
#include "creature.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "point.h"
#include "type_id.h"
#include "veh_interact.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "vpart_range.h"

// Manual measurements for the accessibility hooks on frequent paths.  Run
// before and after a change, for example:
// Cataclysm-test-vcpkg-static-Release-x64.exe "[accessibility_benchmark]" --benchmark-samples 20
// Every benchmark runs with the screen reader on and off; off must show no speech work.

static const activity_id ACT_WAIT( "ACT_WAIT" );

static const itype_id itype_2x4( "2x4" );
static const itype_id itype_hammer( "hammer" );
static const itype_id itype_rock( "rock" );

static const mtype_id mon_zombie( "mon_zombie" );

static const ter_str_id ter_t_wall( "t_wall" );

static const vproto_id vehicle_prototype_car( "car" );

namespace
{

/** Install the capture backend in the requested mode until destroyed. */
class benchmark_speech
{
    public:
        explicit benchmark_speech( const bool speaking ) {
            accessibility::set_backend_for_testing( std::make_unique<capture_backend>() );
            accessibility::set_screen_reader_mode( speaking ? accessibility::screen_reader_mode::on :
                                                   accessibility::screen_reader_mode::off );
            accessibility::enable_game_messages( true );
            accessibility::reset_game_state();
        }
        ~benchmark_speech() {
            accessibility::reset_game_state();
            accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::automatic );
            accessibility::reset_backend_after_testing();
        }
        benchmark_speech( const benchmark_speech & ) = delete;
        benchmark_speech &operator=( const benchmark_speech & ) = delete;
};

void rebuild_caches( map &here, const int z )
{
    here.invalidate_map_cache( z );
    here.build_map_cache( z, true );
    here.update_visibility_cache( z );
}

} // namespace

TEST_CASE( "accessibility_benchmark_vehicle_workshop_tasks",
           "[.][benchmark][accessibility_benchmark]" )
{
    map &here = get_map();
    clear_avatar();
    clear_map();
    clear_vehicles();
    const on_out_of_scope cleanup( []() {
        clear_vehicles();
        clear_map();
        clear_avatar();
    } );
    const tripoint_bub_ms origin( 60, 60, 0 );
    vehicle *veh = here.add_vehicle( vehicle_prototype_car, origin, 0_degrees, 100, 0 );
    REQUIRE( veh != nullptr );
    get_avatar().setpos( here, origin + tripoint_rel_ms( 0, -4, 0 ) );
    rebuild_caches( here, origin.z() );
    const int part = accessibility_gameplay::vehicle_part_list( *veh ).front();

    for( const bool speaking : {
             true, false
         } ) {
        DYNAMIC_SECTION( ( speaking ? "speech on" : "speech off" ) ) {
            benchmark_speech speech( speaking );
            BENCHMARK( "task menu" ) {
                return veh_interact::accessible_tasks_for_testing( here, *veh, part ).size();
            };
        }
    }
}

TEST_CASE( "accessibility_benchmark_eight_hours_of_waiting",
           "[.][benchmark][accessibility_benchmark]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const time_point previous_time = calendar::turn;
    const on_out_of_scope cleanup( [&]() {
        you.cancel_activity();
        clear_avatar();
        set_time( previous_time );
    } );

    for( const bool speaking : {
             true, false
         } ) {
        DYNAMIC_SECTION( ( speaking ? "speech on" : "speech off" ) ) {
            benchmark_speech speech( speaking );
            BENCHMARK( "ACT_WAIT, 8 hours" ) {
                you.assign_activity( ACT_WAIT, to_moves<int>( 8_hours ) );
                int turns = 0;
                // The accessibility calls of a do_turn() pass while the activity runs.
                while( you.activity ) {
                    you.set_moves( 100 );
                    you.activity.do_turn( you );
                    calendar::turn += 1_turns;
                    accessibility_gameplay::announce_turn_alerts( here, you, {} );
                    accessibility::flush_gameplay_announcements();
                    accessibility_gameplay::update_activity_progress( you );
                    accessibility::poll_progress();
                    ++turns;
                }
                accessibility::flush_gameplay_announcements();
                return turns;
            };
        }
    }
}

TEST_CASE( "accessibility_benchmark_look_around_over_many_items",
           "[.][benchmark][accessibility_benchmark]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const on_out_of_scope cleanup( []() {
        clear_map();
        clear_avatar();
    } );
    set_time_to_day();
    const tripoint_bub_ms origin = you.pos_bub( here );
    const tripoint_bub_ms pile = origin + tripoint_rel_ms( 2, 0, 0 );
    const tripoint_bub_ms empty = origin + tripoint_rel_ms( 0, 2, 0 );
    const std::vector<itype_id> kinds = { itype_rock, itype_2x4, itype_hammer };
    for( int count = 0; count < 100; ++count ) {
        here.add_item( pile, item( kinds[count % kinds.size()] ) );
    }
    rebuild_caches( here, origin.z() );
    const visibility_variables &cache = here.get_visibility_variables_cache();

    for( const bool speaking : {
             true, false
         } ) {
        DYNAMIC_SECTION( ( speaking ? "speech on" : "speech off" ) ) {
            benchmark_speech speech( speaking );
            BENCHMARK( "cursor to the pile and back" ) {
                // look_around() builds the parts only when speech is active.
                size_t parts = 0;
                if( accessibility::is_active() ) {
                    parts += accessibility_gameplay::look_parts( here, you, origin, pile, cache ).size();
                    parts += accessibility_gameplay::look_parts( here, you, origin, empty, cache ).size();
                }
                return parts;
            };
        }
    }
}

TEST_CASE( "accessibility_benchmark_turn_alerts_with_visible_threats",
           "[.][benchmark][accessibility_benchmark]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const on_out_of_scope cleanup( []() {
        clear_map();
        clear_avatar();
    } );
    const tripoint_bub_ms origin = you.pos_bub( here );
    std::vector<const Creature *> threats;
    for( int index = 0; index < 10; ++index ) {
        threats.push_back( &spawn_test_monster( mon_zombie.str(),
                                                origin + tripoint_rel_ms( 3 + index, 5, 0 ) ) );
    }

    for( const bool speaking : {
             true, false
         } ) {
        DYNAMIC_SECTION( ( speaking ? "speech on" : "speech off" ) ) {
            benchmark_speech speech( speaking );
            accessibility_gameplay::announce_turn_alerts( here, you, threats );
            accessibility::flush_gameplay_announcements();
            BENCHMARK( "unchanged threats and conditions, twice per turn" ) {
                accessibility_gameplay::announce_turn_alerts( here, you, threats );
                accessibility_gameplay::announce_turn_alerts( here, you, threats );
                return accessibility::flush_gameplay_announcements();
            };
            accessibility_gameplay::announce_turn_alerts( here, you, {} );
            accessibility::flush_gameplay_announcements();
            BENCHMARK( "no threats, unchanged conditions, twice per turn" ) {
                accessibility_gameplay::announce_turn_alerts( here, you, {} );
                accessibility_gameplay::announce_turn_alerts( here, you, {} );
                return accessibility::flush_gameplay_announcements();
            };
        }
    }
}

TEST_CASE( "accessibility_benchmark_driving_obstacles",
           "[.][benchmark][accessibility_benchmark]" )
{
    avatar &you = get_avatar();
    map &here = get_map();
    clear_avatar();
    clear_map();
    clear_vehicles();
    const on_out_of_scope cleanup( [&]() {
        you.controlling_vehicle = false;
        if( you.in_vehicle ) {
            here.unboard_vehicle( you.pos_bub() );
        }
        clear_vehicles();
        clear_map();
        clear_avatar();
    } );
    set_time_to_day();
    const tripoint_bub_ms origin( 60, 60, 0 );
    vehicle *veh = here.add_vehicle( vehicle_prototype_car, origin, 0_degrees, 100, 0 );
    REQUIRE( veh != nullptr );
    std::optional<tripoint_bub_ms> controls;
    for( const vpart_reference &vp : veh->get_avail_parts( "CONTROLS" ) ) {
        controls = vp.pos_bub( here );
        break;
    }
    REQUIRE( controls.has_value() );
    you.setpos( here, *controls );
    here.board_vehicle( *controls, &you );
    you.controlling_vehicle = true;
    veh->engine_on = true;
    veh->turn_dir = veh->face.dir();
    // Fast enough for the longest look-ahead, with walls across the path.
    veh->velocity = 4000;
    for( int y = -6; y <= 6; ++y ) {
        here.ter_set( origin + tripoint_rel_ms( 12, y, 0 ), ter_t_wall );
        here.ter_set( origin + tripoint_rel_ms( 18, y, 0 ), ter_t_wall );
    }
    rebuild_caches( here, origin.z() );

    for( const bool speaking : {
             true, false
         } ) {
        DYNAMIC_SECTION( ( speaking ? "speech on" : "speech off" ) ) {
            benchmark_speech speech( speaking );
            BENCHMARK( "obstacle scan before input" ) {
                return accessibility_gameplay::vehicle_obstacle_transition( here, you ).has_value();
            };
        }
    }
}
