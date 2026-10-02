#include "veh_interact.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "accessibility.h"
#include "accessibility_document_view.h"
#include "accessibility_format.h"
#include "accessibility_gameplay.h"
#include "activity_handlers.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_scope_helpers.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "character_id.h"
#include "contents_change_handler.h"
#include "crafting.h"
#include "creature_tracker.h"
#include "debug.h"
#include "enums.h"
#include "faction.h"
#include "fault.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "handle_liquid.h"
#include "inventory.h"
#include "item.h"
#include "item_group.h"
#include "itype.h"
#include "line.h"
#include "localized_comparator.h"
#include "map.h"
#include "map_selector.h"
#include "mapdata.h"
#include "memory_fast.h"
#include "messages.h"
#include "monster.h"
#include "npc.h"
#include "output.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "point.h"
#include "proficiency.h"
#include "requirements.h"
#include "ret_val.h"
#include "rng.h"
#include "skill.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "tileray.h"
#include "translation.h"
#include "translations.h"
#include "uilist.h"
#include "ui_manager.h"
#include "units.h"
#include "units_utility.h"
#include "value_ptr.h"
#include "veh_shape.h"
#include "veh_type.h"
#include "veh_utils.h"
#include "vehicle.h"
#include "vehicle_selector.h"
#include "vpart_position.h"
#include "vpart_range.h"

static const activity_id ACT_VEHICLE( "ACT_VEHICLE" );

static const ammotype ammo_battery( "battery" );

static const faction_id faction_no_faction( "no_faction" );

static const itype_id fuel_type_battery( "battery" );
static const itype_id itype_battery( "battery" );
static const itype_id itype_plut_cell( "plut_cell" );

static const proficiency_id proficiency_prof_aircraft_mechanic( "prof_aircraft_mechanic" );

static const quality_id qual_HOSE( "HOSE" );
static const quality_id qual_JACK( "JACK" );
static const quality_id qual_LIFT( "LIFT" );
static const quality_id qual_SELF_JACK( "SELF_JACK" );

static const skill_id skill_mechanics( "mechanics" );

static const trait_id trait_BADBACK( "BADBACK" );
static const trait_id trait_DEBUG_HS( "DEBUG_HS" );
static const trait_id trait_STRONGBACK( "STRONGBACK" );

static const vpart_id vpart_ap_wall_wiring( "ap_wall_wiring" );

static std::string status_color( bool status )
{
    return status ? "<color_green>" : "<color_red>";
}
static std::string health_color( bool status )
{
    return status ? "<color_light_green>" : "<color_light_red>";
}

// Cap JACK requirements to support arbitrarily large vehicles.
static constexpr units::mass JACK_LIMIT = 8500_kilogram; // 8500kg ( 8.5 metric tonnes )

// cap JACK requirements to support arbitrarily large vehicles
static double jack_quality( map &here, const vehicle &veh )
{
    const units::quantity<double, units::mass::unit_type> mass = std::min( veh.total_mass( here ),
            JACK_LIMIT );
    return std::ceil( mass / lifting_quality_to_mass( 1 ) );
}

/** Can part currently be reloaded with anything? */
static auto can_refill = []( const map &, const vehicle_part &pt )
{
    return pt.can_reload( );
};

static void act_vehicle_unload_fuel( map &here, vehicle *veh );

player_activity veh_interact::serialize_activity( map &here )
{
    const vehicle_part *pt = sel_vehicle_part;
    const vpart_info *vp = sel_vpart_info;

    if( sel_cmd == 'p' ) {
        if( !parts_here.empty() ) {
            const vpart_reference part_here( *veh, parts_here[0] );
            const vpart_reference displayed_part( *veh, veh->part_displayed_at( part_here.mount_pos() ) );
            return veh_shape( here, *veh ).start( displayed_part.pos_bub( here ) );
        }
        return player_activity();
    }

    if( sel_cmd == 'q' || sel_cmd == ' ' || !vp ) {
        return player_activity();
    }

    avatar &player_character = get_avatar();
    time_duration time = 0_seconds;
    switch( sel_cmd ) {
        case 'i':
            time = vp->install_time( player_character );
            break;
        case 'r':
            if( pt != nullptr ) {
                if( pt->is_broken() ) {
                    time = vp->install_time( player_character );
                } else if( pt->is_repairable() ) {
                    time = vp->repair_time( player_character ) * pt->base.repairable_levels();
                }
            }
            break;
        case 'o':
            time = vp->removal_time( player_character );
            break;
        default:
            break;
    }
    if( player_character.has_trait( trait_DEBUG_HS ) ) {
        time = 1_seconds;
    }
    player_activity res( ACT_VEHICLE, to_moves<int>( time ), static_cast<int>( sel_cmd ) );

    // if we're working on an existing part, use that part as the reference point
    // otherwise (e.g. installing a new frame), just use part 0
    const point_rel_ms q = veh->coord_translate( pt ? pt->mount : veh->part( 0 ).mount );
    const vehicle_part *vpt = pt ? pt : &veh->part( 0 );
    for( const tripoint_abs_ms &p : veh->get_points( true ) ) {
        res.coord_set.insert( p );
    }
    res.values.push_back( veh->pos_abs().x() + q.x() );   // values[0]
    res.values.push_back( veh->pos_abs().y() + q.y() );   // values[1]
    res.values.push_back( dd.x() );   // values[2]
    res.values.push_back( dd.y() );   // values[3]
    res.values.push_back( -dd.x() );   // values[4]
    res.values.push_back( -dd.y() );   // values[5]
    res.values.push_back( veh->index_of_part( vpt ) ); // values[6]
    res.str_values.emplace_back( vp->id.str() );
    res.str_values.emplace_back( "" ); // previously stored the part variant, now obsolete
    res.targets.emplace_back( std::move( refill_target ) );

    return res;
}

void orient_part( map &here, vehicle *veh, const vpart_info &vpinfo, int partnum,
                  const std::optional<point_rel_ms> &part_placement )
{
    avatar &player_character = get_avatar();
    // Stash offset and set it to the location of the part so look_around will
    // start there.
    const tripoint_rel_ms old_view_offset = player_character.view_offset;
    tripoint_bub_ms offset = veh->pos_bub( here );
    // Appliances are one tile so the part placement there is always point::zero
    if( part_placement ) {
        point_rel_ms copied_placement = *part_placement;
        offset = offset + copied_placement;
    }
    player_character.view_offset = offset - player_character.pos_bub( here );

    point_rel_ms delta;
    do {
        popup( _( "Press space, choose a facing direction for the new %s and "
                  "confirm with enter." ),
               vpinfo.name() );

        const std::optional<tripoint_bub_ms> chosen = g->look_around();
        if( !chosen ) {
            continue;
        }
        delta = ( *chosen - offset ).xy();
        // atan2 only gives reasonable values when delta is not all zero
    } while( delta == point_rel_ms::zero );

    // Restore previous view offsets.
    player_character.view_offset = old_view_offset;

    units::angle dir = normalize( atan2( delta.raw() ) - veh->face.dir() );

    veh->part( partnum ).direction = dir;
}

player_activity veh_interact::run( map &here, vehicle &veh, const point_rel_ms &p )
{
    veh_interact vehint( here, veh, p );
    vehint.do_main_loop( here );
    return vehint.serialize_activity( here );
}

std::optional<vpart_reference> veh_interact::select_part( map &here, const vehicle &veh,
        const part_selector &sel, const std::string &title )
{
    std::optional<vpart_reference> res = std::nullopt;
    const auto act = [&]( const map &, const vehicle_part & pt ) {
        res = vpart_reference( const_cast<vehicle &>( veh ), veh.index_of_part( &pt ) );
    };
    std::function<bool( const vpart_reference & )> sel_wrapper = [sel,
    &here]( const vpart_reference & vpr ) {
        return sel( here, vpr.part() );
    };

    const vehicle_part_range vpr = veh.get_all_parts();
    int opts = std::count_if( vpr.begin(), vpr.end(), sel_wrapper );

    if( opts == 1 ) {
        act( here, std::find_if( vpr.begin(), vpr.end(), sel_wrapper )->part() );

    } else if( opts != 0 ) {
        veh_interact vehint( here, const_cast<vehicle &>( veh ) );
        vehint.title = title.empty() ? _( "Select part" ) : title;
        vehint.overview( here, sel, act );
    }

    return res;
}

/**
 * Creates a blank veh_interact window.
 */
veh_interact::veh_interact( map &here, vehicle &veh, const point_rel_ms &p )
    : dd( p ), veh( &veh ), main_context( "VEH_INTERACT", keyboard_mode::keycode )
{
    main_context.register_directions();
    main_context.register_action( "QUIT" );
    main_context.register_action( "INSTALL" );
    main_context.register_action( "REPAIR" );
    main_context.register_action( "MEND" );
    main_context.register_action( "REFILL" );
    main_context.register_action( "REMOVE" );
    main_context.register_action( "RENAME" );
    main_context.register_action( "SIPHON" );
    main_context.register_action( "UNLOAD" );
    main_context.register_action( "CHANGE_SHAPE" );
    main_context.register_action( "ASSIGN_CREW" );
    main_context.register_action( "RELABEL" );
    main_context.register_action( "PREV_TAB" );
    main_context.register_action( "NEXT_TAB" );
    main_context.register_action( "OVERVIEW_DOWN" );
    main_context.register_action( "OVERVIEW_UP" );
    main_context.register_action( "FUEL_LIST_DOWN" );
    main_context.register_action( "FUEL_LIST_UP" );
    main_context.register_action( "DESC_LIST_DOWN" );
    main_context.register_action( "DESC_LIST_UP" );
    main_context.register_action( "PAGE_DOWN" );
    main_context.register_action( "PAGE_UP" );
    main_context.register_action( "CONFIRM" );
    main_context.register_action( "HELP_KEYBINDINGS" );
    main_context.register_action( "FILTER" );
    main_context.register_action( "ANY_INPUT" );
    main_context.register_action( "ACCESSIBILITY_PART_LIST" );
    main_context.register_action( "ACCESSIBILITY_STATUS" );
    main_context.register_action( "ACCESSIBILITY_DETAILS" );
    main_context.register_action( "ACCESSIBILITY_REPEAT_LAST" );

    count_durability();
    cache_tool_availability();
    // Initialize info of selected parts
    move_cursor( here, point_rel_ms::zero );
}

veh_interact::~veh_interact() = default;

void veh_interact::allocate_windows()
{
    // grid window
    const point grid( point::south_east );
    const int grid_w = TERMX - 2; // exterior borders take 2
    const int grid_h = TERMY - 2; // exterior borders take 2

    const int mode_h  = 1;
    const int name_h  = 1;

    page_size = grid_h - ( mode_h + stats_h + name_h ) - 2;

    const int pane_y = grid.y + mode_h + 1;

    pane_w = ( grid_w / 3 ) - 1;

    disp_w = grid_w - ( pane_w * 2 ) - 2;
    const int disp_h = page_size * 0.45;
    const int parts_h = page_size - disp_h;
    const int parts_y = pane_y + disp_h;

    const int name_y = pane_y + page_size + 1;
    const int stats_y = name_y + name_h;

    const int list_x = grid.x + disp_w + 1;
    const int msg_x  = list_x + pane_w + 1;

    const int details_y = name_y;
    const int details_x = list_x;

    const int details_h = 7;
    const int details_w = grid.x + grid_w - details_x;

    // make the windows
    w_border = catacurses::newwin( TERMY, TERMX, point::zero );
    w_mode  = catacurses::newwin( mode_h,    grid_w, grid );
    w_msg   = catacurses::newwin( page_size, pane_w, point( msg_x, pane_y ) );
    w_disp  = catacurses::newwin( disp_h,    disp_w, point( grid.x, pane_y ) );
    w_parts = catacurses::newwin( parts_h,   disp_w, point( grid.x, parts_y ) );
    w_list  = catacurses::newwin( page_size, pane_w, point( list_x, pane_y ) );
    w_name  = catacurses::newwin( name_h,    grid_w, point( grid.x, name_y ) );
    w_details = catacurses::newwin( details_h, details_w, point( details_x, details_y ) );
    w_stats_1 = catacurses::newwin( stats_h, disp_w - 2, point( grid.x + 1, stats_y ) );
    w_stats_2 = catacurses::newwin( stats_h, pane_w - 2, point( grid.x + disp_w + 2, stats_y ) );
    w_stats_3 = catacurses::newwin( stats_h, pane_w - 2, point( grid.x + disp_w + pane_w + 3,
                                    stats_y ) );

}

bool veh_interact::format_reqs( std::string &msg, const requirement_data &reqs,
                                const std::map<skill_id, int> &skills, time_duration time,
                                task_speech *speech ) const
{
    Character &player_character = get_player_character();
    const inventory &inv = player_character.crafting_inventory();
    bool ok = reqs.can_make_with_inventory( inv, is_crafting_component, 1, craft_flags::none, false );

    msg += _( "<color_white>Time required:</color>\n" );
    msg += "> " + to_string_approx( time ) + "\n";

    msg += _( "<color_white>Skills required:</color>\n" );
    for( const auto &e : skills ) {
        bool hasSkill = player_character.get_knowledge_level( e.first ) >= e.second;
        if( !hasSkill ) {
            ok = false;
        }
        //~ %1$s represents the internal color name which shouldn't be translated, %2$s is skill name, and %3$i is skill level
        msg += string_format( _( "> %1$s%2$s %3$i</color>\n" ), status_color( hasSkill ),
                              e.first.obj().name(), e.second );
    }
    if( skills.empty() ) {
        //~ %1$s represents the internal color name which shouldn't be translated, %2$s is the word "NONE"
        msg += string_format( "> %1$s%2$s</color>", status_color( true ), _( "NONE" ) ) + "\n";
    }

    auto comps = reqs.get_folded_components_list( getmaxx( w_msg ) - 2, c_white, inv,
                 is_crafting_component );
    for( const std::string &line : comps ) {
        msg += line + "\n";
    }
    auto tools = reqs.get_folded_tools_list( getmaxx( w_msg ) - 2, c_white, inv );
    for( const std::string &line : tools ) {
        msg += line + "\n";
    }

    if( speech != nullptr ) {
        speech->requirements.push_back( accessibility::format_vehicle_task_time(
                                            to_string_approx( time ) ) );
        std::vector<accessibility::vehicle_skill_requirement> levels;
        for( const auto &e : skills ) {
            const bool met = player_character.get_knowledge_level( e.first ) >= e.second;
            levels.push_back( { e.first.obj().name(), e.second, met } );
            speech->missing += met ? 0 : 1;
        }
        speech->requirements.push_back( accessibility::format_vehicle_skills( levels ) );
        const std::vector<accessibility::requirement_group> groups =
            accessibility_gameplay::requirement_groups( reqs, inv, is_crafting_component, 1, true );
        for( const accessibility::requirement_group &group : groups ) {
            speech->requirements.push_back( accessibility::format_requirement_group( group ) );
        }
        speech->missing += accessibility::count_missing_requirement_groups( groups );
    }

    return ok;
}

struct veh_interact::install_info_t {
    int pos = 0;
    size_t tab = 0;
    std::vector<const vpart_info *> tab_vparts;
    std::vector<vpart_category> tab_list;
};

struct veh_interact::remove_info_t {
    int pos = 0;
    size_t tab = 0;
};

shared_ptr_fast<ui_adaptor> veh_interact::create_or_get_ui_adaptor( map &here )
{
    shared_ptr_fast<ui_adaptor> current_ui = ui.lock();
    if( !current_ui ) {
        ui = current_ui = make_shared_fast<ui_adaptor>();
        current_ui->on_screen_resize( [this]( ui_adaptor & current_ui ) {
            if( ui_hidden ) {
                current_ui.position( point::zero, point::zero );
                return;
            }
            allocate_windows();
            current_ui.position_from_window( catacurses::stdscr );
        } );
        current_ui->mark_resize();
        current_ui->on_redraw( [&here, this]( const ui_adaptor & ) {
            if( ui_hidden ) {
                return;
            }
            display_grid();
            display_name();
            display_stats( here );
            display_veh( here );

            werase( w_parts );
            veh->print_part_list( w_parts, 0, getmaxy( w_parts ) - 1, getmaxx( w_parts ), cpart,
                                  highlight_part,
                                  true, false );
            wnoutrefresh( w_parts );

            werase( w_msg );
            if( !msg.has_value() ) {
                veh->print_vparts_descs( w_msg, getmaxy( w_msg ), getmaxx( w_msg ), cpart, start_at, start_limit );
            } else {
                const int height = catacurses::getmaxy( w_msg );
                const int width = catacurses::getmaxx( w_msg ) - 2;

                // the following contraption is splitting buffer into separate lines for scrolling
                // since earlier code relies on msg already being folded
                std::vector<std::string> buffer;
                std::istringstream msg_stream( msg.value() );
                while( !msg_stream.eof() ) {
                    std::string line;
                    getline( msg_stream, line );
                    if( utf8_width( line ) <= width ) {
                        buffer.emplace_back( line );
                    } else {
                        std::vector<std::string> folded = foldstring( line, width );
                        std::copy( folded.begin(), folded.end(), std::back_inserter( buffer ) );
                    }
                }

                const int pages = static_cast<int>( buffer.size() / ( height - 2 ) );
                w_msg_scroll_offset = clamp( w_msg_scroll_offset, 0, pages );
                for( int line = 0; line < height; ++line ) {
                    const int idx = w_msg_scroll_offset * ( height - 1 ) + line;
                    if( static_cast<size_t>( idx ) >= buffer.size() ) {
                        break;
                    }
                    nc_color dummy = c_unset;
                    print_colored_text( w_msg, point( 1, line ), dummy, c_unset, buffer[idx] );
                }
            }
            wnoutrefresh( w_msg );

            if( install_info ) {
                display_list( install_info->pos, install_info->tab_vparts, 2 );
                display_details( sel_vpart_info );
            } else if( remove_info ) {
                display_details( sel_vpart_info );
                display_overview( here );
            } else {
                display_overview( here );
            }
            display_mode( here );
        } );
    }
    return current_ui;
}

void veh_interact::hide_ui( map &here, const bool hide )
{
    if( hide != ui_hidden ) {
        ui_hidden = hide;
        create_or_get_ui_adaptor( here )->mark_resize();
    }
}

void veh_interact::do_main_loop( map &here )
{
    bool finish = false;
    Character &player_character = get_player_character();
    owned_by_player = veh->handle_potential_theft( player_character, true );
    faction *owner_fac;
    if( veh->has_owner() ) {
        owner_fac = g->faction_manager_ptr->get( veh->get_owner() );
    } else {
        owner_fac = g->faction_manager_ptr->get( faction_no_faction );
    }

    shared_ptr_fast<ui_adaptor> current_ui = create_or_get_ui_adaptor( here );
    if( accessibility::is_active() ) {
        finish = accessible_part_list( here );
        if( !finish ) {
            speak_grid();
        }
    }

    while( !finish ) {
        calc_overview( here );
        ui_manager::redraw();
        const int description_scroll_lines = catacurses::getmaxy( w_parts ) - 4;
        const std::string action = main_context.handle_input();
        msg.reset();
        if( accessibility::is_active() && accessible_grid_action( here, action, finish ) ) {
            // Handled by the screen reader interface.
        } else if( const std::optional<tripoint_rel_ms> vec = main_context.get_direction_rel_ms(
                       action ) ) {
            move_cursor( here, vec->xy() );
        } else if( action == "QUIT" ) {
            finish = true;
        } else if( action == "INSTALL" ) {
            if( veh->handle_potential_theft( player_character ) ) {
                do_install( here );
            }
        } else if( action == "REPAIR" ) {
            if( veh->handle_potential_theft( player_character ) ) {
                do_repair( here );
            }
        } else if( action == "MEND" ) {
            if( veh->handle_potential_theft( player_character ) ) {
                do_mend( here );
            }
        } else if( action == "REFILL" ) {
            if( veh->handle_potential_theft( player_character ) ) {
                do_refill( here );
            }
        } else if( action == "REMOVE" ) {
            if( veh->handle_potential_theft( player_character ) ) {
                do_remove( here );
            }
        } else if( action == "RENAME" ) {
            if( owned_by_player ) {
                do_rename();
            } else {
                if( owner_fac ) {
                    popup( _( "You cannot rename this vehicle as it is owned by: %s." ), _( owner_fac->name ) );
                }
            }
        } else if( action == "SIPHON" ) {
            if( veh->handle_potential_theft( player_character ) ) {
                do_siphon( here );
                // Siphoning may have started a player activity. If so, we should close the
                // vehicle dialog and continue with the activity.
                finish = !player_character.activity.is_null();
                if( !finish ) {
                    // it's possible we just invalidated our crafting inventory
                    cache_tool_availability();
                }
            }
        } else if( action == "UNLOAD" ) {
            if( veh->handle_potential_theft( player_character ) ) {
                finish = do_unload( here );
            }
        } else if( action == "CHANGE_SHAPE" ) {
            sel_cmd = 'p';
        } else if( action == "ASSIGN_CREW" ) {
            if( owned_by_player ) {
                do_assign_crew( here );
            } else {
                if( owner_fac ) {
                    popup( _( "You cannot assign crew on this vehicle as it is owned by: %s." ),
                           _( owner_fac->name ) );
                }
            }
        } else if( action == "RELABEL" ) {
            if( owned_by_player ) {
                do_relabel( here );
            } else {
                if( owner_fac ) {
                    popup( _( "You cannot relabel this vehicle as it is owned by: %s." ), _( owner_fac->name ) );
                }
            }
        } else if( action == "FUEL_LIST_DOWN" ) {
            move_fuel_cursor( here, 1 );
        } else if( action == "FUEL_LIST_UP" ) {
            move_fuel_cursor( here, -1 );
        } else if( action == "OVERVIEW_DOWN" ) {
            move_overview_line( 1 );
        } else if( action == "OVERVIEW_UP" ) {
            move_overview_line( -1 );
        } else if( action == "DESC_LIST_DOWN" ) {
            move_cursor( here, point_rel_ms::zero, 1 );
        } else if( action == "DESC_LIST_UP" ) {
            move_cursor( here, point_rel_ms::zero, -1 );
        } else if( action == "PAGE_DOWN" ) {
            move_cursor( here, point_rel_ms::zero, description_scroll_lines );
        } else if( action == "PAGE_UP" ) {
            move_cursor( here, point_rel_ms::zero, -description_scroll_lines );
        }
        if( sel_cmd != ' ' ) {
            finish = true;
        }
        if( !finish && accessibility::is_active() ) {
            accessible_after_action( here, action );
        }
    }
}

void veh_interact::cache_tool_availability()
{
    map &here = get_map();

    Character &player_character = get_player_character();
    crafting_inv = &player_character.crafting_inventory();

    cache_tool_availability_update_lifting( player_character.pos_bub() );
    int mech_jack = 0;
    if( player_character.is_mounted() ) {
        mech_jack = player_character.mounted_creature->mech_str_addition() + 10;
    }
    int max_quality = std::max( { player_character.max_quality( qual_JACK ), mech_jack,
                                  map_selector( player_character.pos_bub(), PICKUP_RANGE ).max_quality( qual_JACK ),
                                  vehicle_selector( here, player_character.pos_bub(), 2, true, *veh ).max_quality( qual_JACK )
                                } );
    max_jack = lifting_quality_to_mass( max_quality );
}

void veh_interact::cache_tool_availability_update_lifting( const tripoint_bub_ms &world_cursor_pos )
{
    max_lift = get_player_character().best_nearby_lifting_assist( world_cursor_pos );
}

/**
 * Checks if the player is able to perform some command, and returns a nonzero
 * error code if they are unable to perform it. The return from this function
 * should be passed into the various do_whatever functions further down.
 * @param mode The command the player is trying to perform (i.e. 'r' for repair).
 * @return CAN_DO if the player has everything they need,
 *         INVALID_TARGET if the command can't target that square,
 *         LACK_TOOLS if the player lacks tools,
 *         NOT_FREE if something else obstructs the action,
 *         LACK_SKILL if the player's skill isn't high enough,
 *         LOW_MORALE if the player's morale is too low while trying to perform
 *             an action requiring a minimum morale,
 *         UNKNOWN_TASK if the requested operation is unrecognized.
 */
task_reason veh_interact::cant_do( const map &here,  char mode )
{
    bool enough_morale = true;
    bool valid_target = false;
    bool has_tools = false;
    bool part_free = true;
    bool has_skill = true;
    bool enough_light = true;
    const vehicle_part_range vpr = veh->get_all_parts();
    avatar &player_character = get_avatar();
    switch( mode ) {
        case 'i':
            // install mode
            enough_morale = player_character.has_morale_to_craft();
            valid_target = !can_mount.empty();
            //tool checks processed later
            enough_light = player_character.fine_detail_vision_mod() <= 4;
            has_tools = true;
            break;

        case 'r':
            // repair mode
            enough_morale = player_character.has_morale_to_craft();
            valid_target = !need_repair.empty() && cpart >= 0;
            // checked later
            has_tools = true;
            enough_light = player_character.fine_detail_vision_mod() <= 4;
            break;

        case 'm': {
            // mend mode
            enough_morale = player_character.has_morale_to_craft();
            const bool toggling = player_character.has_trait( trait_DEBUG_HS );
            valid_target = std::any_of( vpr.begin(), vpr.end(), [toggling]( const vpart_reference & pt ) {
                if( toggling ) {
                    return pt.part().is_available() && !pt.part().faults_potential().empty();
                } else {
                    return pt.part().is_available() && !pt.part().faults().empty();
                }
            } );
            enough_light = player_character.fine_detail_vision_mod() <= 4;
            // checked later
            has_tools = true;
        }
        break;

        case 'f':
            valid_target = std::any_of( vpr.begin(), vpr.end(), [&here]( const vpart_reference & pt ) {
                return can_refill( here, pt.part() );
            } );
            has_tools = true;
            break;

        case 'o':
            // remove mode
            enough_morale = player_character.has_morale_to_craft();
            valid_target = cpart >= 0;
            part_free = parts_here.size() > 1 ||
                        ( cpart >= 0 && veh->can_unmount( veh->part( cpart ) ).success() );
            //tool and skill checks processed later
            has_tools = true;
            has_skill = true;
            enough_light = player_character.fine_detail_vision_mod() <= 4;
            break;

        case 's':
            // siphon mode
            valid_target = false;
            for( const vpart_reference &vp : veh->get_any_parts( VPFLAG_FLUIDTANK ) ) {
                if( vp.part().base.has_item_with( []( const item & it ) {
                return it.made_of( phase_id::LIQUID );
                } ) ) {
                    valid_target = true;
                    break;
                }
            }
            has_tools = player_character.crafting_inventory( false ).has_quality( qual_HOSE );
            break;

        case 'd':
            // unload mode
            valid_target = false;
            has_tools = true;
            for( auto &e : veh->fuels_left( ) ) {
                if( e.first != fuel_type_battery && item::find_type( e.first )->phase == phase_id::SOLID ) {
                    valid_target = true;
                    break;
                }
            }
            break;

        case 'w':
            // assign crew
            if( g->allies().empty() ) {
                return task_reason::INVALID_TARGET;
            }
            return std::any_of( vpr.begin(), vpr.end(), []( const vpart_reference & e ) {
                return e.part().is_seat();
            } ) ? task_reason::CAN_DO : task_reason::INVALID_TARGET;

        case 'p':
        // change part shape
        // intentional fall-through
        case 'a':
            // relabel
            valid_target = cpart >= 0;
            has_tools = true;
            break;
        default:
            return task_reason::UNKNOWN_TASK;
    }

    if( std::abs( veh->velocity ) > 100 || player_character.controlling_vehicle ) {
        return task_reason::MOVING_VEHICLE;
    }
    if( !valid_target ) {
        return task_reason::INVALID_TARGET;
    }
    if( !enough_morale ) {
        return task_reason::LOW_MORALE;
    }
    if( !enough_light ) {
        return task_reason::LOW_LIGHT;
    }
    if( !has_tools ) {
        return task_reason::LACK_TOOLS;
    }
    if( !part_free ) {
        return task_reason::NOT_FREE;
    }
    // TODO: that is always false!
    if( !has_skill ) {
        return task_reason::LACK_SKILL;
    }
    return task_reason::CAN_DO;
}

bool veh_interact::can_self_jack( map &here )
{
    int lvl = jack_quality( here, *veh );

    for( const vpart_reference &vp : veh->get_avail_parts( "SELF_JACK" ) ) {
        if( vp.part().base.has_quality( qual_SELF_JACK, lvl ) ) {
            return true;
        }
    }
    return false;
}

bool veh_interact::update_part_requirements( map &here, task_speech *speech )
{
    if( sel_vpart_info == nullptr ) {
        return false;
    }
    // Shows why nothing can be installed regardless of tools and skills.
    const auto block = [&]( const std::string & reason ) {
        msg = reason;
        if( speech != nullptr ) {
            speech->blocked = remove_color_tags( reason );
        }
        return false;
    };

    if( std::any_of( parts_here.begin(), parts_here.end(), [&]( const int e ) {
    return veh->part( e ).has_flag( vp_flag::carried_flag );
    } ) ) {
        return block( _( "Unracking is required before installing any parts here." ) );
    }

    if( const std::optional<std::string> conflict = veh->has_engine_conflict( *sel_vpart_info ) ) {
        //~ %1$s is fuel_type
        return block( string_format( _( "Only one %1$s powered engine can be installed." ),
                                     conflict.value() ) );
    }
    if( veh->has_part( "NO_MODIFY_VEHICLE" ) && !sel_vpart_info->has_flag( "SIMPLE_PART" ) ) {
        return block( _( "This vehicle cannot be modified in this way.\n" ) );
    } else if( sel_vpart_info->has_flag( "NO_INSTALL_PLAYER" ) ) {
        return block( _( "This part cannot be installed.\n" ) );
    }

    if( sel_vpart_info->has_flag( "FUNNEL" ) ) {
        if( std::none_of( parts_here.begin(), parts_here.end(), [&]( const int e ) {
        return veh->part( e ).is_tank();
        } ) ) {
            return block( _( "Funnels need to be installed over a tank." ) );
        }
    }

    if( sel_vpart_info->has_flag( "TURRET" ) ) {
        if( std::any_of( parts_here.begin(), parts_here.end(), [&]( const int e ) {
        return veh->part( e ).is_turret();
        } ) ) {
            return block( _( "Can't install turret on another turret." ) );
        }
    }

    bool is_engine = sel_vpart_info->has_flag( "ENGINE" );
    //count current engines, some engines don't require higher skill
    int engines = 0;
    int dif_eng = 0;
    if( is_engine && sel_vpart_info->has_flag( "E_HIGHER_SKILL" ) ) {
        for( const vpart_reference &vp : veh->get_avail_parts( "ENGINE" ) ) {
            if( vp.has_feature( "E_HIGHER_SKILL" ) ) {
                engines++;
                dif_eng = dif_eng / 2 + 8;
            }
        }
    }

    int dif_steering = 0;
    if( sel_vpart_info->has_flag( "STEERABLE" ) ) {
        std::set<int> axles;
        for( const int p : veh->steering ) {
            const vehicle_part &vp = veh->part( p );
            if( !vp.info().has_flag( "TRACKED" ) ) {
                // tracked parts don't contribute to axle complexity
                axles.insert( vp.mount.x() );
            }
        }

        if( !axles.empty() && axles.count( -dd.x() ) == 0 ) {
            // Installing more than one steerable axle is hard
            // (but adding a wheel to an existing axle isn't)
            dif_steering = axles.size() + 5;
        }
    }

    const requirement_data reqs = sel_vpart_info->install_requirements();

    avatar &player_character = get_avatar();
    std::string nmsg;
    bool ok = format_reqs( nmsg, reqs, sel_vpart_info->install_skills,
                           sel_vpart_info->install_time( player_character ), speech );

    nmsg += _( "<color_white>Additional requirements:</color>\n" );

    bool allow_more_eng = engines < 2 || player_character.has_trait( trait_DEBUG_HS );
    // An additional skill level, spoken with its state instead of its color.
    const auto speak_extra_skill = [&]( const std::string & purpose, const int level ) {
        if( speech == nullptr ) {
            return;
        }
        const bool met = player_character.get_knowledge_level( skill_mechanics ) >= level;
        speech->requirements.push_back( accessibility::format_vehicle_extra_skill(
                                            skill_mechanics.obj().name(), level, met, purpose ) );
        speech->missing += met ? 0 : 1;
    };

    if( dif_eng > 0 ) {
        if( !allow_more_eng || player_character.get_knowledge_level( skill_mechanics ) < dif_eng ) {
            ok = false;
        }
        if( !allow_more_eng && speech != nullptr ) {
            speech->blocked = _( "You cannot install any more engines on this vehicle." );
        } else {
            speak_extra_skill( _( "for extra engines" ), dif_eng );
        }
        if( allow_more_eng ) {
            //~ %1$s represents the internal color name which shouldn't be translated, %2$s is skill name, and %3$i is skill level
            nmsg += string_format( _( "> %1$s%2$s %3$i</color> for extra engines." ),
                                   status_color( player_character.get_knowledge_level( skill_mechanics ) >= dif_eng ),
                                   skill_mechanics.obj().name(), dif_eng ) + "\n";
        } else {
            nmsg += _( "> <color_red>You cannot install any more engines on this vehicle.</color>" ) +
                    std::string( "\n" );
        }
    }

    if( dif_steering > 0 ) {
        if( player_character.get_knowledge_level( skill_mechanics ) < dif_steering ) {
            ok = false;
        }
        speak_extra_skill( _( "for extra steering axles" ), dif_steering );
        //~ %1$s represents the internal color name which shouldn't be translated, %2$s is skill name, and %3$i is skill level
        nmsg += string_format( _( "> %1$s%2$s %3$i</color> for extra steering axles." ),
                               status_color( player_character.get_knowledge_level( skill_mechanics ) >= dif_steering ),
                               skill_mechanics.obj().name(), dif_steering ) + "\n";
    }

    std::pair<bool, std::string> res = calc_lift_requirements( here, *sel_vpart_info, speech );
    if( !res.first ) {
        ok = res.first;
    }
    nmsg += res.second;

    const ret_val<void> can_mount = veh->can_mount( -dd, *sel_vpart_info );
    if( !can_mount.success() ) {
        ok = false;
        nmsg += _( "<color_white>Cannot install due to:</color>\n> " ) +
                colorize( can_mount.str(), c_red ) + "\n";
        if( speech != nullptr ) {
            speech->blocked = remove_color_tags( can_mount.str() );
        }
    }

    sel_vpart_info->format_description( nmsg, c_light_gray, getmaxx( w_msg ) - 4 );

    msg = colorize( nmsg, c_light_gray );
    return ok || player_character.has_trait( trait_DEBUG_HS );
}

/**
 * Moves list of fuels up or down.
 * @param delta -1 if moving up,
 *              1 if moving down
 */
void veh_interact::move_fuel_cursor( map &here, int delta )
{
    int max_fuel_indicators = static_cast<int>( veh->get_printable_fuel_types( here ).size() );
    int height = 5;
    fuel_index += delta;

    if( fuel_index < 0 ) {
        fuel_index = 0;
    } else if( fuel_index > max_fuel_indicators - height ) {
        fuel_index = std::max( max_fuel_indicators - height, 0 );
    }
}

static void sort_uilist_entries_by_line_drawing( std::vector<uilist_entry> &shape_ui_entries )
{
    // An ordering of the line drawing symbols that does not result in
    // connecting when placed adjacent to each other vertically.
    const static std::map<int, int> symbol_order = {
        { LINE_XOXO, 0 }, { LINE_OXOX, 1 },
        { LINE_XOOX, 2 }, { LINE_XXOO, 3 },
        { LINE_XXXX, 4 }, { LINE_OXXO, 5 },
        { LINE_OOXX, 6 }
    };

    std::sort( shape_ui_entries.begin(), shape_ui_entries.end(),
    []( const uilist_entry & a, const uilist_entry & b ) {
        auto a_iter = symbol_order.find( a.extratxt.sym );
        auto b_iter = symbol_order.find( b.extratxt.sym );
        if( a_iter != symbol_order.end() ) {
            if( b_iter != symbol_order.end() ) {
                return a_iter->second < b_iter->second;
            } else {
                return true;
            }
        } else if( b_iter != symbol_order.end() ) {
            return false;
        } else {
            return a.extratxt.sym < b.extratxt.sym;
        }
    } );
}

void veh_interact::do_install( map &here )
{
    task_reason reason = cant_do( here,  'i' );

    if( reason == task_reason::INVALID_TARGET ) {
        msg = _( "Cannot install any part here." );
        return;
    }
    if( accessibility::is_active() ) {
        accessible_install( here );
        return;
    }

    restore_on_out_of_scope prev_title( title );
    title = _( "Choose new part to install here:" );

    restore_on_out_of_scope prev_install_info( std::move(
                install_info ) );
    install_info = std::make_unique<install_info_t>();

    std::string filter; // The user specified filter
    std::vector<vpart_category> &tab_list = install_info->tab_list = {};
    std::vector <std::function<bool( const vpart_info * )>> tab_filters;

    for( const vpart_category &cat : vpart_category::all() ) {
        tab_list.push_back( cat );
        if( cat.get_id() == "_all" ) {
            tab_filters.emplace_back( []( const vpart_info * ) {
                return true;
            } );
        } else if( cat.get_id() == "_filter" ) {
            tab_filters.emplace_back( [&filter]( const vpart_info * p ) {
                return lcmatch( p->name(), filter );
            } );
        } else {
            tab_filters.emplace_back( [ cat = cat.get_id()]( const vpart_info * p ) {
                return p->has_category( cat );
            } );
        }
    }

    shared_ptr_fast<ui_adaptor> current_ui = create_or_get_ui_adaptor( here );

    int &pos = install_info->pos = 0;
    size_t &tab = install_info->tab = 0;
    avatar &player_character = get_avatar();

    std::vector<const vpart_info *> &tab_vparts = install_info->tab_vparts;

    while( true ) {
        tab = ( tab + tab_list.size() ) % tab_list.size(); // handle tabs rolling under/over
        tab_vparts.clear();
        std::copy_if( can_mount.begin(), can_mount.end(), std::back_inserter( tab_vparts ),
                      tab_filters[tab] );
        // tab_vparts can be empty or pos out of bounds
        sel_vpart_info = pos >= 0 && pos < static_cast<int>( tab_vparts.size() )
                         ? tab_vparts[pos] : nullptr;

        const bool can_install = update_part_requirements( here );
        ui_manager::redraw();

        const std::string action = main_context.handle_input();
        msg.reset();
        if( action == "FILTER" ) {
            string_input_popup()
            .title( _( "Search for part" ) )
            .width( 50 )
            .description( _( "Filter" ) )
            .max_length( 100 )
            .edit( filter );
            tab = tab_filters.size() - 1; // Move to the user filter tab.
            pos = 0;
        } else if( action == "REPAIR" ) {
            filter.clear();
            tab = 0;
            pos = 0;
        } else if( action == "INSTALL" || action == "CONFIRM" ) {
            if( !can_install || sel_vpart_info == nullptr ) {
                continue;
            }
            switch( reason ) {
                case task_reason::LOW_MORALE:
                    msg = _( "Your morale is too low to construct…" );
                    return;
                case task_reason::LOW_LIGHT:
                    msg = _( "It's too dark to see what you are doing…" );
                    return;
                case task_reason::MOVING_VEHICLE:
                    msg = _( "You can't install parts while driving." );
                    return;
                default:
                    break;
            }
            // Modifying a vehicle with rotors will make in not flightworthy
            // (until we've got a better model)
            // It can only be the player doing this - an npc won't work well with query_yn
            if( veh->would_install_prevent_flyable( *sel_vpart_info, player_character ) ) {
                if( query_yn(
                        _( "Installing this part will mean that this vehicle is no longer "
                           "flightworthy.  Continue?" ) ) ) {
                    veh->set_flyable( false );
                } else {
                    return;
                }
            }
            if( veh->is_foldable() && !sel_vpart_info->folded_volume &&
                !query_yn( _( "Installing this part will make the vehicle unfoldable. "
                              " Continue?" ) ) ) {
                return;
            }

            sel_cmd = 'i';
            break;
        } else if( action == "QUIT" ) {
            sel_vpart_info = nullptr;
            break;
        } else if( action == "PREV_TAB" ) {
            pos = 0;
            tab--;
        } else if( action == "NEXT_TAB" ) {
            pos = 0;
            tab++;
        } else if( action == "DESC_LIST_DOWN" ) {
            w_msg_scroll_offset++;
        } else if( action == "DESC_LIST_UP" ) {
            w_msg_scroll_offset--;
        } else {
            move_in_list( pos, action, tab_vparts.size(), 2 );
            pos = std::max( pos, 0 ); // move_in_list sets pos to -1 when moving up in empty list
        }
    }
}

bool veh_interact::move_in_list( int &pos, const std::string &action, const int size,
                                 const int header ) const
{
    int lines_per_page = page_size - header;
    if( action == "PREV_TAB" || action == "LEFT" || action == "PAGE_UP" ) {
        pos -= lines_per_page;
    } else if( action == "NEXT_TAB" || action == "RIGHT" || action == "PAGE_DOWN" ) {
        pos += lines_per_page;
    } else if( action == "UP" ) {
        pos--;
    } else if( action == "DOWN" ) {
        pos++;
    } else {
        // Anything else -> no movement
        return false;
    }
    if( pos < 0 ) {
        pos = size - 1;
    } else if( pos >= size ) {
        pos = 0;
    }
    return true;
}

bool veh_interact::repair_requirements( map &here, vehicle_part &pt, std::string &nmsg,
                                        task_speech *speech )
{
    avatar &player_character = get_avatar();
    const vpart_info &vp = pt.info();
    const auto block = [&]( const std::string & reason, const nc_color color ) {
        nmsg += colorize( reason, color );
        if( speech != nullptr ) {
            speech->blocked = remove_color_tags( reason );
        }
        return false;
    };
    bool ok = true;
    if( pt.is_broken() ) {
        ok = format_reqs( nmsg, vp.install_requirements(), vp.install_skills,
                          vp.install_time( player_character ), speech );

        if( pt.info().has_flag( "NEEDS_JACKING" ) ) {

            nmsg += _( "<color_white>Additional requirements:</color>\n" );
            std::pair<bool, std::string> res = calc_lift_requirements( here, pt.info(), speech );
            if( !res.first ) {
                ok = false;
            }
            nmsg += res.second;
        }
        if( pt.has_flag( vp_flag::carried_flag ) ) {
            ok = block( _( "\nUnracking is required before replacing this part.\n" ), c_red );
        }

    } else {
        if( !pt.is_repairable() ) {
            ok = block( _( "This part cannot be repaired.\n" ), c_light_red );
        } else if( veh->has_part( "NO_MODIFY_VEHICLE" ) && !vp.has_flag( "SIMPLE_PART" ) ) {
            ok = block( _( "This vehicle cannot be repaired.\n" ), c_light_red );
        } else {
            const int levels = pt.base.repairable_levels();
            ok = format_reqs( nmsg, vp.repair_requirements() * levels, vp.repair_skills,
                              vp.repair_time( player_character ) * levels, speech );
        }
    }

    if( veh->would_repair_prevent_flyable( pt, player_character ) &&
        !player_character.has_proficiency( proficiency_prof_aircraft_mechanic ) ) {
        const std::string warning = string_format(
                                        _( "\n<color_yellow>You require the \"%s\" proficiency to repair this part safely!</color>\n\n" ),
                                        proficiency_prof_aircraft_mechanic->name() );
        nmsg += warning;
        if( speech != nullptr ) {
            speech->requirements.push_back( accessibility::format_semantic_sequence( {
                remove_color_tags( warning )
            } ) );
        }
    }
    return ok;
}

void veh_interact::do_repair( map &here )
{
    task_reason reason = cant_do( here,  'r' );

    if( reason == task_reason::INVALID_TARGET ) {
        vehicle_part *most_repairable = get_most_repairable_part();
        if( most_repairable && most_repairable->is_repairable() ) {
            move_cursor( here, ( most_repairable->mount.raw() + dd ).rotate( 3 ) );
            return;
        }
    }

    auto can_repair = [this, &reason]() {
        switch( reason ) {
            case task_reason::LOW_MORALE:
                msg = _( "Your morale is too low to repair…" );
                return false;
            case task_reason::LOW_LIGHT:
                msg = _( "It's too dark to see what you are doing…" );
                return false;
            case task_reason::MOVING_VEHICLE:
                msg = _( "You can't repair stuff while driving." );
                return false;
            case task_reason::INVALID_TARGET:
                msg = _( "There are no damaged parts on this vehicle." );
                return false;
            default:
                break;
        }
        return true;
    };

    if( !can_repair() ) {
        return;
    }
    if( accessibility::is_active() ) {
        std::vector<int> parts;
        parts.reserve( need_repair.size() );
        for( const int index : need_repair ) {
            parts.push_back( parts_here[index] );
        }
        accessible_choose_part( here, access_task::repair, parts );
        return;
    }

    restore_on_out_of_scope prev_title( title );
    title = _( "Choose a part here to repair:" );

    shared_ptr_fast<ui_adaptor> current_ui = create_or_get_ui_adaptor( here );

    int pos = 0;

    restore_on_out_of_scope prev_hilight_part( highlight_part );

    avatar &player_character = get_avatar();
    while( true ) {
        vehicle_part &pt = veh->part( parts_here[need_repair[pos]] );
        const vpart_info &vp = pt.info();

        std::string nmsg;
        bool ok = repair_requirements( here, pt, nmsg );
        bool would_prevent_flying = veh->would_repair_prevent_flyable( pt, player_character );

        const nc_color desc_color = pt.is_broken() ? c_dark_gray : c_light_gray;
        vp.format_description( nmsg, desc_color, getmaxx( w_msg ) - 4 );

        msg = colorize( nmsg, c_light_gray );

        highlight_part = need_repair[pos];

        ui_manager::redraw();

        const std::string action = main_context.handle_input();
        msg.reset();
        if( ( action == "REPAIR" || action == "CONFIRM" ) && ok ) {
            // Modifying a vehicle with rotors will make in not flightworthy (until we've got a better model)
            if( would_prevent_flying ) {
                // It can only be the player doing this - an npc won't work well with query_yn
                if( query_yn(
                        _( "Repairing this part will mean that this vehicle is no longer flightworthy.  Continue?" ) ) ) {
                    veh->set_flyable( false );
                } else {
                    nmsg += colorize( _( "You chose not to install this part to keep the vehicle flyable.\n" ),
                                      c_light_red );
                    ok = false;
                }
            }
            if( ok ) {
                reason = cant_do( here,  'r' );
                if( !can_repair() ) {
                    return;
                }
                sel_vehicle_part = &pt;
                sel_vpart_info = &vp;
                for( const Character *helper : player_character.get_crafting_helpers() ) {
                    add_msg( m_info, _( "%s helps with this task…" ), helper->get_name() );
                }
                sel_cmd = 'r';
                break;
            }
        } else if( action == "QUIT" ) {
            break;
        } else {
            move_in_list( pos, action, need_repair.size() );
        }
    }
}

void veh_interact::do_mend( map &here )
{
    switch( cant_do( here,  'm' ) ) {
        case task_reason::LOW_MORALE:
            msg = _( "Your morale is too low to mend…" );
            return;
        case task_reason::LOW_LIGHT:
            msg = _( "It's too dark to see what you are doing…" );
            return;
        case task_reason::INVALID_TARGET:
            msg = _( "No faulty parts require mending." );
            return;
        case task_reason::MOVING_VEHICLE:
            msg = _( "You can't mend stuff while driving." );
            return;
        default:
            break;
    }

    restore_on_out_of_scope prev_title( title );
    title = _( "Choose a part here to mend:" );

    avatar &player_character = get_avatar();
    const bool toggling = player_character.has_trait( trait_DEBUG_HS );
    auto sel = [toggling]( const map &, const vehicle_part & pt ) {
        if( toggling ) {
            return !pt.faults_potential().empty();
        } else {
            return !pt.faults().empty();
        }
    };

    auto act = [&]( const map &, const vehicle_part & pt ) {
        player_character.mend_item( veh->part_base( veh->index_of_part( &pt ) ) );
        sel_cmd = 'q';
    };

    overview( here, sel, act );
}

void veh_interact::do_refill( map &here )
{
    switch( cant_do( here, 'f' ) ) {
        case task_reason::MOVING_VEHICLE:
            msg = _( "You can't refill a moving vehicle." );
            return;

        case task_reason::INVALID_TARGET:
            msg = _( "No parts can currently be refilled." );
            return;

        default:
            break;
    }

    restore_on_out_of_scope prev_title( title );
    title = _( "Select part to refill:" );

    auto act = [&]( const map &, const vehicle_part & pt ) {
        auto validate = [&]( const item & obj ) {
            if( pt.is_tank() ) {
                if( obj.is_watertight_container() && obj.num_item_stacks() == 1 ) {
                    // we are assuming only one pocket here, and it's a liquid so only one item
                    return pt.can_reload( obj.only_item() );
                }
            } else if( pt.is_fuel_store() ) {
                bool can_reload = pt.can_reload( obj );
                //check base item for fuel_stores that can take multiple types of ammunition (like the fuel_bunker)
                if( pt.get_base().can_reload_with( obj, true ) ) {
                    return true;
                }
                return can_reload;
            }
            return false;
        };

        refill_target = g->inv_map_splice( validate, string_format( _( "Refill %s" ), pt.name() ), 1 );
        if( refill_target ) {
            sel_vehicle_part = &pt;
            sel_vpart_info = &pt.info();
            sel_cmd = 'f';
        }
    };

    overview( here, can_refill, act );
}

void veh_interact::calc_overview( map &here )
{
    const hotkey_queue &hotkeys = hotkey_queue::alphabets();

    const auto next_hotkey = [&]( input_event & evt ) {
        input_event prev = evt;
        evt = main_context.next_unassigned_hotkey( hotkeys, evt );
        return prev;
    };
    auto is_selectable = [&]( const vehicle_part & pt ) {
        return overview_action && overview_enable && overview_enable( here,  pt );
    };

    overview_opts.clear();
    overview_headers.clear();

    units::power epower = veh->net_battery_charge_rate( here, /* include_reactors = */ true );
    overview_headers["1_ENGINE"] = [this, &here]( const catacurses::window & w, int y ) {
        trim_and_print( w, point( 1, y ), getmaxx( w ) - 2, c_light_gray,
                        string_format( _( "Engines: %sSafe %4d kW</color> %sMax %4d kW</color>" ),
                                       health_color( true ), units::to_kilowatt( veh->total_power( here, true, true ) ),
                                       health_color( false ), units::to_kilowatt( veh->total_power( here ) ) ) );
        right_print( w, y, 1, c_light_gray, _( "Fuel     Use" ) );
    };
    overview_headers["2_TANK"] = []( const catacurses::window & w, int y ) {
        trim_and_print( w, point( 1, y ), getmaxx( w ) - 2, c_light_gray, _( "Tanks" ) );
        right_print( w, y, 1, c_light_gray, _( "Contents     Qty" ) );
    };
    overview_headers["3_BATTERY"] = [epower]( const catacurses::window & w, int y ) {
        std::string batt;
        if( epower < 10_kW || epower > 10_kW ) {
            batt = string_format( _( "Batteries: %s%+4d W</color>" ),
                                  health_color( epower >= 0_W ), units::to_watt( epower ) );
        } else {
            batt = string_format( _( "Batteries: %s%+4.1f kW</color>" ),
                                  health_color( epower >= 0_W ), units::to_watt( epower ) / 1000.0 );
        }
        trim_and_print( w, point( 1, y ), getmaxx( w ) - 2, c_light_gray, batt );
        right_print( w, y, 1, c_light_gray, _( "Capacity  Status" ) );
    };
    overview_headers["4_REACTOR"] = [this]( const catacurses::window & w, int y ) {
        units::power reactor_epower = veh->max_reactor_epower();
        std::string reactor;
        if( reactor_epower == 0_W ) {
            reactor = _( "Reactors" );
        } else if( reactor_epower < 10_kW ) {
            reactor = string_format( _( "Reactors: Up to %s%+4d W</color>" ),
                                     health_color( reactor_epower > 0_W ), units::to_watt( reactor_epower ) );
        } else {
            reactor = string_format( _( "Reactors: Up to %s%+4.1f kW</color>" ),
                                     health_color( reactor_epower > 0_W ), units::to_watt( reactor_epower ) / 1000.0 );
        }
        trim_and_print( w, point( 1, y ), getmaxx( w ) - 2, c_light_gray, reactor );
        right_print( w, y, 1, c_light_gray, _( "Contents     Qty" ) );
    };
    overview_headers["5_TURRET"] = []( const catacurses::window & w, int y ) {
        trim_and_print( w, point( 1, y ), getmaxx( w ) - 2, c_light_gray, _( "Turrets" ) );
        right_print( w, y, 1, c_light_gray, _( "Ammo     Qty" ) );
    };
    overview_headers["6_SEAT"] = []( const catacurses::window & w, int y ) {
        trim_and_print( w, point( 1, y ), getmaxx( w ) - 2, c_light_gray, _( "Seats" ) );
        right_print( w, y, 1, c_light_gray, _( "Who" ) );
    };

    input_event hotkey = main_context.first_unassigned_hotkey( hotkeys );
    bool selectable;

    for( const vpart_reference &vpr : veh->get_all_parts() ) {
        if( !vpr.part().is_available() ) {
            continue;
        }

        if( vpr.part().is_engine() ) {
            // if tank contains something then display the contents in milliliters
            auto details = []( const vehicle_part & pt, const catacurses::window & w, int y ) {
                right_print(
                    w, y, 1, item::find_type( pt.ammo_current() )->color,
                    string_format(
                        "%s     <color_light_gray>%s</color>",
                        !pt.fuel_current().is_null() ? item::nname( pt.fuel_current() ) : "",
                        //~ translation should not exceed 3 console cells
                        right_justify( pt.enabled ? _( "Yes" ) : _( "No" ), 3 ) ) );
            };

            // display engine faults (if any)
            auto msg_cb = [&]( const vehicle_part & pt ) {
                msg = std::string();
                for( const auto &e : pt.faults() ) {
                    msg = msg.value() + string_format( "%s\n  %s\n\n", colorize( e->name(), c_red ),
                                                       colorize( e->description(), c_light_gray ) );
                }
            };
            selectable = is_selectable( vpr.part() );
            overview_opts.emplace_back( "1_ENGINE", &vpr.part(), selectable,
                                        selectable ? next_hotkey( hotkey ) : input_event(),
                                        details,
                                        msg_cb );
        }

        if( vpr.part().is_tank() || ( vpr.part().is_fuel_store() &&
                                      !( vpr.part().is_turret() || vpr.part().is_battery() || vpr.part().is_reactor() ) ) ) {
            auto tank_details = []( const vehicle_part & pt, const catacurses::window & w, int y ) {
                if( !pt.ammo_current().is_null() ) {
                    std::string specials;
                    // vehicle parts can only have one pocket, and we are showing a liquid,
                    // which can only be one.
                    const item &it = pt.base.legacy_front();
                    // a space isn't actually needed in front of the tags here,
                    // but item::display_name tags use a space so this prevents
                    // needing *second* translation for the same thing with a
                    // space in front of it
                    if( it.has_own_flag( flag_FROZEN ) ) {
                        specials += _( " (frozen)" );
                    } else if( it.rotten() ) {
                        specials += _( " (rotten)" );
                    }
                    const itype *pt_ammo_cur = item::find_type( pt.ammo_current() );
                    int offset = 1;
                    std::string fmtstring = "%s %s  %5.1fL";
                    if( pt.is_leaking() ) {
                        fmtstring = str_cat( "%s %s ", leak_marker, "%5.1fL", leak_marker );
                        offset = 0;
                    }
                    right_print( w, y, offset, pt_ammo_cur->color,
                                 string_format( fmtstring, specials, pt_ammo_cur->nname( 1 ),
                                                round_up( units::to_liter( it.volume() ), 1 ) ) );
                } else {
                    if( pt.is_leaking() ) {
                        std::string outputstr = str_cat( leak_marker, "      ", leak_marker );
                        right_print( w, y, 0, c_light_gray, outputstr );
                    }
                }
            };
            auto no_tank_details = []( const vehicle_part & pt, const catacurses::window & w, int y ) {
                if( !pt.ammo_current().is_null() ) {
                    const itype *pt_ammo_cur = item::find_type( pt.ammo_current() );
                    double vol_L = to_liter( pt.ammo_remaining( ) * 250_ml /
                                             pt_ammo_cur->stack_size );
                    int offset = 1;
                    std::string fmtstring = "%s  %5.1fL";
                    if( pt.is_leaking() ) {
                        fmtstring = str_cat( "%s  ", leak_marker, "%5.1fL", leak_marker );
                        offset = 0;
                    }
                    right_print( w, y, offset, pt_ammo_cur->color,
                                 string_format( fmtstring, item::nname( pt.ammo_current() ),
                                                round_up( vol_L, 1 ) ) );
                }
            };

            selectable = is_selectable( vpr.part() );
            if( vpr.part().is_tank() ) {
                overview_opts.emplace_back( "2_TANK", &vpr.part(), selectable, selectable ? next_hotkey(
                                                hotkey ) : input_event(),
                                            tank_details );
            } else if( vpr.part().is_fuel_store() && !( vpr.part().is_turret() ||
                       vpr.part().is_battery() || vpr.part().is_reactor() ) ) {
                overview_opts.emplace_back( "2_TANK", &vpr.part(), selectable, selectable ? next_hotkey(
                                                hotkey ) : input_event(),
                                            no_tank_details );
            }
        }

        if( vpr.part().is_battery() ) {
            // always display total battery capacity and percentage charge
            auto details = []( const vehicle_part & pt, const catacurses::window & w, int y ) {
                int pct = ( static_cast<double>( pt.ammo_remaining( ) ) / pt.ammo_capacity(
                                ammo_battery ) ) * 100;
                int offset = 1;
                std::string fmtstring = "%i    %3i%%";
                if( pt.is_leaking() ) {
                    fmtstring = str_cat( "%i   ", leak_marker, "%3i%%", leak_marker );
                    offset = 0;
                }
                right_print( w, y, offset, item::find_type( pt.ammo_current() )->color,
                             string_format( fmtstring, pt.ammo_capacity( ammo_battery ), pct ) );
            };
            selectable = is_selectable( vpr.part() );
            overview_opts.emplace_back( "3_BATTERY", &vpr.part(), selectable,
                                        selectable ? next_hotkey( hotkey ) : input_event(), details );
        }

        if( vpr.part().is_reactor() || vpr.part().is_turret() ) {
            auto details_ammo = []( const vehicle_part & pt, const catacurses::window & w, int y ) {
                if( pt.ammo_remaining( ) ) {
                    int offset = 1;
                    std::string fmtstring = "%s   %5i";
                    if( pt.is_leaking() ) {
                        fmtstring = str_cat( "%s  ", leak_marker, "%5i", leak_marker );
                        offset = 0;
                    }
                    right_print( w, y, offset, item::find_type( pt.ammo_current() )->color,
                                 string_format( fmtstring, item::nname( pt.ammo_current() ), pt.ammo_remaining( ) ) );
                }
            };
            selectable = is_selectable( vpr.part() );
            if( vpr.part().is_reactor() ) {
                overview_opts.emplace_back( "4_REACTOR", &vpr.part(), selectable,
                                            selectable ? next_hotkey( hotkey ) : input_event(),
                                            details_ammo );
            }
            if( vpr.part().is_turret() ) {
                overview_opts.emplace_back( "5_TURRET", &vpr.part(), selectable,
                                            selectable ? next_hotkey( hotkey ) : input_event(),
                                            details_ammo );
            }
        }

        if( vpr.part().is_seat() ) {
            auto details = []( const vehicle_part & pt, const catacurses::window & w, int y ) {
                const npc *who = pt.crew();
                if( who ) {
                    right_print( w, y, 1, pt.passenger_id == who->getID() ? c_green : c_light_gray, who->get_name() );
                }
            };
            selectable = is_selectable( vpr.part() );
            overview_opts.emplace_back( "6_SEAT", &vpr.part(), selectable, selectable ? next_hotkey(
                                            hotkey ) : input_event(), details );
        }
    }

    auto compare = []( veh_interact::part_option & s1,
    veh_interact::part_option & s2 ) {
        // NOLINTNEXTLINE cata-use-localized-sorting
        return  s1.key <  s2.key;
    };
    std::sort( overview_opts.begin(), overview_opts.end(), compare );

}

void veh_interact::display_overview( const map &here )
{
    werase( w_list );
    std::string last;
    int y = 0;
    if( overview_offset ) {
        trim_and_print( w_list, point( 1, y ), getmaxx( w_list ) - 1,
                        c_yellow, _( "'{' to scroll up" ) );
        y++;
    }
    for( int idx = overview_offset; idx != static_cast<int>( overview_opts.size() ); ++idx ) {
        const vehicle_part &pt = *overview_opts[idx].part;

        // if this is a new section print a header row
        if( last != overview_opts[idx].key ) {
            y += last.empty() ? 0 : 1;
            overview_headers[overview_opts[idx].key]( w_list, y );
            y += 2;
            last = overview_opts[idx].key;
        }

        bool highlighted = false;
        // No action means no selecting, just highlight relevant ones
        if( overview_pos < 0 && overview_enable && !overview_action ) {
            highlighted = overview_enable( here, pt );
        } else if( overview_pos == idx ) {
            highlighted = true;
        }

        // print part name
        nc_color col = overview_opts[idx].selectable ? c_white : c_dark_gray;
        trim_and_print( w_list, point( 1, y ), getmaxx( w_list ) - 1,
                        highlighted ? hilite( col ) : col,
                        "<color_dark_gray>%s </color>%s",
                        right_justify( overview_opts[idx].hotkey.short_description(), 2 ), pt.name() );

        // print extra columns (if any)
        overview_opts[idx].details( pt, w_list, y );
        y++;
        if( y < ( getmaxy( w_list ) - 1 ) ) {
            overview_limit = overview_offset;
        } else {
            overview_limit = idx;
            trim_and_print( w_list, point( 1, y ), getmaxx( w_list ) - 1,
                            c_yellow, _( "'}' to scroll down" ) );
            break;
        }
    }

    wnoutrefresh( w_list );
}

void veh_interact::overview( map &here,
                             const overview_enable_t &enable,
                             const overview_action_t &action )
{
    restore_on_out_of_scope prev_overview_enable( overview_enable );
    restore_on_out_of_scope prev_overview_action( overview_action );
    overview_enable = enable;
    overview_action = action;
    if( accessibility::is_active() && action ) {
        accessible_overview( here );
        return;
    }

    restore_on_out_of_scope prev_overview_pos( overview_pos );

    shared_ptr_fast<ui_adaptor> current_ui = create_or_get_ui_adaptor( here );

    while( true ) {
        calc_overview( here );

        if( overview_pos < 0 || static_cast<size_t>( overview_pos ) >= overview_opts.size() ) {
            overview_pos = -1;
            do {
                if( ++overview_pos >= static_cast<int>( overview_opts.size() ) ) {
                    overview_pos = -1;
                    break; // nothing could be selected
                }
            } while( !overview_opts[overview_pos].selectable );
        }

        const bool has_any_selectable_part = std::any_of( overview_opts.begin(), overview_opts.end(),
        []( const part_option & e ) {
            return e.selectable;
        } );
        if( !has_any_selectable_part ) {
            return; // nothing is selectable
        }

        if( overview_pos >= 0 && static_cast<size_t>( overview_pos ) < overview_opts.size() ) {
            move_cursor( here, ( overview_opts[overview_pos].part->mount.raw() + dd ).rotate( 3 ) );
        }

        if( overview_pos >= 0 && static_cast<size_t>( overview_pos ) < overview_opts.size() &&
            overview_opts[overview_pos].message ) {
            overview_opts[overview_pos].message( *overview_opts[overview_pos].part );
        } else {
            msg.reset();
        }

        ui_manager::redraw();

        const std::string input = main_context.handle_input();
        msg.reset();
        if( input == "CONFIRM" && overview_opts[overview_pos].selectable && overview_action ) {
            overview_action( here, *overview_opts[overview_pos].part );
            break;

        } else if( input == "QUIT" ) {
            break;

        } else if( input == "UP" ) {
            do {
                move_overview_line( -1 );
                if( --overview_pos < 0 ) {
                    overview_pos = overview_opts.size() - 1;
                }
            } while( !overview_opts[overview_pos].selectable );
        } else if( input == "DOWN" ) {
            do {
                move_overview_line( 1 );
                if( ++overview_pos >= static_cast<int>( overview_opts.size() ) ) {
                    overview_pos = 0;
                }
            } while( !overview_opts[overview_pos].selectable );
        } else if( input == "ANY_INPUT" ) {
            // did we try and activate a hotkey option?
            const input_event hotkey = main_context.get_raw_input();
            if( hotkey != input_event() && overview_action ) {
                auto iter = std::find_if( overview_opts.begin(),
                overview_opts.end(), [&hotkey]( const part_option & e ) {
                    return e.hotkey == hotkey;
                } );
                if( iter != overview_opts.end() ) {
                    overview_action( here,  *iter->part );
                    break;
                }
            }
        }
    }
}

void veh_interact::move_overview_line( int amount )
{
    overview_offset += amount;
    overview_offset = std::max( 0, overview_offset );
    overview_offset = std::min( overview_limit, overview_offset );
}

vehicle_part *veh_interact::get_most_damaged_part() const
{
    auto part_damage_comparison = []( const vpart_reference & a, const vpart_reference & b ) {
        return !b.part().removed && b.part().base.damage() > a.part().base.damage();
    };
    const vehicle_part_range vpr = veh->get_all_parts();
    auto high_damage_iterator = std::max_element( vpr.begin(),
                                vpr.end(),
                                part_damage_comparison );
    if( high_damage_iterator == vpr.end() ||
        high_damage_iterator->part().removed ) {
        return nullptr;
    }

    return &( *high_damage_iterator ).part();
}

vehicle_part *veh_interact::get_most_repairable_part() const
{
    return veh_utils::most_repairable_part( *veh, get_player_character() );
}

bool veh_interact::can_remove_part( map &here, int idx, const Character &you,
                                    task_speech *speech )
{
    sel_vehicle_part = &veh->part( idx );
    sel_vpart_info = &sel_vehicle_part->info();
    std::string nmsg;
    bool smash_remove = sel_vpart_info->has_flag( "SMASH_REMOVE" );
    const auto block = [&]( const std::string & reason ) {
        msg = reason;
        if( speech != nullptr ) {
            speech->blocked = remove_color_tags( reason );
        }
        return false;
    };

    if( veh->has_part( "NO_MODIFY_VEHICLE" ) && !sel_vpart_info->has_flag( "SIMPLE_PART" ) &&
        !smash_remove ) {
        return block( _( "This vehicle cannot be modified in this way.\n" ) );
    } else if( sel_vpart_info->has_flag( "NO_UNINSTALL" ) ) {
        return block( _( "This part cannot be uninstalled.\n" ) );
    }

    // What the removal returns; speech names the part without its durability symbol.
    std::vector<std::string> yield;
    std::function<std::string( const std::string & )> removal_text;
    if( sel_vehicle_part->is_broken() ) {
        removal_text = []( const std::string & part_name ) {
            return string_format(
                       _( "<color_white>Removing the broken %1$s may yield some fragments.</color>\n" ),
                       part_name );
        };
    } else if( smash_remove ) {
        std::set<std::string> removed_names;
        for( const item &it : sel_vehicle_part->pieces_for_broken_part() ) {
            removed_names.insert( it.tname() );
        }
        removal_text = [removed = enumerate_as_string( removed_names )]( const std::string & part_name ) {
            return string_format( _( "<color_white>Removing the %1$s may yield:</color>\n> %2$s\n" ),
                                  part_name, removed );
        };
    } else {
        item result_of_removal = veh->part_to_item( here, *sel_vehicle_part );
        removal_text = [result = result_of_removal.display_name()]( const std::string & part_name ) {
            return string_format(
                       _( "<color_white>Removing the %1$s will yield:</color>\n> %2$s\n" ),
                       part_name, result );
        };
    }
    nmsg += removal_text( sel_vehicle_part->name() );
    yield.push_back( removal_text( sel_vehicle_part->name( false ) ) );
    if( !sel_vehicle_part->is_broken() && !smash_remove ) {
        for( const item &it : sel_vehicle_part->get_salvageable() ) {
            nmsg += "> " + it.display_name() + "\n";
            yield.push_back( it.display_name() );
        }
    }
    if( speech != nullptr ) {
        for( std::string &line : yield ) {
            line = remove_color_tags( line );
            line.erase( std::remove( line.begin(), line.end(), '>' ), line.end() );
            while( !line.empty() && std::isspace( static_cast<unsigned char>( line.back() ) ) ) {
                line.pop_back();
            }
        }
        speech->requirements.push_back( accessibility::format_semantic_sequence( {
            string_join( yield, _( ", " ) )
        } ) );
    }

    const requirement_data reqs = sel_vpart_info->removal_requirements();
    bool ok = format_reqs( nmsg, reqs, sel_vpart_info->removal_skills,
                           sel_vpart_info->removal_time( you ), speech );

    nmsg += _( "<color_white>Additional requirements:</color>\n" );

    std::pair<bool, std::string> res = calc_lift_requirements( here, *sel_vpart_info, speech );
    if( !res.first ) {
        ok = res.first;
    }
    nmsg += res.second;

    const ret_val<void> unmount = veh->can_unmount( *sel_vehicle_part );
    if( !unmount.success() ) {
        //~ %1$s represents the internal color name which shouldn't be translated, %2$s is pre-translated reason
        nmsg += string_format( _( "> %1$s%2$s</color>" ), status_color( false ), unmount.str() ) + "\n";
        ok = false;
        if( speech != nullptr ) {
            speech->blocked = remove_color_tags( unmount.str() );
        }
    }
    const nc_color desc_color = sel_vehicle_part->is_broken() ? c_dark_gray : c_light_gray;
    sel_vehicle_part->info().format_description( nmsg, desc_color, getmaxx( w_msg ) - 4 );

    msg = colorize( nmsg, c_light_gray );
    return ok || get_avatar().has_trait( trait_DEBUG_HS );
}

void veh_interact::do_remove( map &here )
{
    task_reason reason = cant_do( here,  'o' );

    if( reason == task_reason::INVALID_TARGET ) {
        msg = _( "No parts here." );
        return;
    }
    if( accessibility::is_active() ) {
        accessible_choose_part( here, access_task::remove, parts_here );
        return;
    }

    restore_on_out_of_scope prev_title( title );
    title = _( "Choose a part here to remove:" );

    restore_on_out_of_scope prev_remove_info( std::move(
                remove_info ) );
    remove_info = std::make_unique<remove_info_t>();

    avatar &player_character = get_avatar();
    int pos = 0;
    for( size_t i = 0; i < parts_here.size(); i++ ) {
        if( can_remove_part( here, parts_here[ i ], player_character ) ) {
            pos = i;
            break;
        }
    }
    msg.reset();

    shared_ptr_fast<ui_adaptor> current_ui = create_or_get_ui_adaptor( here );

    restore_on_out_of_scope prev_overview_enable( overview_enable );

    restore_on_out_of_scope prev_hilight_part( highlight_part );

    while( true ) {
        int part = parts_here[ pos ];

        bool can_remove = can_remove_part( here, part, player_character );

        overview_enable = [this, part]( const map &,  const vehicle_part & pt ) {
            return &pt == &veh->part( part );
        };

        highlight_part = pos;

        calc_overview( here );
        ui_manager::redraw();

        //read input
        const std::string action = main_context.handle_input();
        msg.reset();
        if( can_remove && ( action == "REMOVE" || action == "CONFIRM" ) ) {
            switch( reason ) {
                case task_reason::LOW_MORALE:
                    msg = _( "Your morale is too low to construct…" );
                    return;
                case task_reason::LOW_LIGHT:
                    msg = _( "It's too dark to see what you are doing…" );
                    return;
                case task_reason::NOT_FREE:
                    msg = _( "You cannot remove that part while something is attached to it." );
                    return;
                case task_reason::MOVING_VEHICLE:
                    msg = _( "Better not remove something while driving." );
                    return;
                default:
                    break;
            }

            // Modifying a vehicle with rotors will make in not flightworthy (until we've got a better model)
            // It can only be the player doing this - an npc won't work well with query_yn
            if( veh->would_removal_prevent_flyable( veh->part( part ), player_character ) ) {
                if( query_yn(
                        _( "Removing this part will mean that this vehicle is no longer flightworthy.  Continue?" ) ) ) {
                    veh->set_flyable( false );
                } else {
                    return;
                }
            }
            for( const Character *helper : player_character.get_crafting_helpers() ) {
                add_msg( m_info, _( "%s helps with this task…" ), helper->get_name() );
            }
            sel_cmd = 'o';
            break;
        } else if( action == "QUIT" ) {
            break;
        } else {
            move_in_list( pos, action, parts_here.size() );
        }
    }
    veh->recalculate_enchantment_cache();
}

void veh_interact::do_siphon( map &here )
{
    switch( cant_do( here,  's' ) ) {
        case task_reason::INVALID_TARGET:
            msg = _( "The vehicle has no liquid fuel left to siphon." );
            return;

        case task_reason::LACK_TOOLS:
            msg = _( "You need a <color_red>hose</color> to siphon liquid fuel." );
            return;

        case task_reason::MOVING_VEHICLE:
            msg = _( "You can't siphon from a moving vehicle." );
            return;

        default:
            break;
    }

    restore_on_out_of_scope prev_title( title );
    title = _( "Select part to siphon:" );

    auto sel = [&]( const map &,  const vehicle_part & pt ) {
        return pt.is_tank() && !pt.base.empty() &&
               pt.base.only_item().made_of( phase_id::LIQUID );
    };

    auto act = [&]( map & here, const vehicle_part & pt ) {
        on_out_of_scope restore_ui( [&]() {
            hide_ui( here, false );
        } );
        hide_ui( here, true );
        const item &base = pt.get_base();
        const int idx = veh->index_of_part( &pt );
        item liquid( base.legacy_front() );
        const int liq_charges = liquid.charges;
        liquid_dest_opt liquid_target;
        if( liquid_handler::handle_liquid( liquid, liquid_target, nullptr, 1, nullptr, veh, idx ) ) {
            veh->drain( here, idx, liq_charges - liquid.charges );
        }
    };

    overview( here, sel, act );
}

bool veh_interact::do_unload( map &here )
{
    switch( cant_do( here, 'd' ) ) {
        case task_reason::INVALID_TARGET:
            msg = _( "The vehicle has no solid fuel left to remove." );
            return false;

        case task_reason::MOVING_VEHICLE:
            msg = _( "You can't unload from a moving vehicle." );
            return false;

        default:
            break;
    }

    act_vehicle_unload_fuel( here, veh );
    return true;
}

static void do_change_shape_menu( vehicle_part &vp )
{
    const vpart_info &vpi = vp.info();
    uilist smenu;
    smenu.text = _( "Choose cosmetic variant:" );
    int ret_code = 0;
    int default_selection = 0;
    std::vector<std::string> variants;
    for( const auto& [variant_id, vv] : vpi.variants ) {
        if( variant_id == vp.variant ) {
            default_selection = ret_code;
        }
        uilist_entry entry( vv.get_label() );
        entry.txt = entry.txt.empty() ? _( "Default" ) : entry.txt;
        entry.retval = ret_code++;
        entry.extratxt.left = 1;
        entry.extratxt.sym = vv.get_symbol_curses( 0_degrees, false );
        entry.extratxt.color = vpi.color;
        variants.emplace_back( variant_id );
        smenu.entries.emplace_back( entry );
    }
    sort_uilist_entries_by_line_drawing( smenu.entries );

    // get default selection after sorting
    for( std::size_t i = 0; i < smenu.entries.size(); ++i ) {
        if( smenu.entries[i].retval == default_selection ) {
            default_selection = i;
            break;
        }
    }

    smenu.selected = default_selection;
    smenu.query();
    if( smenu.ret >= 0 ) {
        vp.variant = variants[smenu.ret];
    }
}

void veh_interact::do_assign_crew( map &here )
{
    if( cant_do( here,  'w' ) != task_reason::CAN_DO ) {
        msg = _( "Need at least one seat and an ally to assign crew members." );
        return;
    }

    restore_on_out_of_scope prev_title( title );
    title = _( "Assign crew positions:" );

    auto sel = []( const map &, const vehicle_part & pt ) {
        return pt.is_seat();
    };

    auto act = [&]( map &, vehicle_part & pt ) {
        uilist menu;
        menu.text = _( "Select crew member" );

        if( pt.crew() ) {
            menu.addentry( 0, true, 'c', _( "Clear assignment" ) );
        }

        for( const npc *e : g->allies() ) {
            menu.addentry( e->getID().get_value(), true, -1, e->get_name() );
        }

        menu.query();
        if( menu.ret == 0 ) {
            pt.unset_crew();
        } else if( menu.ret > 0 ) {
            const npc &who = *g->critter_by_id<npc>( character_id( menu.ret ) );
            veh->assign_seat( pt, who );
        }
    };

    overview( here, sel, act );
}

void veh_interact::do_rename()
{
    std::string name = string_input_popup()
                       .title( _( "Enter new vehicle name:" ) )
                       .width( 20 )
                       .query_string();
    if( !name.empty() ) {
        veh->name = name;
        if( veh->tracking_on ) {
            overmap_buffer.remove_vehicle( veh );
            // Add the vehicle again, this time with the new name
            overmap_buffer.add_vehicle( veh );
        }
    }
}

void veh_interact::do_relabel( const map &here )
{
    if( cant_do( here,  'a' ) == task_reason::INVALID_TARGET ) {
        msg = _( "There are no parts here to label." );
        return;
    }

    const vpart_position vp( *veh, cpart );
    string_input_popup pop;
    std::string text = pop
                       .title( _( "New label:" ) )
                       .width( 20 )
                       .text( vp.get_label().value_or( "" ) )
                       .query_string();
    if( pop.confirmed() ) {
        vp.set_label( text );
    }
}

std::pair<bool, std::string> veh_interact::calc_lift_requirements( map &here, const vpart_info
        &sel_vpart_info, task_speech *speech )
{
    int lvl = 0;
    int str = 0;
    quality_id qual;
    bool use_aid = false;
    bool use_str = false;
    bool ok = true;
    std::string nmsg;
    avatar &player_character = get_avatar();

    if( sel_vpart_info.has_flag( "NEEDS_JACKING" ) ) {
        if( terrain_here.has_flag( ter_furn_flag::TFLAG_LIQUID ) ) {
            const auto wrap_badter = foldstring(
                                         _( "<color_red>Unsuitable terrain</color> for working on part that requires jacking." ),
                                         getmaxx( w_msg ) - 4 );
            nmsg += "> " + wrap_badter[0] + "\n";
            for( size_t i = 1; i < wrap_badter.size(); i++ ) {
                nmsg += "  " + wrap_badter[i] + "\n";
            }
            if( speech != nullptr ) {
                speech->requirements.push_back( accessibility::format_semantic_sequence( {
                    remove_color_tags( _( "<color_red>Unsuitable terrain</color> for working on part that requires jacking." ) )
                } ) );
                speech->missing++;
            }
            return std::pair<bool, std::string> ( false, nmsg );
        }
        qual = qual_JACK;
        lvl = jack_quality( here, *veh );
        str = veh->lift_strength( here );
        use_aid = ( max_jack >= lifting_quality_to_mass( lvl ) ) || can_self_jack( here );
        use_str = player_character.can_lift( *veh, here );
    } else {
        item base( sel_vpart_info.base_item );
        qual = qual_LIFT;
        lvl = std::ceil( units::quantity<double, units::mass::unit_type>( base.weight() ) /
                         lifting_quality_to_mass( 1 ) );
        str = base.lift_strength();
        use_aid = max_lift >= base.weight();
        use_str = player_character.can_lift( base );
    }

    if( !( use_aid || use_str ) ) {
        ok = false;
    }

    std::string str_suffix;
    int lift_strength = player_character.get_lift_str();
    int total_lift_strength = lift_strength + player_character.get_lift_assist();
    int total_base_strength = player_character.get_arm_str() + player_character.get_lift_assist();

    if( player_character.has_trait( trait_STRONGBACK ) && total_lift_strength >= str &&
        total_base_strength < str ) {
        str_suffix = string_format( _( "(Strong Back helped, giving +%d strength)" ),
                                    lift_strength - player_character.get_str() );
    } else if( player_character.has_trait( trait_BADBACK ) && total_base_strength >= str &&
               total_lift_strength < str ) {
        str_suffix = string_format( _( "(Bad Back reduced usable strength by %d)" ),
                                    lift_strength - player_character.get_str() );
    }
    if( player_character.get_str() > lift_strength ) {
        str_suffix += str_suffix.empty() ? "" : "  ";
        str_suffix += string_format( _( "(Effective lifting strength is %d)" ), lift_strength );
    }

    nc_color aid_color = use_aid ? c_green : ( use_str ? c_dark_gray : c_red );
    nc_color str_color = use_str ? c_green : ( use_aid ? c_dark_gray : c_red );
    const std::vector<Character *> helpers = player_character.get_crafting_helpers();
    //~ %1$s is quality name, %2$d is quality level
    std::string aid_string = string_format( _( "1 tool with %1$s %2$d" ),
                                            qual.obj().name, lvl );

    std::string str_string;
    if( !helpers.empty() ) {
        str_string = string_format( _( "strength ( assisted ) %d %s" ), str, str_suffix );
    } else {
        str_string = string_format( _( "strength %d %s" ), str, str_suffix );
    }

    nmsg += string_format( _( "> %1$s <color_white>OR</color> %2$s" ),
                           colorize( aid_string, aid_color ),
                           colorize( str_string, str_color ) ) + "\n";
    if( speech != nullptr ) {
        std::string spoken_strength = str_string;
        while( !spoken_strength.empty() && spoken_strength.back() == ' ' ) {
            spoken_strength.pop_back();
        }
        speech->requirements.push_back( accessibility::format_vehicle_lift_requirement( aid_string,
                                        use_aid, spoken_strength, use_str ) );
        speech->missing += ok ? 0 : 1;
    }

    std::pair<bool, std::string> result( ok, nmsg );
    return result;
}

/**
 * Returns the first part on the vehicle at the given position.
 * @param d The coordinates, relative to the viewport's 0-point (?)
 * @return The first vehicle part at the specified coordinates.
 */
int veh_interact::part_at( const point_rel_ms &d )
{
    const point_rel_ms vd{ -dd + d.rotate( 1 ) };
    return veh->part_displayed_at( vd );
}

/**
 * Checks to see if you can potentially install this part at current position.
 * Affects coloring in display_list() and is also used to
 * sort can_mount so potentially installable parts come first.
 */
bool veh_interact::can_potentially_install( const vpart_info &vpart )
{
    bool engine_reqs_met = true;
    bool can_make = vpart.install_requirements().can_make_with_inventory( *crafting_inv,
                    is_crafting_component, 1, craft_flags::none, false );
    bool hammerspace = get_player_character().has_trait( trait_DEBUG_HS );

    int engines = 0;
    if( vpart.has_flag( VPFLAG_ENGINE ) && vpart.has_flag( "E_HIGHER_SKILL" ) ) {
        for( const vpart_reference &vp : veh->get_avail_parts( "ENGINE" ) ) {
            if( vp.has_feature( "E_HIGHER_SKILL" ) ) {
                engines++;
            }
        }
        engine_reqs_met = engines < 2;
    }

    return hammerspace || ( can_make && engine_reqs_met && !vpart.has_flag( VPFLAG_APPLIANCE ) );
}

/**
 * Moves the cursor on the vehicle editing window.
 * @param d How far to move the cursor.
 * @param dstart_at How far to change the start position for vehicle part descriptions
 */
void veh_interact::move_cursor( map &here, const point_rel_ms &d, int dstart_at )
{
    dd += d.rotate( 3 );
    if( d != point_rel_ms::zero ) {
        start_limit = 0;
    } else {
        start_at += dstart_at;
    }

    // Update the current active component index to the new position.
    cpart = part_at( point_rel_ms::zero );
    const point_rel_ms vd = -dd;
    const point_rel_ms q = veh->coord_translate( vd );
    const tripoint_bub_ms vehp = veh->pos_bub( here ) + q;
    const bool has_critter = get_creature_tracker().creature_at( vehp );
    terrain_here = here.ter( vehp ).obj();
    bool obstruct = here.impassable_ter_furn( vehp );
    const optional_vpart_position ovp = here.veh_at( vehp );
    if( ovp && &ovp->vehicle() != veh ) {
        obstruct = true;
    }
    cursor_obstructed = obstruct;

    can_mount.clear();
    if( !obstruct ) {
        std::vector<const vpart_info *> req_missing;
        for( const vpart_info &vpi : vehicles::parts::get_all() ) {
            if( has_critter && vpi.has_flag( VPFLAG_OBSTACLE ) ) {
                continue;
            }
            if( vpi.has_flag( "NO_INSTALL_HIDDEN" ) ||
                vpi.has_flag( VPFLAG_APPLIANCE ) ) {
                continue; // hide parts with incompatible flags
            }
            if( can_potentially_install( vpi ) ) {
                can_mount.push_back( &vpi );
            } else {
                req_missing.push_back( &vpi );
            }
        }
        auto vpart_localized_sort = []( const vpart_info * a, const vpart_info * b ) {
            return localized_compare( a->name(), b->name() );
        };
        std::sort( can_mount.begin(), can_mount.end(), vpart_localized_sort );
        std::sort( req_missing.begin(), req_missing.end(), vpart_localized_sort );
        can_mount.insert( can_mount.end(), req_missing.cbegin(), req_missing.cend() );
    }

    need_repair.clear();
    parts_here.clear();
    if( cpart >= 0 ) {
        parts_here = veh->parts_at_relative( veh->part( cpart ).mount, true );
        for( size_t i = 0; i < parts_here.size(); i++ ) {
            vehicle_part &pt = veh->part( parts_here[i] );

            if( pt.is_repairable() || pt.is_broken() ) {
                need_repair.push_back( i );
            }
        }
    }

    /* Update the lifting quality to be the that is available for this newly selected tile */
    cache_tool_availability_update_lifting( vehp );
}

void veh_interact::display_grid()
{
    // border window
    draw_border( w_border );

    wattron( w_border, BORDER_COLOR );

    // match grid lines
    const int y_mode = getmaxy( w_mode ) + 1;
    // |-
    mvwaddch( w_border, point( 0, y_mode ), LINE_XXXO );
    // -|
    mvwaddch( w_border, point( TERMX - 1, y_mode ), LINE_XOXX );
    const int y_list = getbegy( w_list ) + getmaxy( w_list );
    // |-
    mvwaddch( w_border, point( 0, y_list ), LINE_XXXO );
    // -|
    mvwaddch( w_border, point( TERMX - 1, y_list ), LINE_XOXX );

    const int grid_w = getmaxx( w_border ) - 2;

    // Two lines dividing the three middle sections.
    mvwvline( w_border, point( getmaxx( w_disp ) + 1,                     getmaxy( w_mode ) + 2 ),
              LINE_XOXO, page_size );
    mvwvline( w_border, point( getmaxx( w_disp ) + 2 + getmaxx( w_list ), getmaxy( w_mode ) + 2 ),
              LINE_XOXO, page_size );
    // Two lines dividing the vertical menu sections.
    mvwhline( w_border, point( 1,             getmaxy( w_mode ) + 1 ), LINE_OXOX, grid_w );
    mvwhline( w_border, point( 1, getmaxy( w_mode ) + 2 + page_size ), LINE_OXOX, grid_w );
    // Fix up the line intersections.
    mvwaddch( w_border, point( getmaxx( w_disp ) + 1, getmaxy( w_mode ) + 1 ), LINE_OXXX );
    // _|_
    mvwaddch( w_border, point( getmaxx( w_disp ) + 1, getmaxy( w_mode ) + 2 + page_size ), LINE_XXOX );
    mvwaddch( w_border, point( getmaxx( w_disp ) + 2 + getmaxx( w_list ), getmaxy( w_mode ) + 1 ),
              LINE_OXXX );
    // _|_
    mvwaddch( w_border, point( getmaxx( w_disp ) + 2 + getmaxx( w_list ),
                               getmaxy( w_mode ) + 2 + page_size ),
              LINE_XXOX );

    wattroff( w_border, BORDER_COLOR );

    wnoutrefresh( w_border );
}

/**
 * Draws the viewport with the vehicle.
 */
void veh_interact::display_veh( map &here )
{
    werase( w_disp );
    const point h_size = point( getmaxx( w_disp ), getmaxy( w_disp ) ) / 2;

    if( debug_mode ) {
        // show CoM, pivot in debug mode

        const point_rel_ms &pivot = veh->pivot_point( here );
        const point_rel_ms &com = veh->local_center_of_mass( here );

        mvwprintz( w_disp, point::zero, c_green, "CoM   %d,%d", com.x(), com.y() );
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        mvwprintz( w_disp, point( 0, 1 ), c_red,   "Pivot %d,%d", pivot.x(), pivot.y() );

        const point_rel_ms com_s = ( com + dd ).rotate( 3 ) + h_size;
        const point_rel_ms pivot_s = ( pivot + dd ).rotate( 3 ) + h_size;

        mvwhline( w_disp, point( 0, com_s.y() ), c_green, LINE_OXOX, std::min( getmaxx( w_disp ),
                  com_s.x() + 1 ) );
        mvwvline( w_disp, point( com_s.x(), 0 ), c_green, LINE_XOXO, std::min( getmaxy( w_disp ),
                  com_s.y() + 1 ) );

        mvwhline( w_disp, point( std::max( 0, pivot_s.x() ), pivot_s.y() ), c_red, LINE_OXOX,
                  getmaxx( w_disp ) - std::max( 0, pivot_s.x() ) + 1 );
        mvwvline( w_disp, point( pivot_s.x(), std::max( 0, pivot_s.y() ) ), c_red, LINE_XOXO,
                  getmaxy( w_disp ) - std::max( 0, pivot_s.y() ) + 1 );
    }

    // Draw guidelines to make current selection point more visible.
    mvwvline( w_disp, point( h_size.x, 0 ), c_dark_gray, LINE_XOXO, getmaxy( w_disp ) );
    mvwhline( w_disp, point( 0, h_size.y ), c_dark_gray, LINE_OXOX, getmaxx( w_disp ) );

    nc_color col_at_cursor = c_black;
    int sym_at_cursor = ' ';
    //Iterate over structural parts so we only hit each square once
    for( const int structural_part_idx : veh->all_parts_at_location( "structure" ) ) {
        const vehicle_part &vp = veh->part( structural_part_idx );
        const vpart_display vd = veh->get_display_of_tile( vp.mount, false, false );
        const point_rel_ms q = ( vp.mount + dd ).rotate( 3 );

        if( q != point_rel_ms::zero ) { // cursor is not on this part
            mvwputch( w_disp, h_size + q.raw(), vd.color, vd.symbol_curses );
            continue;
        }
        cpart = structural_part_idx;
        col_at_cursor = vd.color;
        sym_at_cursor = vd.symbol_curses;
    }

    const point pt_disp( getmaxx( w_disp ) / 2, getmaxy( w_disp ) / 2 );
    const tripoint_bub_ms pos_at_cursor = veh->pos_bub( here ) + veh->coord_translate( -dd );
    const optional_vpart_position ovp = here.veh_at( pos_at_cursor );
    col_at_cursor = hilite( col_at_cursor );
    if( here.impassable_ter_furn( pos_at_cursor ) || ( ovp && &ovp->vehicle() != veh ) ) {
        col_at_cursor = red_background( col_at_cursor );
    }
    mvwputch( w_disp, pt_disp, col_at_cursor, sym_at_cursor );
    wnoutrefresh( w_disp );
}

static std::string wheel_state_description( map &here, const vehicle &veh )
{
    bool is_boat = !veh.floating.empty();
    bool is_land = !veh.wheelcache.empty() || !is_boat;

    bool suf_land = veh.sufficient_wheel_config();
    bool bal_land = veh.balanced_wheel_config( here );

    bool suf_boat = veh.can_float( here );

    float steer = veh.steering_effectiveness( here );

    std::string wheel_status;
    if( !suf_land && is_boat ) {
        wheel_status = _( "<color_light_red>disabled</color>" );
    } else if( !suf_land ) {
        wheel_status = _( "<color_light_red>lack</color>" );
    } else if( !bal_land ) {
        wheel_status = _( "<color_light_red>unbalanced</color>" );
    } else if( steer < 0 ) {
        wheel_status = _( "<color_light_red>no steering</color>" );
    } else if( steer < 0.033 ) {
        wheel_status = _( "<color_light_red>broken steering</color>" );
    } else if( steer < 0.5 ) {
        wheel_status = _( "<color_light_red>poor steering</color>" );
    } else {
        wheel_status = _( "<color_light_green>enough</color>" );
    }

    std::string boat_status;
    if( !suf_boat ) {
        boat_status = _( "<color_light_red>sinks</color>" );
    } else {
        boat_status = _( "<color_light_blue>floats</color>" );
    }

    if( is_boat && is_land ) {
        return string_format( _( "Wheels/boat: %s/%s" ), wheel_status, boat_status );
    }

    if( is_boat ) {
        return string_format( _( "Boat: %s" ), boat_status );
    }

    return string_format( _( "Wheels: %s" ), wheel_status );
}

/**
 * Displays the vehicle's stats at the bottom of the window.
 */
void veh_interact::display_stats( map &here ) const
{
    werase( w_stats_1 );
    werase( w_stats_2 );
    werase( w_stats_3 );

    on_out_of_scope refresh_windows( [&]() {
        wnoutrefresh( w_stats_1 );
        wnoutrefresh( w_stats_2 );
        wnoutrefresh( w_stats_3 );
    } );

    // 3 * stats_h
    const int slots = 24;
    std::array<const catacurses::window *, slots> win;
    std::array<int, slots> row;

    units::volume total_cargo = 0_ml;
    units::volume free_cargo = 0_ml;
    for( const vpart_reference &vpr : veh->get_any_parts( VPFLAG_CARGO ) ) {
        const vehicle_stack vs = vpr.items();
        total_cargo += vs.max_volume();
        free_cargo += vs.free_volume();
    }

    for( int i = 0; i < slots; i++ ) {
        if( i < stats_h ) {
            // First column
            win[i] = &w_stats_1;
            row[i] = i;
        } else if( i < ( 2 * stats_h ) ) {
            // Second column
            win[i] = &w_stats_2;
            row[i] = i - stats_h;
        } else {
            // Third column
            win[i] = &w_stats_3;
            row[i] = i - 2 * stats_h;
        }
    }

    bool is_boat = !veh->floating.empty();
    bool is_ground = !veh->wheelcache.empty() || !is_boat;
    bool is_aircraft = veh->is_rotorcraft( here ) && veh->is_flying_in_air();

    const auto vel_to_int = []( const double vel ) {
        return static_cast<int>( convert_velocity( vel, VU_VEHICLE ) );
    };

    int i = 0;
    if( is_aircraft ) {
        fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                        _( "Air Safe/Top speed: <color_light_green>%3d</color>/<color_light_red>%3d</color> %s" ),
                        vel_to_int( veh->safe_rotor_velocity( here, false ) ),
                        vel_to_int( veh->max_rotor_velocity( here, false ) ),
                        velocity_units( VU_VEHICLE ) );
        i += 1;
        fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                        _( "Air acceleration: <color_light_blue>%3d</color> %s/s" ),
                        vel_to_int( veh->rotor_acceleration( here, false ) ),
                        velocity_units( VU_VEHICLE ) );
        i += 1;
    } else {
        if( is_ground ) {
            fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                            _( "Safe/Top speed: <color_light_green>%3d</color>/<color_light_red>%3d</color> %s" ),
                            vel_to_int( veh->safe_ground_velocity( here, false ) ),
                            vel_to_int( veh->max_ground_velocity( here, false ) ),
                            velocity_units( VU_VEHICLE ) );
            i += 1;
            // TODO: extract accelerations units to its own function
            fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                            //~ /t means per turn
                            _( "Acceleration: <color_light_blue>%3d</color> %s/s" ),
                            vel_to_int( veh->ground_acceleration( here, false ) ),
                            velocity_units( VU_VEHICLE ) );
            i += 1;
        } else {
            i += 2;
        }
        if( is_boat ) {
            fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                            _( "Water Safe/Top speed: <color_light_green>%3d</color>/<color_light_red>%3d</color> %s" ),
                            vel_to_int( veh->safe_water_velocity( here, false ) ),
                            vel_to_int( veh->max_water_velocity( here, false ) ),
                            velocity_units( VU_VEHICLE ) );
            i += 1;
            // TODO: extract accelerations units to its own function
            fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                            //~ /t means per turn
                            _( "Water acceleration: <color_light_blue>%3d</color> %s/s" ),
                            vel_to_int( veh->water_acceleration( here, false ) ),
                            velocity_units( VU_VEHICLE ) );
            i += 1;
        } else {
            i += 2;
        }
    }
    fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                    _( "Mass: <color_light_blue>%5.0f</color> %s" ),
                    convert_weight( veh->total_mass( here ) ), weight_units() );
    i += 1;
    fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                    disp_w > 35 ? _( "Cargo volume: <color_light_blue>%s</color> / <color_light_blue>%s</color> %s" ) :
                    _( "Cargo: <color_light_blue>%s</color> / <color_light_blue>%s</color> %s" ),
                    format_volume( total_cargo - free_cargo ),
                    format_volume( total_cargo ), volume_units_abbr() );
    i += 1;
    // Write the overall damage
    mvwprintz( *win[i], point( 0, row[i] ), c_light_gray, _( "Status: " ) );
    fold_and_print( *win[i], point( utf8_width( _( "Status: " ) ), row[i] ), getmaxx( *win[i] ),
                    total_durability_color, total_durability_text );
    i += 1;

    fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                    wheel_state_description( here, *veh ) );
    i += 1;

    if( install_info || remove_info ) {
        // don't draw the second and third columns which would be overwritten by w_details
        return;
    }

    // advance to next column if necessary
    i = std::max( i, stats_h );

    //This lambda handles printing parts in the "Most damaged" and "Needs repair" cases
    //for the veh_interact ui
    const auto print_part = [&]( const std::string & str, int slot, vehicle_part * pt ) {
        mvwprintz( *win[slot], point( 0, row[slot] ), c_light_gray, str );
        int iw = utf8_width( str ) + 1;
        return fold_and_print( *win[slot], point( iw, row[slot] ), getmaxx( *win[slot] ), c_light_gray,
                               pt->name() );
    };

    vehicle_part *mostDamagedPart = get_most_damaged_part();
    vehicle_part *most_repairable = get_most_repairable_part();

    // Write the most damaged part
    if( mostDamagedPart && mostDamagedPart->damage_percent() ) {
        const std::string damaged_header = mostDamagedPart == most_repairable ?
                                           _( "Most damaged:" ) :
                                           _( "Most damaged (can't repair):" );
        i += print_part( damaged_header, i, mostDamagedPart );
    } else {
        i += 1;
    }

    // Write the part that needs repair the most.
    if( most_repairable && most_repairable != mostDamagedPart ) {
        const std::string needsRepair = _( "Needs repair:" );
        i += print_part( needsRepair, i, most_repairable );
    } else {
        i += 1;
    }

    fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                    _( "Air drag:       <color_light_blue>%5.2f</color>" ),
                    veh->coeff_air_drag() );
    i += 1;

    if( is_boat ) {
        fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                        _( "Water drag:     <color_light_blue>%5.2f</color>" ),
                        veh->coeff_water_drag( here ) );
    }
    i += 1;

    if( is_ground ) {
        fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                        _( "Rolling drag:   <color_light_blue>%5.2f</color>" ),
                        veh->coeff_rolling_drag( here ) );
    }
    i += 1;

    fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                    _( "Static drag:    <color_light_blue>%5d</color>" ),
                    units::to_watt( veh->static_drag( false ) ) );
    i += 1;

    fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                    _( "Offroad:        <color_light_blue>%4d</color>%%" ),
                    static_cast<int>( veh->k_traction( here, veh->wheel_area() *
                                      veh->average_offroad_rating() ) * 100 ) );
    i += 1;

    if( is_boat ) {

        const double water_clearance = veh->water_hull_height( here ) - veh->water_draft( here );
        const char *draft_string = water_clearance > 0 ?
                                   _( "Draft/Clearance:<color_light_blue>%4.2f</color>m/<color_light_blue>%4.2f</color>m" ) :
                                   _( "Draft/Clearance:<color_light_blue>%4.2f</color>m/<color_light_red>%4.2f</color>m" ) ;

        fold_and_print( *win[i], point( 0, row[i] ), getmaxx( *win[i] ), c_light_gray,
                        draft_string,
                        veh->water_draft( here ), water_clearance );
        i += 1;
    }

    // advance to next column if necessary
    i = std::max( i, 2 * stats_h );

    // Print fuel percentage & type name only if it fits in the window, 10 is width of "E...F 100%"
    veh->print_fuel_indicators( here, *win[i], point( 0, row[i] ), fuel_index, true,
                                ( getmaxx( *win[i] ) > 10 ),
                                ( getmaxx( *win[i] ) > 10 ) );

}

void veh_interact::display_name()
{
    werase( w_name );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    mvwprintz( w_name, point( 1, 0 ), c_light_gray, _( "Name: " ) );

    mvwprintz( w_name, point( 1 + utf8_width( _( "Name: " ) ), 0 ),
               !veh->is_owned_by( get_player_character(), true ) ? c_light_red : c_light_green,
               string_format( _( "%s (%s)" ), veh->name, veh->get_owner_name() ) );
    wnoutrefresh( w_name );
}

static std::string veh_act_desc( const input_context &ctxt, const std::string &id,
                                 const std::string &desc, const task_reason reason )
{
    static const translation inline_fmt_enabled = to_translation(
                "keybinding", "<color_light_gray>%1$s<color_light_green>%2$s</color>%3$s</color>" );
    static const translation inline_fmt_disabled = to_translation(
                "keybinding", "<color_dark_gray>%1$s<color_green>%2$s</color>%3$s</color>" );
    static const translation separate_fmt_enabled = to_translation(
                "keybinding", "<color_light_gray><color_light_green>%1$s</color>-%2$s</color>" );
    static const translation separate_fmt_disabled = to_translation(
                "keybinding", "<color_dark_gray><color_green>%1$s</color>-%2$s</color>" );
    if( reason == task_reason::CAN_DO ) {
        return ctxt.get_desc( id, desc, input_context::allow_all_keys,
                              inline_fmt_enabled, separate_fmt_enabled );
    } else {
        return ctxt.get_desc( id, desc, input_context::allow_all_keys,
                              inline_fmt_disabled, separate_fmt_disabled );
    }
}

/**
 * Prints the list of usable commands, and highlights the hotkeys used to activate them.
 */
void veh_interact::display_mode( const map &here )
{
    werase( w_mode );

    if( title.has_value() ) {
        nc_color title_col = c_light_gray;
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        print_colored_text( w_mode, point( 1, 0 ), title_col, title_col, title.value() );
    } else {
        constexpr size_t action_cnt = 12;
        const std::array<std::string, action_cnt> actions = { {
                veh_act_desc( main_context, "INSTALL",
                              pgettext( "veh_interact", "install" ),
                              cant_do( here,  'i' ) ),
                veh_act_desc( main_context, "REPAIR",
                              pgettext( "veh_interact", "repair" ),
                              cant_do( here, 'r' ) ),
                veh_act_desc( main_context, "MEND",
                              pgettext( "veh_interact", "mend" ),
                              cant_do( here, 'm' ) ),
                veh_act_desc( main_context, "REFILL",
                              pgettext( "veh_interact", "refill" ),
                              cant_do( here,  'f' ) ),
                veh_act_desc( main_context, "REMOVE",
                              pgettext( "veh_interact", "remove" ),
                              cant_do( here,  'o' ) ),
                veh_act_desc( main_context, "SIPHON",
                              pgettext( "veh_interact", "siphon" ),
                              cant_do( here,  's' ) ),
                veh_act_desc( main_context, "UNLOAD",
                              pgettext( "veh_interact", "unload" ),
                              cant_do( here,  'd' ) ),
                veh_act_desc( main_context, "ASSIGN_CREW",
                              pgettext( "veh_interact", "crew" ),
                              cant_do( here, 'w' ) ),
                veh_act_desc( main_context, "CHANGE_SHAPE",
                              pgettext( "veh_interact", "shape" ),
                              cant_do( here, 'p' ) ),
                veh_act_desc( main_context, "RENAME",
                              pgettext( "veh_interact", "rename" ),
                              task_reason::CAN_DO ),
                veh_act_desc( main_context, "RELABEL",
                              pgettext( "veh_interact", "label" ),
                              cant_do( here,  'a' ) ),
                veh_act_desc( main_context, "QUIT",
                              pgettext( "veh_interact", "back" ),
                              task_reason::CAN_DO ),
            }
        };

        std::array < int, action_cnt + 1 > pos;
        pos[0] = 0;
        for( size_t i = 0; i < action_cnt; i++ ) {
            pos[i + 1] = pos[i] + utf8_width( actions[i], true );
        }
        const int space = std::max<int>( getmaxx( w_mode ) - pos.back(), action_cnt + 1 );
        for( size_t i = 0; i < action_cnt; i++ ) {
            nc_color dummy = c_white;
            print_colored_text( w_mode, point( pos[i] + space * ( i + 1 ) / ( action_cnt + 1 ), 0 ),
                                dummy, c_white, actions[i] );
        }
    }
    wnoutrefresh( w_mode );
}

/**
 * Draws the list of parts that can be mounted in the selected square. Used
 * when installing new parts.
 * @param pos The current cursor position in the list.
 * @param list The list to display parts from.
 * @param header Number of lines occupied by the list header
 */
void veh_interact::display_list( size_t pos, const std::vector<const vpart_info *> &list,
                                 const int header )
{
    werase( w_list );
    int lines_per_page = page_size - header;
    size_t page = pos / lines_per_page;
    for( size_t i = page * lines_per_page; i < ( page + 1 ) * lines_per_page && i < list.size(); i++ ) {
        const vpart_info &info = *list[i];
        const vpart_variant &vv = info.variants.at( info.variant_default );
        int y = i - page * lines_per_page + header;
        mvwputch( w_list, point( 1, y ), info.color, vv.get_symbol_curses( 0_degrees, false ) );
        nc_color col = can_potentially_install( info ) ? c_white : c_dark_gray;
        trim_and_print( w_list, point( 3, y ), getmaxx( w_list ) - 3, pos == i ? hilite( col ) : col,
                        info.name() );
    }

    if( install_info ) {
        auto &tab_list = install_info->tab_list;
        size_t &tab = install_info->tab;
        // draw tab menu
        int tab_x = 0;
        for( size_t i = 0; i < tab_list.size(); i++ ) {
            bool active = tab == i; // current tab is active
            std::string tab_name = active ? tab_list[i].name() : tab_list[i].short_name();
            tab_x += active; // add a space before selected tab
            draw_subtab( w_list, tab_x, tab_name, active, false );
            // one space padding and add a space after selected tab
            tab_x += 1 + utf8_width( tab_name ) + active;
        }
    }
    wnoutrefresh( w_list );
}

/**
 * Used when installing parts.
 * Opens up w_details containing info for part currently selected in w_list.
 */
void veh_interact::display_details( const vpart_info *part )
{
    const int details_w = getmaxx( w_details );

    werase( w_details );

    draw_border( w_details );

    if( part == nullptr ) {
        wnoutrefresh( w_details );
        return;
    }
    // displays data in two columns
    int column_width = details_w / 2;
    int col_1 = 2;
    int col_2 = col_1 + column_width;
    int line = 0;
    bool small_mode = column_width < 20;

    // line 0: part name
    fold_and_print( w_details, point( col_1, line ), details_w, c_light_green, part->name() );

    // line 1: (column 1) durability   (column 2) damage mod
    fold_and_print( w_details, point( col_1, line + 1 ), column_width, c_white,
                    "%s: <color_light_gray>%d</color>",
                    small_mode ? _( "Dur" ) : _( "Durability" ),
                    part->durability );
    fold_and_print( w_details, point( col_2, line + 1 ), column_width, c_white,
                    "%s: <color_light_gray>%d%%</color>",
                    small_mode ? _( "Dmg" ) : _( "Damage" ),
                    part->dmg_mod );

    // line 2: (column 1) weight   (column 2) folded volume (if applicable)
    fold_and_print( w_details, point( col_1, line + 2 ), column_width, c_white,
                    "%s: <color_light_gray>%.1f%s</color>",
                    small_mode ? _( "Wgt" ) : _( "Weight" ),
                    convert_weight( item::find_type( part->base_item )->weight ),
                    weight_units() );
    if( part->folded_volume ) {
        fold_and_print( w_details, point( col_2, line + 2 ), column_width, c_white,
                        "%s: <color_light_gray>%s %s</color>",
                        small_mode ? _( "FoldVol" ) : _( "Folded Volume" ),
                        format_volume( part->folded_volume.value() ),
                        volume_units_abbr() );
    }

    // line 3: (column 1) size, bonus, wheel diameter (if applicable)    (column 2) epower, wheel width (if applicable)
    if( part->size > 0_ml && part->has_flag( VPFLAG_CARGO ) ) {
        fold_and_print( w_details, point( col_1, line + 3 ), column_width, c_white,
                        "%s: <color_light_gray>%s %s</color>",
                        small_mode ? _( "Cap" ) : _( "Capacity" ),
                        format_volume( part->size ), volume_units_abbr() );
    }

    if( part->bonus > 0 ) {
        std::string label;
        if( part->has_flag( VPFLAG_SEATBELT ) ) {
            label = small_mode ? _( "Str" ) : _( "Strength" );
        } else if( part->has_flag( "HORN" ) ) {
            label = _( "Noise" );
        } else if( part->has_flag( "MUFFLER" ) ) {
            label = small_mode ? _( "NoisRed" ) : _( "Noise Reduction" );
        } else if( part->has_flag( VPFLAG_EXTENDS_VISION ) ) {
            label = _( "Range" );
        } else if( part->has_flag( VPFLAG_LIGHT ) || part->has_flag( VPFLAG_CONE_LIGHT ) ||
                   part->has_flag( VPFLAG_WIDE_CONE_LIGHT ) ||
                   part->has_flag( VPFLAG_CIRCLE_LIGHT ) || part->has_flag( VPFLAG_DOME_LIGHT ) ||
                   part->has_flag( VPFLAG_AISLE_LIGHT ) || part->has_flag( VPFLAG_EVENTURN ) ||
                   part->has_flag( VPFLAG_ODDTURN ) || part->has_flag( VPFLAG_ATOMIC_LIGHT ) ) {
            label = _( "Light" );
        }

        if( !label.empty() ) {
            fold_and_print( w_details, point( col_1, line + 3 ), column_width, c_white,
                            "%s: <color_light_gray>%d</color>", label,
                            part->bonus );
        }
    }

    if( part->has_flag( VPFLAG_WHEEL ) ) {
        // Note: there is no guarantee that whl is non-empty!
        const cata::value_ptr<islot_wheel> &whl = item::find_type( part->base_item )->wheel;
        fold_and_print( w_details, point( col_1, line + 3 ), column_width, c_white,
                        "%s: <color_light_gray>%d\"</color>",
                        small_mode ? _( "Dia" ) : _( "Wheel Diameter" ),
                        whl ? whl->diameter : 0 );
        fold_and_print( w_details, point( col_2, line + 3 ), column_width, c_white,
                        "%s: <color_light_gray>%d\"</color>",
                        small_mode ? _( "Wdt" ) : _( "Wheel Width" ),
                        whl ? whl->width : 0 );
    }

    if( part->epower != 0_W ) {
        fold_and_print( w_details, point( col_2, line + 3 ), column_width, c_white,
                        "%s: <color_light_gray>%+4d</color>",
                        small_mode ? _( "Epwr" ) : _( "Electric Power" ),
                        units::to_watt( part->epower ) );
    }

    // line 4 [horizontal]: fuel_type (if applicable)
    // line 4 [vertical/hybrid]: (column 1) fuel_type (if applicable)    (column 2) power (if applicable)
    // line 5 [horizontal]: power (if applicable)
    if( !part->fuel_type.is_null() ) {
        fold_and_print( w_details, point( col_1, line + 4 ), column_width,
                        c_white, _( "Charge: <color_light_gray>%s</color>" ),
                        item::nname( part->fuel_type ) );
    }
    int part_consumption = units::to_watt( part->energy_consumption );
    if( part_consumption != 0 ) {
        fold_and_print( w_details, point( col_2, line + 4 ), column_width, c_white,
                        _( "Drain: <color_light_gray>%+8d</color>" ), -part_consumption );
    }

    // line 5 [vertical/hybrid] flags
    std::vector<std::string> flags = { { "OPAQUE", "OPENABLE", "BOARDABLE" } };
    std::vector<std::string> flag_labels = { { _( "opaque" ), _( "openable" ), _( "boardable" ) } };
    std::string label;
    for( size_t i = 0; i < flags.size(); i++ ) {
        if( part->has_flag( flags[i] ) ) {
            label += ( label.empty() ? "" : " " ) + flag_labels[i];
        }
    }
    // 6 [horizontal]: (column 1) flags    (column 2) battery capacity (if applicable)
    fold_and_print( w_details, point( col_1, line + 5 ), details_w, c_yellow, label );

    if( part->fuel_type == itype_battery && !part->has_flag( VPFLAG_ENGINE ) &&
        !part->has_flag( VPFLAG_ALTERNATOR ) ) {
        const cata::value_ptr<islot_magazine> &battery = item::find_type( part->base_item )->magazine;
        fold_and_print( w_details, point( col_2, line + 5 ), column_width, c_white,
                        "%s: <color_light_gray>%8d</color>",
                        small_mode ? _( "BatCap" ) : _( "Battery Capacity" ),
                        battery->capacity );
    } else {
        units::power part_power = part->power;
        if( part_power != 0_W ) {
            fold_and_print( w_details, point( col_2, line + 5 ), column_width, c_white,
                            _( "Power: <color_light_gray>%+8d</color>" ), units::to_watt( part_power ) );
        }
    }

    wnoutrefresh( w_details );
}

void veh_interact::count_durability()
{
    const vehicle_part_range vpr = veh->get_all_parts();
    int qty = std::accumulate( vpr.begin(), vpr.end(), 0,
    []( int lhs, const vpart_reference & rhs ) {
        return lhs + std::max( rhs.part().base.damage(), rhs.part().base.degradation() );
    } );

    int total = std::accumulate( vpr.begin(), vpr.end(), 0,
    []( int lhs, const vpart_reference & rhs ) {
        return lhs + rhs.part().base.max_damage();
    } );

    int pct = total ? 100 * qty / total : 0;

    if( pct < 5 ) {
        total_durability_text = _( "like new" );
        total_durability_color = c_light_green;

    } else if( pct < 33 ) {
        total_durability_text = _( "dented" );
        total_durability_color = c_yellow;

    } else if( pct < 66 ) {
        total_durability_text = _( "battered" );
        total_durability_color = c_magenta;

    } else if( pct < 100 ) {
        total_durability_text = _( "wrecked" );
        total_durability_color = c_red;

    } else {
        total_durability_text = _( "destroyed" );
        total_durability_color = c_dark_gray;
    }
}

void act_vehicle_siphon( map &here, vehicle *veh )
{
    std::vector<itype_id> fuels;
    bool has_liquid = false;
    // Check all tanks on this vehicle to see if they contain any liquid
    for( const vpart_reference &vp : veh->get_any_parts( VPFLAG_FLUIDTANK ) ) {
        if( vp.part().contains_liquid() ) {
            has_liquid = true;
            break;
        }
    }
    if( !has_liquid ) {
        add_msg( m_info, _( "The vehicle has no liquid fuel left to siphon." ) );
        return;
    }

    std::string title = _( "Select tank to siphon:" );
    auto sel = []( const map &, const vehicle_part & pt ) {
        return pt.contains_liquid();
    };
    if( const std::optional<vpart_reference> tank = veh_interact::select_part( here, *veh, sel,
            title ) ) {
        item liquid( tank->part().get_base().only_item() );
        const int liq_charges = liquid.charges;
        liquid_dest_opt liquid_target;
        if( liquid_handler::handle_liquid( liquid, liquid_target, nullptr, 1, nullptr, veh,
                                           tank->part_index() ) ) {
            veh->drain( here, tank->part_index(), liq_charges - liquid.charges );
            veh->invalidate_mass();
        }
    }
}

void act_vehicle_unload_fuel( map &here, vehicle *veh )
{
    std::vector<itype_id> fuels;
    for( auto &e : veh->fuels_left( ) ) {
        const itype *type = item::find_type( e.first );

        if( e.first == fuel_type_battery || type->phase != phase_id::SOLID ) {
            // This skips battery and plutonium cells
            continue;
        }
        fuels.push_back( e.first );
    }
    if( fuels.empty() ) {
        add_msg( m_info, _( "The vehicle has no solid fuel left to remove." ) );
        return;
    }
    itype_id fuel;
    if( fuels.size() > 1 ) {
        uilist smenu;
        smenu.text = _( "Remove what?" );
        for( auto &fuel : fuels ) {
            if( fuel == itype_plut_cell && veh->fuel_left( here,  fuel ) < PLUTONIUM_CHARGES ) {
                continue;
            }
            smenu.addentry( item::nname( fuel ) );
        }
        smenu.query();
        if( smenu.ret < 0 || static_cast<size_t>( smenu.ret ) >= fuels.size() ) {
            add_msg( m_info, _( "Never mind." ) );
            return;
        }
        fuel = fuels[smenu.ret];
    } else {
        fuel = fuels.front();
    }

    Character &player_character = get_player_character();
    int qty = veh->fuel_left( here, fuel );
    if( fuel == itype_plut_cell ) {
        if( qty / PLUTONIUM_CHARGES == 0 ) {
            add_msg( m_info, _( "The vehicle has no charged plutonium cells." ) );
            return;
        }
        item plutonium( fuel, calendar::turn, qty / PLUTONIUM_CHARGES );
        player_character.i_add( plutonium );
        veh->drain( here, fuel, qty - ( qty % PLUTONIUM_CHARGES ) );
    } else {
        item solid_fuel( fuel, calendar::turn, qty );
        player_character.i_add( solid_fuel );
        veh->drain( here, fuel, qty );
    }

}

/**
 * Called when the activity timer for installing parts, repairing, etc times
 * out and the action is complete.
 */
void veh_interact::complete_vehicle( map &here, Character &you )
{
    if( you.activity.values.size() < 7 ) {
        debugmsg( "ACT_VEHICLE values.size() is %d", you.activity.values.size() );
        return;
    }
    if( you.activity.str_values.empty() ) {
        debugmsg( "ACT_VEHICLE str_values is empty" );
        return;
    }
    const tripoint_abs_ms act_pos( you.activity.values[0], you.activity.values[1], you.posz() );
    optional_vpart_position ovp = here.veh_at( act_pos );
    if( !ovp ) {
        // so the vehicle could have lost some of its parts from other NPCS works
        // during this player/NPCs activity.
        // check the vehicle points that were stored at beginning of activity.
        for( const tripoint_abs_ms &pt : you.activity.coord_set ) {
            ovp = here.veh_at( here.get_bub( pt ) );
            if( ovp ) {
                break;
            }
        }
        // check again, to see if it really is a case of vehicle gone missing.
        if( !ovp ) {
            debugmsg( "Activity ACT_VEHICLE: vehicle not found" );
            return;
        }
    }

    vehicle &veh = ovp->vehicle();
    const point_rel_ms d( you.activity.values[4], you.activity.values[5] );
    const vpart_id part_id( you.activity.str_values[0] );
    const vpart_info &vpinfo = part_id.obj();

    // cmd = Install Repair reFill remOve Siphon Unload reName relAbel
    switch( static_cast<char>( you.activity.index ) ) {
        case 'i': {
            const inventory &inv = you.crafting_inventory();
            const requirement_data reqs = vpinfo.install_requirements();
            if( !reqs.can_make_with_inventory( inv, is_crafting_component, 1, craft_flags::none, false ) ) {
                you.add_msg_player_or_npc( m_info,
                                           _( "You don't meet the requirements to install the %s." ),
                                           _( "<npcname> doesn't meet the requirements to install the %s." ),
                                           vpinfo.name() );
                break;
            }

            // consume items extracting a match for the parts base item
            item base;
            std::vector<item> installed_with;
            for( const std::vector<item_comp> &e : reqs.get_components() ) {
                for( item &obj : you.consume_items( e, 1, is_crafting_component, [&vpinfo]( const itype_id & itm ) {
                return itm == vpinfo.base_item;
            } ) ) {
                    if( obj.typeId() == vpinfo.base_item ) {
                        base = obj;
                    } else {
                        installed_with.push_back( obj );
                    }
                }
            }
            if( base.is_null() ) {
                if( !you.has_trait( trait_DEBUG_HS ) ) {
                    add_msg( m_info, _( "Could not find base part in requirements for %s." ), vpinfo.name() );
                    break;
                } else {
                    base = item( vpinfo.base_item );
                }
            }

            for( const auto &e : reqs.get_tools() ) {
                you.consume_tools( e );
            }

            you.invalidate_crafting_inventory();
            const int partnum = veh.install_part( here, d, part_id, std::move( base ), installed_with );
            if( partnum < 0 ) {
                debugmsg( "complete_vehicle install part fails dx=%d dy=%d id=%s",
                          d.x(), d.y(), part_id.c_str() );
                break;
            }
            ::vehicle_part &vp_new = veh.part( partnum );
            if( vp_new.info().variants.size() > 1 ) {
                do_change_shape_menu( vp_new );
            }

            // Need map-relative coordinates to compare to output of look_around.
            // Need to call coord_translate() directly since it's a new part.
            const point_rel_ms q = veh.coord_translate( d );

            if( vpinfo.has_flag( VPFLAG_CONE_LIGHT ) ||
                vpinfo.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ||
                vpinfo.has_flag( VPFLAG_HALF_CIRCLE_LIGHT ) ) {
                orient_part( here, &veh, vpinfo, partnum, q );
            }

            const tripoint_bub_ms vehp = veh.pos_bub( here ) + tripoint_rel_ms( q, 0 );
            // TODO: allow boarding for non-players as well.
            Character *const pl = get_creature_tracker().creature_at<Character>( vehp );
            if( vpinfo.has_flag( VPFLAG_BOARDABLE ) && pl ) {
                here.board_vehicle( vehp, pl );
            }

            you.add_msg_if_player( m_good, _( "You install a %1$s into the %2$s." ), vp_new.name(), veh.name );

            for( const auto &sk : vpinfo.install_skills ) {
                you.practice( sk.first, veh_utils::calc_xp_gain( vpinfo, sk.first, you ) );
            }
            here.add_vehicle_to_cache( &veh );
            break;
        }

        case 'r': {
            vehicle_part &vp = veh.part( you.activity.values[6] );
            veh_utils::repair_part( here, veh, vp, you );
            break;
        }

        case 'f': {
            if( you.activity.targets.empty() || !you.activity.targets.front() ) {
                debugmsg( "Activity ACT_VEHICLE: missing refill source" );
                break;
            }

            item_location &src = you.activity.targets.front();
            vehicle_part &vp = veh.part( you.activity.values[6] );
            if( vp.is_tank() && src->is_container() && !src->empty() ) {
                item_location contained( src, &src->only_item() );
                contained->charges -= vp.base.fill_with( *contained, contained->charges );

                contents_change_handler handler;
                handler.unseal_pocket_containing( contained );

                // if code goes here, we can assume "vp" has already refilled with "contained" something.
                int remaining_ammo_capacity = vp.ammo_capacity( contained->ammo_type() ) - vp.ammo_remaining(
                                              );

                if( remaining_ammo_capacity ) {
                    //~ 1$s vehicle name, 2$s tank name
                    you.add_msg_if_player( m_good, _( "You refill the %1$s's %2$s." ), veh.name, vp.name() );
                } else {
                    //~ 1$s vehicle name, 2$s tank name
                    you.add_msg_if_player( m_good, _( "You completely refill the %1$s's %2$s." ), veh.name, vp.name() );
                }

                if( contained->charges == 0 ) {
                    contained.remove_item();
                } else {
                    you.add_msg_if_player( m_good, _( "There's some left over!" ) );
                }

                handler.handle_by( you );
            } else if( vp.is_fuel_store() ) {
                contents_change_handler handler;
                handler.unseal_pocket_containing( src );

                int qty = src->charges;
                vp.base.reload( you, std::move( src ), qty );

                //~ 1$s vehicle name, 2$s reactor name
                you.add_msg_if_player( m_good, _( "You refuel the %1$s's %2$s." ), veh.name, vp.name() );

                handler.handle_by( you );
            } else {
                debugmsg( "vehicle part is not reloadable" );
                break;
            }

            veh.invalidate_mass();
            break;
        }

        case 'O': // 'O' = remove appliance
        case 'o': {
            int vp_index = you.activity.values[6];
            if( vp_index >= veh.part_count() ) {
                vp_index = veh.get_next_shifted_index( vp_index, you );
                if( vp_index == -1 ) {
                    you.add_msg_if_player( m_info,
                                           //~ 1$s is the vehicle part name
                                           _( "The %1$s has already been removed by someone else." ),
                                           vpinfo.name() );
                    return;
                }
            }
            vehicle_part *vp = &veh.part( vp_index );
            const vpart_info &vpi = vp->info();
            const bool appliance_removal = static_cast<char>( you.activity.index ) == 'O';
            const bool wall_wire_removal = appliance_removal && vpi.id == vpart_ap_wall_wiring;
            const bool broken = vp->is_broken();
            const bool smash_remove = vpi.has_flag( "SMASH_REMOVE" );
            const inventory &inv = you.crafting_inventory();
            const requirement_data &reqs = vpi.removal_requirements();
            if( !reqs.can_make_with_inventory( inv, is_crafting_component ) ) {
                //~  1$s is the vehicle part name
                add_msg( m_info, _( "You don't meet the requirements to remove the %1$s." ), vpi.name() );
                break;
            }
            for( const auto &e : reqs.get_components() ) {
                you.consume_items( e, 1, is_crafting_component );
            }
            for( const auto &e : reqs.get_tools() ) {
                you.consume_tools( e );
            }

            you.invalidate_crafting_inventory();

            // This will be a list of all the items which arise from this removal.
            std::list<item> resulting_items;

            // First we get all the contents of the part
            vehicle_stack contents = veh.get_items( *vp );
            resulting_items.insert( resulting_items.end(), contents.begin(), contents.end() );
            contents.clear();

            if( broken ) {
                you.add_msg_if_player( _( "You remove the broken %1$s from the %2$s." ), vp->name(), veh.name );
            } else if( smash_remove ) {
                you.add_msg_if_player( _( "You smash the %1$s to bits, removing it from the %2$s." ),
                                       vp->name(), veh.name );
            } else {
                you.add_msg_if_player( _( "You remove the %1$s from the %2$s." ), vp->name(), veh.name );
            }

            if( wall_wire_removal ) {
                veh.part_to_item( here, *vp ); // what's going on here? this line isn't doing anything...
            } else if( vpi.has_flag( "TOW_CABLE" ) ) {
                veh.invalidate_towing( here, true, &you );
            } else if( broken ) {
                item_group::ItemList pieces = vp->pieces_for_broken_part();
                resulting_items.insert( resulting_items.end(), pieces.begin(), pieces.end() );
            } else {
                if( smash_remove ) {
                    item_group::ItemList pieces = vp->pieces_for_broken_part();
                    resulting_items.insert( resulting_items.end(), pieces.begin(), pieces.end() );
                } else {
                    resulting_items.push_back( veh.removed_part( here, *vp ) );

                    // damage reduces chance of success (0.8^damage_level)
                    const double component_success_chance = std::pow( 0.8, vp->damage_level() );
                    const double charges_min = std::clamp( component_success_chance, 0.0, 1.0 );
                    const double charges_max = std::clamp( component_success_chance + 0.1, 0.0, 1.0 );
                    for( item &it : vp->get_salvageable() ) {
                        if( it.count_by_charges() ) {
                            const int charges_befor = it.charges;
                            it.charges *= rng_float( charges_min, charges_max );
                            const int charges_destroyed = charges_befor - it.charges;
                            if( charges_destroyed > 0 ) {
                                you.add_msg_player_or_npc( m_bad,
                                                           _( "You fail to recover %1$d %2$s." ),
                                                           _( "<npcname> fails to recover %1$d %2$s." ),
                                                           charges_destroyed,
                                                           it.type_name( charges_destroyed ) );
                            }
                            if( it.charges > 0 ) {
                                resulting_items.push_back( it );
                            }
                        } else if( component_success_chance > rng_float( 0, 1 ) ) {
                            resulting_items.push_back( it );
                        } else {
                            you.add_msg_player_or_npc( m_bad,
                                                       _( "You fail to recover %1$s." ),
                                                       _( "<npcname> fails to recover %1$s." ),
                                                       it.type_name() );
                        }
                    }
                }
                for( const std::pair<const skill_id, int> &sk : vpi.install_skills ) {
                    // removal is half as educational as installation
                    you.practice( sk.first, veh_utils::calc_xp_gain( vpi, sk.first, you ) / 2 );
                }
            }

            // Power cables must remove parts from the target vehicle, too.
            if( vpi.has_flag( VPFLAG_POWER_TRANSFER ) ) {
                veh.remove_remote_part( here, *vp );
            }

            if( appliance_removal && veh.part_count() > 1 ) {
                // Split up power grids
                veh.find_and_split_vehicles( here, { vp_index } );
                veh.part_removal_cleanup( here );
                // Ensure the position, pivot, and precalc points are up-to-date
                veh.pos -= veh.pivot_anchor[0];
                veh.precalc_mounts( 0, veh.turn_dir, point_rel_ms::zero );
                here.rebuild_vehicle_level_caches();

                if( auto newpart = here.veh_at( act_pos ).part_with_feature( VPFLAG_APPLIANCE, false ) ) {
                    vp = &newpart->part();
                } else {
                    debugmsg( "No appliance part left to remove after splitting vehicle!" );
                    vp = nullptr;
                }
                //always stop after removing an appliance
                you.activity.set_to_null();
            }

            // Save these values now so they aren't lost when parts or vehicles are destroyed.
            const point_rel_ms part_mount = vp->mount;
            const tripoint_bub_ms part_pos = veh.bub_part_pos( here, *vp );

            veh.unlink_cables( here, part_mount, you,
                               false, /* unneeded as items will be unlinked if the connected part is removed */
                               appliance_removal || vpi.location == "structure",
                               appliance_removal || vpi.has_flag( VPFLAG_CABLE_PORTS ) || vpi.has_flag( VPFLAG_BATTERY ) );

            if( veh.part_count_real() <= 1 ) {
                you.add_msg_if_player( _( "You completely dismantle the %s." ), veh.name );
                you.activity.set_to_null();
                // destroy vehicle clears the cache
                here.destroy_vehicle( &veh );
            } else if( vp ) {
                veh.remove_part( *vp );
                // part_removal_cleanup calls refresh, so parts_at_relative is valid
                veh.part_removal_cleanup( here );
                if( veh.parts_at_relative( part_mount, true ).empty() ) {
                    here.clear_vehicle_point_from_cache( &veh, part_pos );
                }
            }
            // This will be part of an NPC "job" where they need to clean up the activity
            // items afterwards
            if( you.is_npc() ) {
                for( item &it : resulting_items ) {
                    it.set_var( "activity_var", you.name );
                }
            }
            // Finally, put all the results somewhere (we wanted to wait until this
            // point because we don't want to put them back into the vehicle part
            // that just got removed).
            std::vector<item_location> locs = put_into_vehicle_or_drop_ret_locs( you,
                                              item_drop_reason::deliberate,
                                              resulting_items );
            if( you.is_npc() ) {
                for( const item_location &itl : locs ) {
                    you.may_activity_occupancy_after_end_items_loc.push_back( itl );
                }
            }
            break;
        }
        case 'u': {
            // Unplug action just sheds loose connections,
            // assuming vehicle::shed_loose_parts was already called so that
            // the removed parts have had time to be processed
            you.add_msg_if_player( _( "You disconnect the %s's power connection." ), veh.name );
            break;
        }
    }
    you.invalidate_crafting_inventory();
    you.invalidate_weight_carried_cache();
}

static std::string access_task_name( const int task )
{
    static const std::array<translation, 9> names = { {
            to_translation( "vehicle task", "Repair" ),
            to_translation( "vehicle task", "Mend" ),
            to_translation( "vehicle task", "Remove" ),
            to_translation( "vehicle task", "Install here" ),
            to_translation( "vehicle task", "Refill" ),
            to_translation( "vehicle task", "Siphon" ),
            to_translation( "vehicle task", "Assign crew" ),
            to_translation( "vehicle task", "Change shape" ),
            to_translation( "vehicle task", "Label" )
        }
    };
    return names.at( task ).translated();
}

/** Why a vehicle task cannot start, in words, or empty for reasons the caller names. */
static std::string task_reason_text( const task_reason reason )
{
    switch( reason ) {
        case task_reason::MOVING_VEHICLE:
            return _( "the vehicle is moving" );
        case task_reason::LOW_MORALE:
            return _( "your morale is too low" );
        case task_reason::LOW_LIGHT:
            return _( "it is too dark to see what you are doing" );
        case task_reason::LACK_TOOLS:
            return _( "you lack the tools" );
        case task_reason::NOT_FREE:
            return _( "something is attached to it" );
        case task_reason::LACK_SKILL:
            return _( "your skill is too low" );
        default:
            break;
    }
    return std::string();
}

/** Unmet requirements in words: what blocks the task, or how many are missing. */
static std::string requirement_reason( const std::string &blocked, const size_t missing )
{
    return blocked.empty() ? accessibility::format_vehicle_missing_requirements( missing ) : blocked;
}

/** The description of a part type as spoken sentences, one per source line. */
static std::vector<std::string> description_lines( const vpart_info &info )
{
    std::string description;
    info.format_description( description, c_light_gray, 0 );
    std::vector<std::string> lines;
    std::istringstream stream( remove_color_tags( description ) );
    std::string line;
    while( std::getline( stream, line ) ) {
        const std::string sentence = accessibility::format_semantic_sequence( { line } );
        if( !sentence.empty() ) {
            lines.push_back( sentence );
        }
    }
    return lines;
}

void veh_interact::focus_part( map &here, const int part )
{
    move_cursor( here, ( veh->part( part ).mount.raw() + dd ).rotate( 3 ) );
}

std::string veh_interact::cursor_description() const
{
    return accessibility_gameplay::vehicle_tile_description( *veh, -dd, cursor_obstructed );
}

void veh_interact::speak_grid()
{
    accessibility::announce( accessibility::format_semantic_sequence( {
        _( "Vehicle grid" ), cursor_description()
    } ) );
}

void veh_interact::show_requirements( const std::string &title, const std::string &reason,
                                      const task_speech &speech ) const
{
    std::vector<std::string> lines;
    if( !reason.empty() ) {
        lines.push_back( accessibility::format_semantic_sequence( {
            string_format( _( "Unavailable: %s" ), reason )
        } ) );
    }
    lines.insert( lines.end(), speech.requirements.begin(), speech.requirements.end() );
    if( lines.empty() ) {
        lines.emplace_back( _( "No requirements." ) );
    }
    accessibility_ui::show_document( title, title, lines, lines );
}

std::string veh_interact::task_unavailable( map &here, const access_task task, const int part,
        task_speech &speech )
{
    avatar &you = get_avatar();
    vehicle_part &pt = veh->part( part );
    // Reasons that apply to the whole vehicle; the caller names an invalid target.
    const auto mode_reason = [&]( const char mode ) {
        const task_reason reason = cant_do( here, mode );
        return reason == task_reason::INVALID_TARGET ? std::string() : task_reason_text( reason );
    };
    //~ Why a vehicle task is unavailable: %s is the owner of the vehicle.
    const std::string not_owned = string_format( _( "owned by %s" ), veh->get_owner_name() );
    std::string reason;
    switch( task ) {
        case access_task::repair: {
            if( !pt.is_repairable() && !pt.is_broken() ) {
                return pt.damage_percent() > 0 ? _( "this part cannot be repaired" ) : _( "not damaged" );
            }
            reason = mode_reason( 'r' );
            std::string nmsg;
            if( !repair_requirements( here, pt, nmsg, &speech ) && reason.empty() ) {
                reason = requirement_reason( speech.blocked, speech.missing );
            }
            break;
        }
        case access_task::mend:
            reason = mode_reason( 'm' );
            break;
        case access_task::remove: {
            reason = mode_reason( 'o' );
            const bool ok = can_remove_part( here, part, you, &speech );
            if( !ok && reason.empty() ) {
                reason = requirement_reason( speech.blocked, speech.missing );
            }
            break;
        }
        case access_task::install: {
            const task_reason install = cant_do( here, 'i' );
            reason = install == task_reason::INVALID_TARGET ? _( "nothing can be installed here" ) :
                     task_reason_text( install );
            break;
        }
        case access_task::refill:
            reason = mode_reason( 'f' );
            break;
        case access_task::siphon: {
            const task_reason siphon = cant_do( here, 's' );
            reason = siphon == task_reason::LACK_TOOLS ? _( "you need a hose" ) : mode_reason( 's' );
            break;
        }
        case access_task::crew:
            if( !owned_by_player ) {
                reason = not_owned;
            } else if( cant_do( here, 'w' ) != task_reason::CAN_DO ) {
                reason = _( "you have no allies to assign" );
            }
            break;
        case access_task::shape:
            break;
        case access_task::label:
            if( !owned_by_player ) {
                reason = not_owned;
            }
            break;
    }
    msg.reset();
    return reason;
}

std::vector<veh_interact::access_option> veh_interact::part_tasks( map &here, const int part )
{
    const vehicle_part &pt = veh->part( part );
    const bool debug_faults = get_avatar().has_trait( trait_DEBUG_HS );
    std::vector<access_task> tasks = { access_task::repair };
    if( pt.is_available() && ( debug_faults ? !pt.faults_potential().empty() :
                               !pt.faults().empty() ) ) {
        tasks.push_back( access_task::mend );
    }
    tasks.push_back( access_task::remove );
    tasks.push_back( access_task::install );
    if( can_refill( here, pt ) ) {
        tasks.push_back( access_task::refill );
    }
    if( pt.is_tank() && !pt.base.empty() && pt.base.only_item().made_of( phase_id::LIQUID ) ) {
        tasks.push_back( access_task::siphon );
    }
    if( pt.is_seat() ) {
        tasks.push_back( access_task::crew );
    }
    if( pt.info().variants.size() > 1 ) {
        tasks.push_back( access_task::shape );
    }
    tasks.push_back( access_task::label );
    std::vector<access_option> options;
    options.reserve( tasks.size() );
    for( const access_task task : tasks ) {
        options.push_back( { task, part, access_task_name( static_cast<int>( task ) ) } );
    }
    return options;
}

void veh_interact::evaluate_tasks( map &here, const std::vector<access_option> &options,
                                   std::vector<std::string> &reasons, std::vector<task_speech> &speech )
{
    reasons.clear();
    speech.assign( options.size(), task_speech() );
    for( size_t i = 0; i < options.size(); ++i ) {
        focus_part( here, options[i].part );
        reasons.push_back( task_unavailable( here, options[i].task, options[i].part, speech[i] ) );
    }
}

int veh_interact::accessible_task_menu( map &here, const std::string &title,
                                        const std::vector<access_option> &options, const bool reopen )
{
    if( options.empty() ) {
        return -1;
    }
    std::vector<std::string> reasons;
    std::vector<task_speech> speech;
    evaluate_tasks( here, options, reasons, speech );
    const auto option_title = [&]( const size_t index ) {
        //~ Vehicle task requirement document: %1$s is the task, %2$s the part.
        return string_format( _( "%1$s %2$s" ), access_task_name( static_cast<int>( options[index].task ) ),
                              veh->part( options[index].part ).name( false ) );
    };

    uilist menu;
    menu.title = title;
    menu.allow_disabled = true;
    for( size_t i = 0; i < options.size(); ++i ) {
        menu.addentry_col( static_cast<int>( i ), reasons[i].empty(), MENU_AUTOASSIGN, options[i].label,
                           reasons[i] );
        menu.entries.back().accessibility_focus = accessibility::format_vehicle_task( options[i].label,
                reasons[i] );
    }
    menu.accessibility_details = [&]() {
        if( menu.selected < 0 || static_cast<size_t>( menu.selected ) >= menu.entries.size() ) {
            return;
        }
        const size_t index = static_cast<size_t>( menu.entries[menu.selected].retval );
        show_requirements( option_title( index ), reasons[index], speech[index] );
    };
    if( reopen ) {
        menu.accessibility_reopen();
    }
    while( true ) {
        menu.query();
        if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= options.size() ) {
            return -1;
        }
        const size_t chosen = static_cast<size_t>( menu.ret );
        if( reasons[chosen].empty() ) {
            focus_part( here, options[chosen].part );
            return menu.ret;
        }
        accessibility::announce( accessibility::format_vehicle_task_blocked(
                                     access_task_name( static_cast<int>( options[chosen].task ) ),
                                     veh->part( options[chosen].part ).name( false ), reasons[chosen] ) );
        menu.accessibility_resume();
    }
}

bool veh_interact::run_task( map &here, const access_task task, const int part )
{
    Character &you = get_player_character();
    focus_part( here, part );
    vehicle_part &pt = veh->part( part );
    const auto announce_helpers = [&you]() {
        for( const Character *helper : you.get_crafting_helpers() ) {
            add_msg( m_info, _( "%s helps with this task…" ), helper->get_name() );
        }
    };
    switch( task ) {
        case access_task::repair:
            // Modifying a vehicle with rotors will make it not flightworthy.
            if( veh->would_repair_prevent_flyable( pt, you ) ) {
                if( !query_yn(
                        _( "Repairing this part will mean that this vehicle is no longer flightworthy.  Continue?" ) ) ) {
                    break;
                }
                veh->set_flyable( false );
            }
            announce_helpers();
            sel_vehicle_part = &pt;
            sel_vpart_info = &pt.info();
            sel_cmd = 'r';
            break;
        case access_task::remove:
            if( veh->would_removal_prevent_flyable( pt, you ) ) {
                if( !query_yn(
                        _( "Removing this part will mean that this vehicle is no longer flightworthy.  Continue?" ) ) ) {
                    break;
                }
                veh->set_flyable( false );
            }
            announce_helpers();
            sel_vehicle_part = &pt;
            sel_vpart_info = &pt.info();
            sel_cmd = 'o';
            veh->recalculate_enchantment_cache();
            break;
        case access_task::install:
            do_install( here );
            break;
        case access_task::mend:
            preselected_part = part;
            do_mend( here );
            break;
        case access_task::refill:
            preselected_part = part;
            do_refill( here );
            break;
        case access_task::siphon:
            preselected_part = part;
            do_siphon( here );
            break;
        case access_task::crew:
            preselected_part = part;
            do_assign_crew( here );
            break;
        case access_task::shape:
            do_change_shape_menu( pt );
            break;
        case access_task::label:
            do_relabel( here );
            break;
    }
    preselected_part = -1;
    if( msg ) {
        accessibility::announce( accessibility::format_semantic_sequence( { remove_color_tags( *msg ) } ) );
        msg.reset();
    }
    if( task == access_task::siphon ) {
        // Siphoning may have started an activity that continues after the screen closes.
        if( !you.activity.is_null() ) {
            return true;
        }
        cache_tool_availability();
    }
    return sel_cmd != ' ';
}

void veh_interact::accessible_choose_part( map &here, const access_task task,
        const std::vector<int> &parts )
{
    std::vector<access_option> options;
    options.reserve( parts.size() );
    for( const int part : parts ) {
        options.push_back( { task, part, accessibility::format_vehicle_tile_part(
                                 accessibility_gameplay::vehicle_part_row( *veh, veh->part( part ) ) )
                           } );
    }
    const std::string menu_title = task == access_task::repair ? _( "Choose a part here to repair:" ) :
                                   _( "Choose a part here to remove:" );
    const int choice = accessible_task_menu( here, menu_title, options, false );
    if( choice >= 0 ) {
        run_task( here, task, options[choice].part );
    }
}

bool veh_interact::accessible_part_tasks( map &here, const int part )
{
    Character &you = get_player_character();
    bool reopen = false;
    while( true ) {
        focus_part( here, part );
        const std::vector<access_option> options = part_tasks( here, part );
        const accessibility::vehicle_part_row row = accessibility_gameplay::vehicle_part_row( *veh,
                veh->part( part ) );
        //~ Task menu of a vehicle part: %1$s is the part, %2$s its location such as "front left".
        const std::string menu_title = string_format( _( "Tasks for %1$s, %2$s" ), row.name,
                                       row.location );
        const int choice = accessible_task_menu( here, menu_title, options, reopen );
        if( choice < 0 ) {
            return false;
        }
        reopen = true;
        const access_task task = options[choice].task;
        const bool theft_check = task != access_task::crew && task != access_task::shape &&
                                 task != access_task::label;
        if( theft_check && !veh->handle_potential_theft( you ) ) {
            continue;
        }
        if( run_task( here, task, part ) ) {
            return true;
        }
    }
}

bool veh_interact::accessible_part_list( map &here )
{
    int focused = -1;
    bool reopen = false;
    while( true ) {
        const std::vector<int> parts = accessibility_gameplay::vehicle_part_list( *veh );
        if( parts.empty() ) {
            return false;
        }
        uilist menu;
        //~ Accessible list of the parts of the examined vehicle: %s is its name.
        menu.title = string_format( _( "Parts of %s" ), veh->name );
        menu.accessibility_count = []( const size_t count ) {
            return accessibility::format_vehicle_part_count( count );
        };
        for( size_t i = 0; i < parts.size(); ++i ) {
            const accessibility::vehicle_part_row row = accessibility_gameplay::vehicle_part_row( *veh,
                    veh->part( parts[i] ) );
            menu.addentry_col( static_cast<int>( i ), true, std::nullopt, row.name, row.location );
            menu.entries.back().accessibility_focus = accessibility::format_vehicle_part_row( row );
            if( parts[i] == focused ) {
                menu.selected = static_cast<int>( i );
            }
        }
        menu.accessibility_details = [&]() {
            if( menu.selected < 0 || static_cast<size_t>( menu.selected ) >= menu.entries.size() ) {
                return;
            }
            const vehicle_part &pt = veh->part( parts[menu.entries[menu.selected].retval] );
            const std::string name = pt.name( false );
            const std::vector<std::string> lines = accessibility_gameplay::vehicle_part_details( *veh, pt );
            accessibility_ui::show_document( name, name, lines, lines );
        };
        if( reopen ) {
            menu.accessibility_reopen();
        }
        menu.query();
        if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= parts.size() ) {
            return false;
        }
        focused = parts[menu.ret];
        if( accessible_part_tasks( here, focused ) ) {
            return true;
        }
        reopen = true;
    }
}

void veh_interact::accessible_install( map &here )
{
    avatar &player_character = get_avatar();
    const std::string location = accessibility::format_vehicle_location(
                                     accessibility_gameplay::vehicle_grid_position( *veh, -dd ) );
    uilist menu;
    menu.title = _( "Choose new part to install here:" );
    //~ Accessible install list: %s is the location on the vehicle, such as "front left".
    menu.accessibility_context = string_format( _( "Install at %s" ), location );
    menu.allow_disabled = true;
    menu.accessibility_count = []( const size_t count ) {
        return accessibility::format_vehicle_install_count( count );
    };
    for( const vpart_category &cat : vpart_category::all() ) {
        if( cat.get_id() != "_filter" ) {
            menu.add_category( cat.get_id(), cat.name() );
        }
    }
    menu.set_category_filter( [this]( const uilist_entry & entry, const std::string & key ) {
        return key == "_all" || can_mount[entry.retval]->has_category( key );
    } );
    for( size_t i = 0; i < can_mount.size(); ++i ) {
        const vpart_info &info = *can_mount[i];
        // The visual list grays out parts whose requirements are not met.
        const bool potential = can_potentially_install( info );
        menu.addentry( static_cast<int>( i ), potential, std::nullopt, info.name() );
        menu.entries.back().accessibility_focus = accessibility::format_vehicle_task( info.name(),
                potential ? std::string() : accessibility::format_vehicle_missing_requirements( 0 ) );
    }
    const auto requirements = [&]( const int index, task_speech & speech ) {
        sel_vpart_info = can_mount[index];
        const bool ok = update_part_requirements( here, &speech );
        msg.reset();
        return ok;
    };
    menu.accessibility_details = [&]() {
        if( menu.selected < 0 || static_cast<size_t>( menu.selected ) >= menu.entries.size() ) {
            return;
        }
        const int index = menu.entries[menu.selected].retval;
        task_speech speech;
        const bool ok = requirements( index, speech );
        const std::vector<std::string> description = description_lines( *can_mount[index] );
        speech.requirements.insert( speech.requirements.end(), description.begin(), description.end() );
        //~ Install requirement document: %s is the part.
        show_requirements( string_format( _( "Install %s" ), can_mount[index]->name() ),
                           ok ? std::string() : requirement_reason( speech.blocked, speech.missing ), speech );
    };
    while( true ) {
        menu.query();
        if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= can_mount.size() ) {
            sel_vpart_info = nullptr;
            return;
        }
        task_speech speech;
        if( requirements( menu.ret, speech ) ) {
            break;
        }
        accessibility::announce( accessibility::format_vehicle_task_blocked(
                                     pgettext( "vehicle task", "Install" ), sel_vpart_info->name(),
                                     requirement_reason( speech.blocked, speech.missing ) ) );
        menu.accessibility_resume();
    }
    switch( cant_do( here, 'i' ) ) {
        case task_reason::LOW_MORALE:
            msg = _( "Your morale is too low to construct…" );
            return;
        case task_reason::LOW_LIGHT:
            msg = _( "It's too dark to see what you are doing…" );
            return;
        case task_reason::MOVING_VEHICLE:
            msg = _( "You can't install parts while driving." );
            return;
        default:
            break;
    }
    // Modifying a vehicle with rotors will make it not flightworthy.
    if( veh->would_install_prevent_flyable( *sel_vpart_info, player_character ) ) {
        if( !query_yn( _( "Installing this part will mean that this vehicle is no longer "
                          "flightworthy.  Continue?" ) ) ) {
            return;
        }
        veh->set_flyable( false );
    }
    if( veh->is_foldable() && !sel_vpart_info->folded_volume &&
        !query_yn( _( "Installing this part will make the vehicle unfoldable. "
                      " Continue?" ) ) ) {
        return;
    }
    sel_cmd = 'i';
}

void veh_interact::accessible_overview( map &here )
{
    if( preselected_part >= 0 ) {
        vehicle_part &pt = veh->part( preselected_part );
        if( overview_enable( here, pt ) ) {
            overview_action( here, pt );
        }
        return;
    }
    calc_overview( here );
    std::vector<vehicle_part *> parts;
    for( const part_option &option : overview_opts ) {
        if( option.selectable ) {
            parts.push_back( option.part );
        }
    }
    if( parts.empty() ) {
        return;
    }
    uilist menu;
    menu.title = title.value_or( _( "Select part" ) );
    menu.accessibility_count = []( const size_t count ) {
        return accessibility::format_vehicle_part_count( count );
    };
    for( size_t i = 0; i < parts.size(); ++i ) {
        const accessibility::vehicle_part_row row = accessibility_gameplay::vehicle_part_row( *veh,
                *parts[i] );
        menu.addentry_col( static_cast<int>( i ), true, MENU_AUTOASSIGN, row.name, row.location );
        menu.entries.back().accessibility_focus = accessibility::format_vehicle_part_row( row );
    }
    menu.accessibility_details = [&]() {
        if( menu.selected < 0 || static_cast<size_t>( menu.selected ) >= menu.entries.size() ) {
            return;
        }
        const vehicle_part &pt = *parts[menu.entries[menu.selected].retval];
        const std::string name = pt.name( false );
        const std::vector<std::string> lines = accessibility_gameplay::vehicle_part_details( *veh, pt );
        accessibility_ui::show_document( name, name, lines, lines );
    };
    menu.query();
    if( menu.ret >= 0 && static_cast<size_t>( menu.ret ) < parts.size() ) {
        vehicle_part &pt = *parts[menu.ret];
        focus_part( here, veh->index_of_part( &pt ) );
        overview_action( here, pt );
    }
}

bool veh_interact::accessible_grid_action( map &here, const std::string &action, bool &finish )
{
    if( action == "ACCESSIBILITY_PART_LIST" ) {
        finish = accessible_part_list( here );
        if( !finish ) {
            speak_grid();
        }
    } else if( action == "ACCESSIBILITY_STATUS" ) {
        accessibility::announce( accessible_stats( here ) );
    } else if( action == "ACCESSIBILITY_DETAILS" ) {
        const std::string location = accessibility::format_vehicle_location(
                                         accessibility_gameplay::vehicle_grid_position( *veh, -dd ) );
        const std::vector<std::string> lines = accessibility_gameplay::vehicle_tile_details( *veh, -dd );
        accessibility_ui::show_document( location, location, lines, lines );
        accessibility::announce( cursor_description() );
    } else if( action == "ACCESSIBILITY_REPEAT_LAST" ) {
        accessibility::repeat_last_announcement();
    } else {
        return false;
    }
    return true;
}

void veh_interact::accessible_after_action( map &, const std::string &action )
{
    static const std::set<std::string> task_actions = {
        "INSTALL", "REPAIR", "MEND", "REFILL", "REMOVE", "SIPHON", "UNLOAD", "ASSIGN_CREW",
        "RELABEL", "RENAME"
    };
    if( !main_context.get_direction_rel_ms( action ) && task_actions.count( action ) == 0 ) {
        return;
    }
    if( msg ) {
        accessibility::announce( accessibility::format_semantic_sequence( { remove_color_tags( *msg ) } ) );
    } else {
        accessibility::announce( cursor_description() );
    }
}

std::string veh_interact::accessible_stats( map &here ) const
{
    accessibility::vehicle_workshop_stats stats;
    stats.name = veh->name;
    if( !veh->is_owned_by( get_player_character(), true ) ) {
        stats.owner = veh->get_owner_name();
    }
    stats.units = velocity_units( VU_VEHICLE );
    const auto vel_to_int = []( const double vel ) {
        return static_cast<int>( convert_velocity( vel, VU_VEHICLE ) );
    };
    const bool is_boat = !veh->floating.empty();
    const bool is_ground = !veh->wheelcache.empty() || !is_boat;
    if( veh->is_rotorcraft( here ) && veh->is_flying_in_air() ) {
        stats.speeds.push_back( { _( "In the air" ), vel_to_int( veh->safe_rotor_velocity( here, false ) ),
                                  vel_to_int( veh->max_rotor_velocity( here, false ) ),
                                  vel_to_int( veh->rotor_acceleration( here, false ) )
                                } );
    } else {
        if( is_ground ) {
            stats.speeds.push_back( { std::string(), vel_to_int( veh->safe_ground_velocity( here, false ) ),
                                      vel_to_int( veh->max_ground_velocity( here, false ) ),
                                      vel_to_int( veh->ground_acceleration( here, false ) )
                                    } );
        }
        if( is_boat ) {
            stats.speeds.push_back( { _( "On water" ), vel_to_int( veh->safe_water_velocity( here, false ) ),
                                      vel_to_int( veh->max_water_velocity( here, false ) ),
                                      vel_to_int( veh->water_acceleration( here, false ) )
                                    } );
        }
    }
    stats.mass = string_format( "%.0f %s", convert_weight( veh->total_mass( here ) ), weight_units() );
    units::volume total_cargo = 0_ml;
    units::volume free_cargo = 0_ml;
    for( const vpart_reference &vpr : veh->get_any_parts( VPFLAG_CARGO ) ) {
        const vehicle_stack vs = vpr.items();
        total_cargo += vs.max_volume();
        free_cargo += vs.free_volume();
    }
    stats.cargo_used = string_format( "%s %s", format_volume( total_cargo - free_cargo ),
                                      volume_units_abbr() );
    stats.cargo_total = string_format( "%s %s", format_volume( total_cargo ), volume_units_abbr() );
    stats.status = remove_color_tags( total_durability_text );
    stats.wheels = remove_color_tags( wheel_state_description( here, *veh ) );
    vehicle_part *most_damaged = get_most_damaged_part();
    vehicle_part *most_repairable = get_most_repairable_part();
    if( most_damaged && most_damaged->damage_percent() ) {
        stats.most_damaged = most_damaged->name( false );
        stats.most_damaged_repairable = most_damaged == most_repairable;
    }
    if( most_repairable && most_repairable != most_damaged ) {
        stats.needs_repair = most_repairable->name( false );
    }
    stats.fuels = accessibility_gameplay::vehicle_status( here, *veh ).fuels;
    stats.air_drag = veh->coeff_air_drag();
    stats.has_water_drag = is_boat;
    stats.water_drag = is_boat ? veh->coeff_water_drag( here ) : 0.0;
    stats.has_rolling_drag = is_ground;
    stats.rolling_drag = is_ground ? veh->coeff_rolling_drag( here ) : 0.0;
    stats.static_drag = units::to_watt( veh->static_drag( false ) );
    stats.offroad = static_cast<int>( veh->k_traction( here, veh->wheel_area() *
                                      veh->average_offroad_rating() ) * 100 );
    if( is_boat ) {
        stats.has_draft = true;
        stats.draft = veh->water_draft( here );
        stats.clearance = veh->water_hull_height( here ) - veh->water_draft( here );
    }
    return accessibility::format_vehicle_workshop_stats( stats );
}

std::vector<std::pair<std::string, std::vector<std::string>>>
veh_interact::accessible_tasks_for_testing( map &here, vehicle &veh, const int part )
{
    veh_interact vehint( here, veh );
    vehint.owned_by_player = veh.handle_potential_theft( get_player_character(), true );
    const std::vector<access_option> options = vehint.part_tasks( here, part );
    std::vector<std::string> reasons;
    std::vector<task_speech> speech;
    vehint.evaluate_tasks( here, options, reasons, speech );
    std::vector<std::pair<std::string, std::vector<std::string>>> result;
    result.reserve( options.size() );
    for( size_t i = 0; i < options.size(); ++i ) {
        result.emplace_back( accessibility::format_vehicle_task( options[i].label, reasons[i] ),
                             speech[i].requirements );
    }
    return result;
}

std::string veh_interact::accessible_tile_for_testing( map &here, vehicle &veh, const int part )
{
    veh_interact vehint( here, veh );
    vehint.focus_part( here, part );
    return vehint.cursor_description();
}

std::string veh_interact::accessible_stats_for_testing( map &here, vehicle &veh )
{
    veh_interact vehint( here, veh );
    return vehint.accessible_stats( here );
}
