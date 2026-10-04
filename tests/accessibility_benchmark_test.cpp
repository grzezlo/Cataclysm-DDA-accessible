#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
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
#include "inventory_ui.h"
#include "item.h"
#include "item_location.h"
#include "item_stack.h"
#include "map_selector.h"
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
            auto capture = std::make_unique<capture_backend>();
            captured = capture.get();
            accessibility::set_backend_for_testing( std::move( capture ) );
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
        capture_backend &backend() {
            return *captured;
        }
    private:
        capture_backend *captured = nullptr;
};

class benchmark_multiselector final : public inventory_multiselector
{
    public:
        explicit benchmark_multiselector( Character &you ) : inventory_multiselector( you ) {}
        void select_all() {
            std::vector<inventory_entry *> entries;
            for( inventory_column *column : get_all_columns() ) {
                const std::vector<inventory_entry *> items = column->get_entries(
                []( const inventory_entry & entry ) {
                    return entry.is_item();
                }, true );
                entries.insert( entries.end(), items.begin(), items.end() );
            }
            for( inventory_entry *entry : entries ) {
                toggle_entry( *entry, entry->get_available_count() );
            }
        }
        const std::vector<std::pair<item_location, int>> &selection() const {
            return to_use;
        }
};

using profile_clock = std::chrono::steady_clock;

double microseconds_since( const profile_clock::time_point start )
{
    return std::chrono::duration<double, std::micro>( profile_clock::now() - start ).count();
}

void rebuild_caches( map &here, const int z )
{
    here.invalidate_map_cache( z );
    here.build_map_cache( z, true );
    here.update_visibility_cache( z );
}

} // namespace

// Isolate the exact snapshot and comparison in inventory_multiselector::on_input().
// Populate to_use through real selection operations, rather than a synthetic vector.
TEST_CASE( "accessibility_benchmark_large_unchanged_selection",
           "[.][benchmark][accessibility_profile]" )
{
    for( const int count : {
             10, 100, 1000
         } ) {
        for( const bool speaking : {
                 true, false
             } ) {
            DYNAMIC_SECTION( count << " selected items, speech " << ( speaking ? "on" : "off" ) ) {
                clear_avatar();
                clear_map();
                const on_out_of_scope cleanup( []() {
                    clear_map();
                    clear_avatar();
                } );
                benchmark_speech speech( speaking );
                map &here = get_map();
                const tripoint_bub_ms pile( 60, 60, 0 );
                for( int index = 0; index < count; ++index ) {
                    here.add_item( pile, item( itype_rock ) );
                }
                benchmark_multiselector selector( get_avatar() );
                selector.add_map_items( pile );
                const item_location first( map_cursor( pile ), &*here.i_at( pile ).begin() );
                REQUIRE( selector.highlight_one_of( { first } ) );
                selector.select_all();
                REQUIRE( selector.selection().size() == static_cast<size_t>( count ) );
                BENCHMARK( "unchanged selection snapshot and comparison" ) {
                    if( !accessibility::is_active() ) {
                        return true;
                    }
                    const std::vector<std::pair<item_location, int>> before = selector.selection();
                    return selector.selection() == before;
                };
                CHECK( speech.backend().messages.empty() );
            }
        }
    }
}

TEST_CASE( "accessibility_profile_wait_components", "[.][accessibility_profile]" )
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
    const std::array<const char *, 5> names = { "activity.do_turn (includes speech scope/progress)",
                                                "threat/condition checks", "flush", "second progress update", "poll"
                                              };
    for( const bool speaking : {
             true, false
         } ) {
        for( int sample = 0; sample < 3; ++sample ) {
            benchmark_speech speech( speaking );
            set_time( previous_time );
            you.assign_activity( ACT_WAIT, to_moves<int>( 8_hours ) );
            std::array<double, 5> elapsed = {};
            int turns = 0;
            const auto start = profile_clock::now();
            while( you.activity ) {
                you.set_moves( 100 );
                auto part = profile_clock::now();
                you.activity.do_turn( you );
                elapsed[0] += microseconds_since( part );
                calendar::turn += 1_turns;
                part = profile_clock::now();
                accessibility_gameplay::announce_turn_alerts( here, you, {} );
                elapsed[1] += microseconds_since( part );
                part = profile_clock::now();
                accessibility::flush_gameplay_announcements();
                elapsed[2] += microseconds_since( part );
                part = profile_clock::now();
                accessibility_gameplay::update_activity_progress( you );
                elapsed[3] += microseconds_since( part );
                part = profile_clock::now();
                accessibility::poll_progress();
                elapsed[4] += microseconds_since( part );
                ++turns;
            }
            accessibility::flush_gameplay_announcements();
            const double total = microseconds_since( start );
            REQUIRE( turns == to_turns<int>( 8_hours ) );
            std::cout << "wait speech=" << speaking << " sample=" << sample << " turns=" << turns
                      << " total_ms=" << total / 1000 << " backend_calls=" << speech.backend().messages.size()
                      << " cancels=" << speech.backend().cancels << '\n';
            for( size_t index = 0; index < names.size(); ++index ) {
                std::cout << "  " << names[index] << " ms=" << elapsed[index] / 1000
                          << " us/turn=" << elapsed[index] / turns << '\n';
            }
            CHECK( speech.backend().messages.size() == ( speaking ? 2 : 0 ) );
        }
    }
}

TEST_CASE( "accessibility_profile_movement_with_sixty_creatures",
           "[.][accessibility_profile]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const time_point previous_time = calendar::turn;
    const on_out_of_scope cleanup( [&]() {
        clear_map();
        clear_avatar();
        set_time( previous_time );
    } );
    set_time_to_day();
    const tripoint_bub_ms origin( 60, 60, 0 );
    you.setpos( here, origin );
    std::vector<const Creature *> crowd;
    for( int index = 0; index < 60; ++index ) {
        crowd.push_back( &spawn_test_monster( mon_zombie.str(),
                                              origin + tripoint_rel_ms( 6 + index % 8, -4 + index / 8, 0 ) ) );
    }
    const auto visible_threats = [&]() {
        std::vector<const Creature *> visible;
        for( const Creature *creature : crowd ) {
            if( you.sees( here, *creature ) ) {
                visible.push_back( creature );
            }
        }
        return visible;
    };
    rebuild_caches( here, origin.z() );
    REQUIRE( visible_threats().size() == 60 );
    for( int y = -6; y <= 6; ++y ) {
        here.ter_set( origin + tripoint_rel_ms( 2, y, 0 ), ter_t_wall );
    }
    rebuild_caches( here, origin.z() );
    REQUIRE( visible_threats().empty() );
    // Walk up the west side, around the end, and onto the east side of the wall.
    std::vector<tripoint_bub_ms> route;
    for( int y = 0; y >= -9; --y ) {
        route.push_back( origin + tripoint_rel_ms( 0, y, 0 ) );
    }
    for( int x = 1; x <= 3; ++x ) {
        route.push_back( origin + tripoint_rel_ms( x, -9, 0 ) );
    }
    for( int y = -8; y <= 0; ++y ) {
        route.push_back( origin + tripoint_rel_ms( 3, y, 0 ) );
    }
    const size_t outward = route.size();
    for( size_t index = outward - 1; index > 0; --index ) {
        route.push_back( route[index - 1] );
    }
    for( const bool speaking : {
             true, false
         } ) {
        for( int sample = 0; sample < 3; ++sample ) {
            benchmark_speech speech( speaking );
            size_t maximum_visible = 0;
            size_t visibility_changes = 0;
            size_t previous_visible = 0;
            double visibility_us = 0;
            double hooks_us = 0;
            double maximum_hooks_us = 0;
            const auto start = profile_clock::now();
            for( const tripoint_bub_ms &position : route ) {
                const tripoint_abs_ms old = you.pos_abs();
                you.setpos( here, position );
                const auto cache_start = profile_clock::now();
                rebuild_caches( here, position.z() );
                const std::vector<const Creature *> visible = visible_threats();
                visibility_us += microseconds_since( cache_start );
                maximum_visible = std::max( maximum_visible, visible.size() );
                visibility_changes += visible.size() != previous_visible;
                previous_visible = visible.size();
                const auto hooks_start = profile_clock::now();
                accessibility::note_player_input();
                accessibility_gameplay::announce_avatar_moved( old, you, here );
                accessibility_gameplay::announce_turn_alerts( here, you, visible );
                accessibility::flush_gameplay_announcements();
                const double hooks = microseconds_since( hooks_start );
                hooks_us += hooks;
                maximum_hooks_us = std::max( maximum_hooks_us, hooks );
                // A routine redraw of the same threats must not add backend calls.
                const size_t spoken = speech.backend().messages.size();
                accessibility_gameplay::announce_turn_alerts( here, you, visible );
                accessibility::flush_gameplay_announcements();
                REQUIRE( speech.backend().messages.size() == spoken );
            }
            const double total = microseconds_since( start );
            REQUIRE( maximum_visible == 60 );
            REQUIRE( visibility_changes > 2 );
            CHECK( speech.backend().messages.size() <= route.size() );
            CHECK( speech.backend().messages.empty() == !speaking );
            std::cout << "crowd speech=" << speaking << " sample=" << sample << " steps=" << route.size()
                      << " max_visible=" << maximum_visible << " visibility_changes=" << visibility_changes
                      << " total_ms=" << total / 1000 << " visibility_ms=" << visibility_us / 1000
                      << " hooks_us/step=" << hooks_us / route.size() << " hooks_max_us=" << maximum_hooks_us
                      << " backend_calls=" << speech.backend().messages.size()
                      << " cancels=" << speech.backend().cancels << '\n';
            if( speaking ) {
                const std::string latest = accessibility::last_announcement();
                accessibility::note_player_input();
                REQUIRE( accessibility::repeat_last_announcement() );
                CHECK( speech.backend().messages.back() == latest );
            }
        }
    }
}

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
    threats.reserve( 10 );
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
