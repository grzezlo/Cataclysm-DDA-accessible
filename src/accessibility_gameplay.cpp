#include "accessibility_gameplay.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

#include "accessibility.h"
#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "character.h"
#include "character_martial_arts.h"
#include "creature.h"
#include "creature_tracker.h"
#include "display.h"
#include "effect.h"
#include "enums.h"
#include "field.h"
#include "flag.h"
#include "input_context.h"
#include "item.h"
#include "item_category.h"
#include "item_contents.h"
#include "line.h"
#include "map.h"
#include "map_item_stack.h"
#include "mapdata.h"
#include "martialarts.h"
#include "monster.h"
#include "move_mode.h"
#include "npc.h"
#include "omdata.h"
#include "output.h"
#include "overmapbuffer.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "ui_iteminfo.h"
#include "uilist.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"

static const efftype_id effect_bite( "bite" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_blind( "blind" );
static const efftype_id effect_infected( "infected" );

static const flag_id json_flag_SPLINT( "SPLINT" );

static const trait_id trait_INATTENTIVE( "INATTENTIVE" );

namespace accessibility_gameplay
{

namespace
{

enum class situation_entry_kind : int {
    current_position,
    hazard,
    creature,
    exit,
    obstacle,
    furniture,
    item
};

struct semantic_tile_info {
    std::string terrain;
    std::string furniture;
    std::vector<std::string> fields_and_hazards;
    std::vector<std::string> hazards;
    std::string vehicle;
    std::string creature;
    std::vector<std::string> items;
};

/** The announced auto-travel; valid while accessibility::travel_in_progress(). */
struct travel_plan {
    std::string destination;
    tripoint_abs_ms target;
    tripoint_abs_omt omt_target;
    bool distant = false;
    bool canceled = false;
};
travel_plan travel;

std::string route_tile_name( const map &here, const tripoint_bub_ms &position )
{
    return here.has_furn( position ) ? here.furn( position ).obj().name() :
           here.ter( position ).obj().name();
}

std::string overmap_location( const tripoint_abs_omt &origin, const tripoint_abs_omt &target )
{
    const int distance = rl_dist( origin.xy(), target.xy() );
    if( distance == 0 ) {
        return _( "here" );
    }
    const std::string direction = direction_name( direction_from( origin.xy().raw(),
                                  target.xy().raw() ) );
    return string_format( n_gettext( "%1$d map tile %2$s", "%1$d map tiles %2$s", distance ),
                          distance, direction );
}

std::string overmap_tile_name( const tripoint_abs_omt &position )
{
    const om_vision_level vision = overmap_buffer.seen( position );
    if( vision == om_vision_level::unseen ) {
        return _( "unseen" );
    }
    return overmap_buffer.ter( position )->get_name( vision );
}

struct limb_conditions {
    std::vector<bodypart_id> bleeding;
    std::vector<bodypart_id> broken;
};

limb_conditions collect_limb_conditions( const Character &who )
{
    limb_conditions result;
    for( const bodypart_id &bp : who.get_all_body_parts( get_body_part_flags::only_main ) ) {
        if( who.get_effect_int( effect_bleed, bp ) > 0 ) {
            result.bleeding.push_back( bp );
        }
        if( who.is_limb_broken( bp ) ) {
            result.broken.push_back( bp );
        }
    }
    return result;
}

std::vector<std::string> limb_names( const std::vector<bodypart_id> &limbs )
{
    std::vector<std::string> names;
    names.reserve( limbs.size() );
    for( const bodypart_id &bp : limbs ) {
        names.push_back( body_part_name_as_heading( bp, 1 ) );
    }
    return names;
}

std::string relative_z_location( const tripoint_bub_ms &origin, const tripoint_bub_ms &target )
{
    const int difference = target.z() - origin.z();
    if( difference == 0 ) {
        return {};
    }
    if( difference > 0 ) {
        return string_format( n_gettext( "%d level above", "%d levels above", difference ), difference );
    }
    const int levels_below = -difference;
    return string_format( n_gettext( "%d level below", "%d levels below", levels_below ),
                          levels_below );
}

/** Use the same visibility rule as look-around so both views describe the same tiles. */
visibility_type tile_visibility( map &here, const tripoint_bub_ms &position,
                                 const visibility_variables &cache )
{
    if( !here.inbounds( position ) ) {
        return visibility_type::HIDDEN;
    }
    return here.get_visibility( here.apparent_light_at( position, cache ), cache );
}

bool is_terrain_hazard( const map &here, const tripoint_bub_ms &position )
{
    return here.has_flag_ter( ter_furn_flag::TFLAG_DEEP_WATER, position ) ||
           here.has_flag_ter( ter_furn_flag::TFLAG_ROUGH, position ) ||
           here.has_flag_ter( ter_furn_flag::TFLAG_SHARP, position ) ||
           here.has_flag_ter( ter_furn_flag::TFLAG_UNSTABLE, position ) ||
           here.has_flag_ter( ter_furn_flag::TFLAG_NO_FLOOR, position ) ||
           here.has_flag_ter( ter_furn_flag::TFLAG_NO_FLOOR_WATER, position );
}

semantic_tile_info collect_semantic_tile_info( map &here, const avatar &viewer,
        const tripoint_bub_ms &position, const bool include_terrain )
{
    semantic_tile_info result;
    if( include_terrain ) {
        result.terrain = string_format( _( "Terrain: %s" ), here.tername( position ) );
    }
    if( here.has_furn( position ) ) {
        result.furniture = string_format( _( "Furniture: %s" ), here.furnname( position ) );
    }

    const trap &visible_trap = here.tr_at( position );
    if( visible_trap.can_see( position, viewer ) ) {
        const std::string trap_description = visible_trap.is_benign() ?
                                             string_format( _( "Trap: %s" ), visible_trap.name() ) :
                                             string_format( _( "Hazard: %s" ), visible_trap.name() );
        result.fields_and_hazards.push_back( trap_description );
        if( !visible_trap.is_benign() ) {
            result.hazards.push_back( trap_description );
        }
    }
    for( const std::pair<const field_type_id, field_entry> &field : here.field_at( position ) ) {
        const std::string field_description = field.second.is_dangerous() ?
                                              string_format( _( "Hazard: %s" ), field.second.name() ) :
                                              string_format( _( "Field: %s" ), field.second.name() );
        result.fields_and_hazards.push_back( field_description );
        if( field.second.is_dangerous() ) {
            result.hazards.push_back( field_description );
        }
    }

    if( is_terrain_hazard( here, position ) ) {
        const std::string terrain_hazard = string_format( _( "Hazard: %s" ), here.tername( position ) );
        result.fields_and_hazards.push_back( terrain_hazard );
        result.hazards.push_back( terrain_hazard );
    }

    if( include_terrain ) {
        if( const optional_vpart_position vp = here.veh_at( position ) ) {
            std::string vehicle_description = vp->vehicle().name;
            if( const std::optional<vpart_reference> displayed_part = vp.part_displayed() ) {
                vehicle_description += ", " + displayed_part->part().name();
            }
            result.vehicle = string_format( _( "Vehicle: %s" ), vehicle_description );
        }
    }

    const Creature *creature = get_creature_tracker().creature_at( position, true );
    if( creature != nullptr && creature != &viewer && viewer.sees( here, *creature ) ) {
        const std::string attitude = viewer.has_trait( trait_INATTENTIVE ) ? _( "Unknown" ) :
                                     Creature::get_attitude_ui_data( creature->attitude_to( viewer ) ).first.translated();
        result.creature = accessibility::format_semantic_sequence( {
            string_format( _( "Creature: %s" ), creature->disp_name() ),
            accessibility::format_creature_attitude( attitude )
        } );
    }

    if( here.sees_some_items( position, viewer ) ) {
        if( here.has_flag( ter_furn_flag::TFLAG_CONTAINER, position ) &&
            !here.could_see_items( position, viewer ) ) {
            result.items.push_back( _( "Items: contents are not visible" ) );
        } else if( viewer.has_effect( effect_blind ) || viewer.worn_with_flag( flag_BLIND ) ) {
            result.items.push_back( _( "Items: something is here, but it is not visible" ) );
        } else {
            std::map<std::string, int> items_here;
            for( const item &it : here.i_at( position ) ) {
                items_here[it.tname()] += it.count();
            }
            for( const std::pair<const std::string, int> &item_entry : items_here ) {
                const std::string name = item_entry.second > 1 ?
                                         string_format( _( "%1$s (%2$d)" ), item_entry.first, item_entry.second ) :
                                         item_entry.first;
                result.items.push_back( string_format( _( "Item: %s" ), name ) );
            }
        }
    }
    return result;
}

void add_situation_entry( std::vector<accessibility::semantic_list_entry> &entries,
                          const situation_entry_kind kind,
                          const tripoint_bub_ms &origin, const tripoint_bub_ms &position,
                          const std::string &label )
{
    const tripoint_rel_ms offset = position - origin;
    entries.push_back( { rl_dist( origin.xy(), position.xy() ), offset.z(), offset.y(), offset.x(),
                         static_cast<int>( kind ), label, situation_location( origin, position ) } );
}

std::string current_tile_name( map &here, const tripoint_bub_ms &position )
{
    std::string tile = here.tername( position );
    if( here.has_furn( position ) ) {
        tile += ", " + here.furnname( position );
    }
    return tile;
}

std::string semantic_hunger_text( const Character &who )
{
    if( !who.needs_food() ) {
        return _( "Without hunger" );
    }
    const std::string text = display::hunger_text_color( who ).first;
    return text.empty() ? _( "Satisfied" ) : text;
}

std::string semantic_thirst_text( const Character &who )
{
    const std::string text = display::thirst_text_color( who ).first;
    return text.empty() ? _( "Hydrated" ) : text;
}

std::string semantic_sleepiness_text( const Character &who )
{
    const std::string text = display::sleepiness_text_color( who ).first;
    return text.empty() ? _( "Rested" ) : text;
}

std::string semantic_pain_text( const Character &who )
{
    const std::string text = display::pain_text_color( who ).first;
    return text.empty() ? _( "No pain" ) : text;
}

std::string semantic_env_temp_text( const Character &who )
{
    const std::string temp = display::get_temp( who );
    return temp == "-" ? _( "Unknown" ) : temp;
}

std::vector<std::string> condition_transitions( const Character &who )
{
    accessibility::condition_tracker current;
    current.hunger_tier = semantic_hunger_text( who );
    current.thirst_tier = semantic_thirst_text( who );
    current.sleepiness_tier = semantic_sleepiness_text( who );
    current.pain_tier = semantic_pain_text( who );
    current.weariness_tier = display::weariness_text_color( who ).first;

    const int stam_max = who.get_stamina_max();
    current.stamina_critical = stam_max > 0 && who.get_stamina() <= ( stam_max / 4 );

    const limb_conditions limbs = collect_limb_conditions( who );
    current.bleeding_limbs = limb_names( limbs.bleeding );
    current.broken_limbs = limb_names( limbs.broken );

    return accessibility::evaluate_condition_transitions( current );
}

/**
 * Identify a threat by object identity.  Creature tracker indices shift when an
 * unrelated monster is removed, so they would cause false "spotted" repeats.
 */
std::string threat_id( const Creature &threat )
{
    if( const npc *person = dynamic_cast<const npc *>( &threat ) ) {
        return string_format( "npc:%d", person->getID().get_value() );
    }
    return "creature:" + std::to_string( reinterpret_cast<std::uintptr_t>( &threat ) );
}

std::string list_footer( const input_context &ctxt, const bool detail_view )
{
    const std::string navigation = string_format( _( "%1$s/%2$s: navigate" ),
                                   ctxt.get_desc( "UP", 1 ), ctxt.get_desc( "DOWN", 1 ) );
    const std::string confirm = detail_view ?
                                string_format( _( "%1$s or %2$s: return to status list" ),
                                        ctxt.get_desc( "QUIT", 1 ), ctxt.get_desc( "CONFIRM", 1 ) ) :
                                string_format( _( "%1$s: view details.  %2$s: return to game" ),
                                        ctxt.get_desc( "CONFIRM", 1 ), ctxt.get_desc( "QUIT", 1 ) );
    return string_format( _( "%1$s.  %2$s.  %3$s: repeat." ), navigation, confirm,
                          ctxt.get_desc( "ACCESSIBILITY_REPEAT_LAST", 1 ) );
}

input_context list_context()
{
    input_context ctxt( "UILIST" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    return ctxt;
}

} // namespace

std::string situation_location( const tripoint_bub_ms &origin, const tripoint_bub_ms &target )
{
    const int distance = rl_dist( origin.xy(), target.xy() );
    if( distance == 0 ) {
        return _( "here" );
    }

    const std::string direction = direction_name( direction_from( origin.xy().raw(),
                                  target.xy().raw() ) );
    return string_format( n_gettext( "%1$d tile %2$s", "%1$d tiles %2$s", distance ),
                          distance, direction );
}

std::vector<std::string> look_parts( map &here, const avatar &viewer,
                                     const tripoint_bub_ms &origin, const tripoint_bub_ms &position,
                                     const visibility_variables &cache )
{
    std::vector<std::string> parts;
    const visibility_type visibility = tile_visibility( here, position, cache );
    switch( visibility ) {
        case visibility_type::CLEAR: {
            const semantic_tile_info tile = collect_semantic_tile_info( here, viewer, position, true );
            parts.push_back( tile.terrain );
            parts.push_back( tile.furniture );
            parts.insert( parts.end(), tile.fields_and_hazards.begin(), tile.fields_and_hazards.end() );
            parts.push_back( tile.vehicle );
            parts.push_back( tile.creature );
            parts.insert( parts.end(), tile.items.begin(), tile.items.end() );
            break;
        }
        case visibility_type::BOOMER:
            parts.push_back( _( "A bright pink blur" ) );
            break;
        case visibility_type::BOOMER_DARK:
            parts.push_back( _( "A pink blur" ) );
            break;
        case visibility_type::DARK:
            parts.push_back( _( "Darkness" ) );
            break;
        case visibility_type::LIT:
            parts.push_back( _( "Bright light" ) );
            break;
        case visibility_type::HIDDEN:
            parts.push_back( _( "Unseen" ) );
            break;
    }
    parts.push_back( relative_z_location( origin, position ) );
    parts.push_back( situation_location( origin, position ) );
    return parts;
}

std::string gameplay_focus_description( map &here, const avatar &viewer )
{
    return string_format( _( "Gameplay. Current position: %s" ),
                          current_tile_name( here, viewer.pos_bub( here ) ) );
}

accessibility::direction_target direction_target( map &here, const avatar &viewer,
        const tripoint_bub_ms &origin, const tripoint_bub_ms &position )
{
    accessibility::direction_target result;
    result.direction = position == origin ? _( "here" ) :
                       direction_name( direction_from( origin.raw(), position.raw() ) );
    if( tile_visibility( here, position, here.get_visibility_variables_cache() ) !=
        visibility_type::CLEAR ) {
        result.target = _( "unseen" );
        return result;
    }

    const Creature *creature = get_creature_tracker().creature_at( position, true );
    const optional_vpart_position vp = here.veh_at( position );
    std::optional<vpart_reference> displayed_part;
    if( vp ) {
        displayed_part = vp.part_displayed();
    }
    if( creature != nullptr && creature != &viewer && viewer.sees( here, *creature ) ) {
        result.target = creature->disp_name();
    } else if( displayed_part ) {
        result.target = displayed_part->part().name();
    } else if( here.has_furn( position ) ) {
        result.target = here.furnname( position );
    } else {
        result.target = here.tername( position );
    }

    if( here.sees_some_items( position, viewer ) && here.could_see_items( position, viewer ) &&
        !viewer.has_effect( effect_blind ) && !viewer.worn_with_flag( flag_BLIND ) ) {
        const int count = static_cast<int>( here.i_at( position ).size() );
        //~ %1$s: target such as "table"; %2$d: number of items on it
        result.target = string_format( n_gettext( "%1$s with %2$d item", "%1$s with %2$d items", count ),
                                       result.target, count );
    }
    return result;
}

std::vector<accessibility::direction_target> direction_targets( map &here, const avatar &viewer,
        const tripoint_bub_ms &origin, const std::vector<tripoint_bub_ms> &valid )
{
    static const std::array<tripoint_rel_ms, 9> compass_order = { {
            tripoint_rel_ms::north, tripoint_rel_ms::north_east, tripoint_rel_ms::east,
            tripoint_rel_ms::south_east, tripoint_rel_ms::south, tripoint_rel_ms::south_west,
            tripoint_rel_ms::west, tripoint_rel_ms::north_west, tripoint_rel_ms::zero
        }
    };
    std::vector<tripoint_bub_ms> ordered;
    for( const tripoint_rel_ms &offset : compass_order ) {
        if( std::find( valid.begin(), valid.end(), origin + offset ) != valid.end() ) {
            ordered.push_back( origin + offset );
        }
    }
    // Vertical targets follow in their original order.
    for( const tripoint_bub_ms &position : valid ) {
        if( std::find( ordered.begin(), ordered.end(), position ) == ordered.end() ) {
            ordered.push_back( position );
        }
    }
    std::vector<accessibility::direction_target> result;
    result.reserve( ordered.size() );
    for( const tripoint_bub_ms &position : ordered ) {
        result.push_back( direction_target( here, viewer, origin, position ) );
    }
    return result;
}

std::string arrival_announcement( map &here, const avatar &viewer, const bool loaded )
{
    return accessibility::format_game_arrival( loaded, viewer.name,
            here.name( viewer.pos_bub( here ) ) );
}

void situation_review( map &here, const avatar &viewer )
{
    const tripoint_bub_ms origin = viewer.pos_bub( here );
    std::vector<accessibility::semantic_list_entry> situation_entries;
    std::vector<accessibility::semantic_list_entry> region_entries;

    add_situation_entry( situation_entries, situation_entry_kind::current_position, origin, origin,
                         string_format( _( "Current position: %1$s. Movement: %2$s" ),
                                        current_tile_name( here, origin ),
                                        viewer.current_movement_mode()->name() ) );

    here.update_visibility_cache( origin.z() );
    const visibility_variables &cache = here.get_visibility_variables_cache();
    for( const tripoint_bub_ms &position : here.points_in_radius( origin,
            MAX_VIEW_DISTANCE, 0 ) ) {
        if( position == origin ||
            tile_visibility( here, position, cache ) != visibility_type::CLEAR ) {
            continue;
        }

        const bool is_exit = here.has_flag_ter_or_furn( ter_furn_flag::TFLAG_DOOR, position ) ||
                             here.has_flag_ter_or_furn( ter_furn_flag::TFLAG_GOES_UP, position ) ||
                             here.has_flag_ter_or_furn( ter_furn_flag::TFLAG_GOES_DOWN, position ) ||
                             here.has_flag_ter_or_furn( ter_furn_flag::TFLAG_RAMP_UP, position ) ||
                             here.has_flag_ter_or_furn( ter_furn_flag::TFLAG_RAMP_DOWN, position );
        const bool is_obstacle = here.impassable( position );
        // Most visible tiles are plain floor; skip them before building any text.
        const bool may_describe = is_exit || is_obstacle || here.has_furn( position ) ||
                                  here.has_items( position ) || here.field_at( position ).field_count() > 0 ||
                                  !here.tr_at( position ).is_null() || is_terrain_hazard( here, position ) ||
                                  get_creature_tracker().creature_at( position, true ) != nullptr;
        if( !may_describe ) {
            continue;
        }

        const semantic_tile_info tile = collect_semantic_tile_info( here, viewer, position, false );
        for( const std::string &hazard : tile.hazards ) {
            add_situation_entry( region_entries, situation_entry_kind::hazard, origin, position,
                                 hazard );
        }
        if( !tile.creature.empty() ) {
            add_situation_entry( situation_entries, situation_entry_kind::creature, origin, position,
                                 tile.creature );
        }

        if( is_exit || is_obstacle ) {
            const std::string name = here.has_furn( position ) ? here.furnname( position ) :
                                     here.tername( position );
            if( is_exit ) {
                const std::string passability = is_obstacle ? _( "Impassable" ) : _( "Passable" );
                add_situation_entry( situation_entries, situation_entry_kind::exit, origin, position,
                                     string_format( _( "Exit: %1$s. %2$s" ), name, passability ) );
            } else {
                add_situation_entry( region_entries, situation_entry_kind::obstacle, origin, position,
                                     string_format( _( "Obstacle: %s" ), name ) );
            }
        }

        if( !tile.furniture.empty() && !is_exit && !is_obstacle ) {
            add_situation_entry( situation_entries, situation_entry_kind::furniture, origin, position,
                                 tile.furniture );
        }

        for( const std::string &item_description : tile.items ) {
            add_situation_entry( situation_entries, situation_entry_kind::item, origin, position,
                                 item_description );
        }
    }

    for( accessibility::connected_semantic_list_group &group :
         accessibility::group_connected_semantic_list_entries( std::move( region_entries ) ) ) {
        if( group.count > 1 ) {
            group.nearest.context = string_format(
                                        n_gettext( "%1$d connected tile; nearest: %2$s",
                                                   "%1$d connected tiles; nearest: %2$s", group.count ),
                                        group.count, group.nearest.context );
        }
        situation_entries.push_back( std::move( group.nearest ) );
    }

    accessibility::sort_semantic_list_entries( situation_entries );

    const input_context ctxt = list_context();
    uilist menu;
    menu.title = _( "Situation review" );
    menu.footer_text = string_format(
                           _( "%1$s or %2$s: return to game.  %3$s: repeat the last announcement." ),
                           ctxt.get_desc( "CONFIRM", 1 ), ctxt.get_desc( "QUIT", 1 ),
                           ctxt.get_desc( "ACCESSIBILITY_REPEAT_LAST", 1 ) );
    for( size_t index = 0; index < situation_entries.size(); ++index ) {
        const accessibility::semantic_list_entry &entry = situation_entries[index];
        menu.addentry_col( static_cast<int>( index ), true, std::nullopt, entry.label,
                           entry.context );
    }
    menu.query();
}

void status_review( const avatar &viewer, const std::string &safe_mode )
{
    enum class status_category : int {
        health = 0,
        stamina_condition,
        needs,
        temperature_effects,
        wielded_weapon,
        movement_safety,
        time_environment,
        count
    };

    const input_context ctxt = list_context();
    int selected = 0;

    while( true ) {
        uilist main_menu;
        main_menu.title = _( "Status review" );
        main_menu.footer_text = list_footer( ctxt, false );

        const limb_conditions limbs = collect_limb_conditions( viewer );
        accessibility::status_snapshot snapshot;
        snapshot.health = display::health_text_color( viewer ).first;
        snapshot.pain = semantic_pain_text( viewer );
        snapshot.bleeding = !limbs.bleeding.empty();
        snapshot.broken_limb = !limbs.broken.empty();

        const int stam_max = viewer.get_stamina_max();
        const int stam_pct = stam_max > 0 ? ( viewer.get_stamina() * 100 ) / stam_max : 0;
        snapshot.stamina_percent = stam_pct;
        snapshot.focus = viewer.get_focus();
        snapshot.weariness = display::weariness_text_color( viewer ).first;
        snapshot.hunger = semantic_hunger_text( viewer );
        snapshot.thirst = semantic_thirst_text( viewer );
        snapshot.sleepiness = semantic_sleepiness_text( viewer );
        snapshot.body_temperature = display::temp_text_color( viewer ).first;
        snapshot.environment_temperature = semantic_env_temp_text( viewer );
        for( const effect &eff : viewer.get_effects() ) {
            if( !eff.disp_name().empty() ) {
                snapshot.active_effect_count++;
            }
        }
        snapshot.wielded_weapon = viewer.weapname();
        snapshot.movement_mode = viewer.current_movement_mode()->name();
        snapshot.speed = viewer.get_speed();
        snapshot.safe_mode = safe_mode;
        snapshot.time = display::time_string( viewer );
        snapshot.date = display::date_string();
        snapshot.weather = display::weather_text_color( viewer ).first;

        const std::vector<accessibility::status_summary_entry> summaries =
            accessibility::format_status_summary( snapshot );
        for( size_t index = 0; index < summaries.size(); ++index ) {
            main_menu.addentry_col( static_cast<int>( index ), true, std::nullopt,
                                    summaries[index].label, summaries[index].value );
        }

        main_menu.selected = selected;
        main_menu.query();

        if( main_menu.ret < 0 ) {
            break;
        }

        selected = main_menu.ret;

        uilist detail_menu;
        detail_menu.footer_text = list_footer( ctxt, true );

        switch( static_cast<status_category>( selected ) ) {
            case status_category::health: {
                detail_menu.title = _( "Health & Pain details" );
                int idx = 0;
                for( const bodypart_id &bp : viewer.get_all_body_parts( get_body_part_flags::only_main ) ) {
                    const std::string name = body_part_name_as_heading( bp, 1 );
                    std::string status = string_format( _( "%1$d/%2$d HP" ),
                                                        viewer.get_part_hp_cur( bp ), viewer.get_part_hp_max( bp ) );
                    if( viewer.is_limb_broken( bp ) ) {
                        status += ", ";
                        status += _( "Broken" );
                    }
                    if( viewer.worn_with_flag( json_flag_SPLINT, bp ) ) {
                        status += ", ";
                        status += _( "Splinted" );
                    }
                    const int bleed = viewer.get_effect_int( effect_bleed, bp );
                    if( bleed > 0 ) {
                        status += ", " + string_format( _( "Bleeding (intensity %d)" ), bleed );
                    }
                    if( viewer.has_effect( effect_bite, bp.id() ) ) {
                        status += ", ";
                        status += _( "Bitten" );
                    }
                    if( viewer.has_effect( effect_infected, bp.id() ) ) {
                        status += ", ";
                        status += _( "Infected" );
                    }
                    detail_menu.addentry_col( idx++, true, std::nullopt, name, status );
                }
                detail_menu.addentry_col( idx++, true, std::nullopt,
                                          _( "Lifestyle health" ), display::health_text_color( viewer ).first );
                detail_menu.addentry_col( idx++, true, std::nullopt,
                                          _( "Pain" ),
                                          string_format( _( "%1$s (level %2$d)" ),
                                                  semantic_pain_text( viewer ),
                                                  viewer.get_perceived_pain() ) );
                break;
            }
            case status_category::stamina_condition: {
                detail_menu.title = _( "Stamina & Condition details" );
                detail_menu.addentry_col( 0, true, std::nullopt,
                                          _( "Stamina" ),
                                          string_format( _( "%1$d / %2$d (%3$d%%)" ),
                                                  viewer.get_stamina(), stam_max, stam_pct ) );
                detail_menu.addentry_col( 1, true, std::nullopt,
                                          _( "Focus" ), std::to_string( viewer.get_focus() ) );
                detail_menu.addentry_col( 2, true, std::nullopt,
                                          _( "Weariness" ), display::weariness_text_color( viewer ).first );
                detail_menu.addentry_col( 3, true, std::nullopt,
                                          _( "Weariness penalty" ), display::weary_malus_text_color( viewer ).first );
                detail_menu.addentry_col( 4, true, std::nullopt,
                                          _( "Activity level" ), display::activity_text_color( viewer ).first );
                break;
            }
            case status_category::needs: {
                detail_menu.title = _( "Needs details" );
                detail_menu.addentry_col( 0, true, std::nullopt,
                                          _( "Hunger" ), semantic_hunger_text( viewer ) );
                detail_menu.addentry_col( 1, true, std::nullopt,
                                          _( "Thirst" ), semantic_thirst_text( viewer ) );
                detail_menu.addentry_col( 2, true, std::nullopt,
                                          _( "Sleepiness" ), semantic_sleepiness_text( viewer ) );
                detail_menu.addentry_col( 3, true, std::nullopt,
                                          _( "Weight category" ), display::weight_text_color( viewer ).first );
                detail_menu.addentry_col( 4, true, std::nullopt,
                                          _( "Weight description" ), display::weight_long_description( viewer ) );
                break;
            }
            case status_category::temperature_effects: {
                detail_menu.title = _( "Temperature & Effects details" );
                detail_menu.addentry_col( 0, true, std::nullopt,
                                          _( "Body temperature feel" ), display::temp_text_color( viewer ).first );
                detail_menu.addentry_col( 1, true, std::nullopt,
                                          _( "Environment temperature" ), semantic_env_temp_text( viewer ) );
                int eff_idx = 2;
                for( const effect &eff : viewer.get_effects() ) {
                    const std::string name = eff.disp_name();
                    if( !name.empty() ) {
                        detail_menu.addentry_col( eff_idx++, true, std::nullopt, name, eff.disp_desc() );
                    }
                }
                break;
            }
            case status_category::wielded_weapon: {
                detail_menu.title = _( "Wielded weapon details" );
                detail_menu.addentry_col( 0, true, std::nullopt,
                                          _( "Wielded item" ), viewer.weapname_simple() );
                detail_menu.addentry_col( 1, true, std::nullopt,
                                          _( "Ammunition" ),
                                          viewer.weapname_ammo().empty() ? _( "None" ) : viewer.weapname_ammo() );
                detail_menu.addentry_col( 2, true, std::nullopt,
                                          _( "Attack mode" ),
                                          viewer.weapname_mode().empty() ? _( "Standard" ) : viewer.weapname_mode() );
                detail_menu.addentry_col( 3, true, std::nullopt,
                                          _( "Martial style" ),
                                          viewer.martial_arts_data->selected_style_name( viewer ) );
                break;
            }
            case status_category::movement_safety: {
                detail_menu.title = _( "Movement & Safety details" );
                detail_menu.addentry_col( 0, true, std::nullopt,
                                          _( "Movement mode" ), viewer.current_movement_mode()->name() );
                detail_menu.addentry_col( 1, true, std::nullopt,
                                          _( "Speed" ), std::to_string( viewer.get_speed() ) );
                detail_menu.addentry_col( 2, true, std::nullopt,
                                          _( "Move counter & mode" ),
                                          display::move_count_and_mode_text_color( viewer ).first );
                detail_menu.addentry_col( 3, true, std::nullopt,
                                          _( "Safe mode" ), snapshot.safe_mode );
                break;
            }
            case status_category::time_environment: {
                detail_menu.title = _( "Time & Environment details" );
                detail_menu.addentry_col( 0, true, std::nullopt,
                                          _( "Time" ), display::time_string( viewer ) );
                detail_menu.addentry_col( 1, true, std::nullopt,
                                          _( "Date" ), display::date_string() );
                detail_menu.addentry_col( 2, true, std::nullopt,
                                          _( "Weather" ), display::weather_text_color( viewer ).first );
                detail_menu.addentry_col( 3, true, std::nullopt,
                                          _( "Wind" ), display::wind_text_color( viewer ).first );
                detail_menu.addentry_col( 4, true, std::nullopt,
                                          _( "Time of day" ), display::time_approx() );
                break;
            }
            case status_category::count:
                break;
        }

        if( !detail_menu.entries.empty() ) {
            detail_menu.query();
        }
    }
}

void announce_turn_alerts( const map &here, const Character &viewer,
                           const std::vector<const Creature *> &threats )
{
    if( !accessibility::is_active() || !accessibility::game_messages_enabled() ) {
        return;
    }

    std::vector<std::string> threat_ids;
    threat_ids.reserve( threats.size() );
    for( const Creature *threat : threats ) {
        threat_ids.push_back( threat_id( *threat ) );
    }

    const tripoint_bub_ms origin = viewer.pos_bub( here );
    const auto describe = [&]() {
        constexpr size_t max_described_threats = 3;
        std::vector<std::pair<int, const Creature *>> by_distance;
        by_distance.reserve( threats.size() );
        for( const Creature *threat : threats ) {
            by_distance.emplace_back( rl_dist( origin, threat->pos_bub( here ) ), threat );
        }
        const size_t described = std::min( max_described_threats, by_distance.size() );
        std::partial_sort( by_distance.begin(), by_distance.begin() + described, by_distance.end(),
        []( const auto & lhs, const auto & rhs ) {
            return lhs.first < rhs.first;
        } );
        std::vector<std::string> descriptions;
        for( size_t index = 0; index < described; ++index ) {
            const Creature &threat = *by_distance[index].second;
            descriptions.push_back( accessibility::format_threat_description( threat.disp_name(),
                                    by_distance[index].first,
                                    direction_name( direction_from( origin, threat.pos_bub( here ) ) ) ) );
        }
        return accessibility::format_threat_announcement( descriptions,
                by_distance.size() - described );
    };

    std::vector<std::string> alerts;
    if( const std::optional<std::string> threat_transition =
            accessibility::evaluate_threat_transition( threat_ids, describe ) ) {
        alerts.push_back( *threat_transition );
    }
    const std::vector<std::string> condition_alerts = condition_transitions( viewer );
    alerts.insert( alerts.end(), condition_alerts.begin(), condition_alerts.end() );
    for( const std::string &alert : alerts ) {
        accessibility::queue_gameplay_announcement( alert, accessibility::gameplay_speech::meaningful );
    }
}

void before_player_input( map &here, const avatar &viewer )
{
    // Waiting for a key means announced travel is over.  Arrival already ended
    // it, so only an early stop reaches this point.
    if( accessibility::take_travel_in_progress() && !travel.canceled ) {
        std::string location;
        if( travel.distant ) {
            location = overmap_location( viewer.pos_abs_omt(), travel.omt_target );
        } else {
            location = situation_location( viewer.pos_bub(), here.get_bub( travel.target ) );
        }
        accessibility::queue_gameplay_announcement( accessibility::format_travel_stopped(
                    travel.destination, location ), accessibility::gameplay_speech::meaningful );
    }
    if( accessibility::take_suppressed_movement() ) {
        accessibility::queue_gameplay_announcement( gameplay_focus_description( here, viewer ),
                accessibility::gameplay_speech::routine );
    }
    accessibility::flush_gameplay_announcements();
}

void announce_avatar_moved( const tripoint_abs_ms &old_abs_pos, const avatar &viewer,
                            const map &here )
{
    if( !accessibility::is_active() || !accessibility::game_messages_enabled() ) {
        return;
    }
    // Only steps with more route ahead are skipped.  The route loses its step
    // before the move, so the final step is spoken as the arrival.  The saved
    // destination point is not used: it stays set after a finished route.
    // Overmap travel ends each local route and plans the next one, so only
    // travel_arrived() ends it.
    const bool travel_active = accessibility::travel_in_progress();
    if( viewer.has_destination() || ( travel_active && travel.distant ) ) {
        // Per-step speech would flood auto-travel; if travel stops early the
        // position is reported when the game next waits for the player.
        accessibility::note_suppressed_movement();
        return;
    }
    // This step is spoken, so no separate position report is needed.
    accessibility::take_suppressed_movement();
    const tripoint_bub_ms new_pos = viewer.pos_bub();
    const tripoint_abs_ms new_abs_pos = here.get_abs( new_pos );
    const std::string tile_name = route_tile_name( here, new_pos );
    if( travel_active ) {
        accessibility::take_travel_in_progress();
        accessibility::queue_gameplay_announcement( accessibility::format_travel_arrival( tile_name ),
                accessibility::gameplay_speech::meaningful );
        return;
    }
    accessibility::announce_movement(
        direction_name( direction_from( old_abs_pos.raw(), new_abs_pos.raw() ) ), tile_name );
}

void announce_travel_start( map &here, const avatar &viewer,
                            const std::vector<tripoint_bub_ms> &route )
{
    if( !accessibility::is_active() || !accessibility::game_messages_enabled() || route.empty() ) {
        return;
    }
    const tripoint_bub_ms origin = viewer.pos_bub();
    const tripoint_bub_ms destination = route.back();
    const visibility_variables &cache = here.get_visibility_variables_cache();
    std::vector<std::string> names;
    names.reserve( route.size() );
    for( const tripoint_bub_ms &position : route ) {
        names.push_back( tile_visibility( here, position, cache ) == visibility_type::CLEAR ?
                         route_tile_name( here, position ) : _( "unseen" ) );
    }
    travel = travel_plan();
    travel.destination = direction_target( here, viewer, origin, destination ).target;
    travel.target = here.get_abs( destination );
    accessibility::note_travel_started();
    accessibility::queue_gameplay_announcement( accessibility::format_travel_start(
                travel.destination, situation_location( origin, destination ),
                static_cast<int>( route.size() ), accessibility::merge_travel_segments( names ) ),
            accessibility::gameplay_speech::meaningful );
}

void announce_overmap_travel_start( const avatar &viewer,
                                    const std::vector<tripoint_abs_omt> &path )
{
    if( !accessibility::is_active() || !accessibility::game_messages_enabled() || path.empty() ) {
        return;
    }
    const tripoint_abs_omt origin = viewer.pos_abs_omt();
    // The path is stored destination first and may end with the current tile.
    std::vector<std::string> names;
    for( auto position = path.rbegin(); position != path.rend(); ++position ) {
        if( *position != origin ) {
            names.push_back( overmap_tile_name( *position ) );
        }
    }
    travel = travel_plan();
    travel.distant = true;
    travel.omt_target = path.front();
    travel.destination = overmap_tile_name( travel.omt_target );
    accessibility::note_travel_started();
    accessibility::queue_gameplay_announcement( accessibility::format_travel_start(
                travel.destination, overmap_location( origin, travel.omt_target ), 0,
                accessibility::merge_travel_segments( names ) ),
            accessibility::gameplay_speech::meaningful );
}

void travel_canceled()
{
    travel.canceled = true;
}

void travel_arrived()
{
    accessibility::take_travel_in_progress();
}

accessibility::nearby_list_entry nearby_item_entry( const avatar &viewer,
        const map_item_stack &stack, const int page, const int result_index, const int result_count,
        const int priority, const std::optional<content_newness> newness )
{
    accessibility::nearby_list_entry result;
    if( stack.example == nullptr || page < 0 || static_cast<size_t>( page ) >= stack.vIG.size() ) {
        return result;
    }

    const auto &group = stack.vIG[page];
    result.name = remove_color_tags( group.it->display_name() );
    result.count = stack.totalcount;
    result.position_index = page + 1;
    result.position_count = static_cast<int>( stack.vIG.size() );
    result.location = situation_location( viewer.pos_bub(), viewer.pos_bub() + group.pos );
    result.details.push_back( string_format( _( "Category: %s" ),
                              stack.example->get_category_of_contents().name_header() ) );
    if( group.count != stack.totalcount ) {
        result.details.push_back( string_format( _( "At this location: %d" ), group.count ) );
    }
    if( priority > 0 ) {
        result.details.push_back( _( "High priority" ) );
    } else if( priority < 0 ) {
        result.details.push_back( _( "Low priority" ) );
    }
    if( newness ) {
        switch( *newness ) {
            case content_newness::NEW:
                result.details.push_back( _( "New" ) );
                break;
            case content_newness::MIGHT_BE_HIDDEN:
                result.details.push_back( _( "May hide contents" ) );
                break;
            case content_newness::SEEN:
                break;
        }
    }
    result.result_index = result_index;
    result.result_count = result_count;
    return result;
}

accessibility::nearby_list_entry nearby_creature_entry( const map &here, const avatar &viewer,
        const Creature &creature, const int result_index, const int result_count,
        const bool player_knows )
{
    accessibility::nearby_list_entry result;
    const monster *const mon = dynamic_cast<const monster *>( &creature );
    const npc *const person = dynamic_cast<const npc *>( &creature );
    result.name = mon != nullptr ? mon->name() : creature.disp_name();
    result.location = situation_location( viewer.pos_bub(), creature.pos_bub() );

    if( player_knows ) {
        result.details.push_back( accessibility::format_creature_attitude(
                                      Creature::get_attitude_ui_data(
                                          creature.attitude_to( viewer ) ).first.translated() ) );
        std::string behavior;
        if( mon != nullptr ) {
            behavior = mon->get_attitude().first;
        } else if( person != nullptr ) {
            behavior = npc_attitude_name( person->get_attitude() );
        }
        if( !behavior.empty() ) {
            result.details.push_back( string_format( _( "Behavior: %s" ), behavior ) );
        }
        if( creature.sees( here, viewer ) ) {
            result.details.push_back( _( "Sees you" ) );
        }
    } else {
        result.details.push_back( accessibility::format_creature_attitude( _( "unknown" ) ) );
    }
    result.details.push_back( accessibility::format_creature_health( creature.get_hp(),
                              creature.get_hp_max() ) );
    result.result_index = result_index;
    result.result_count = result_count;
    return result;
}

std::string nearby_item_sort_change( const list_item_sort_mode mode )
{
    switch( mode ) {
        case list_item_sort_mode::count:
        case list_item_sort_mode::DISTANCE:
            return _( "Sorted by distance" );
        case list_item_sort_mode::NAME:
            return _( "Sorted by name" );
        case list_item_sort_mode::CATEGORY_DISTANCE:
            return _( "Sorted by category and distance" );
        case list_item_sort_mode::CATEGORY_NAME:
            return _( "Sorted by category and name" );
    }
    return {};
}

accessibility::item_action_availability item_action_availability( const hint_rating rating )
{
    switch( rating ) {
        case hint_rating::good:
            break;
        case hint_rating::iffy:
            return accessibility::item_action_availability::not_now;
        case hint_rating::cant:
            return accessibility::item_action_availability::not_applicable;
    }
    return accessibility::item_action_availability::available;
}

void describe_item_action_menu( uilist &menu, const std::string &item_name,
                                const item_info_data &info )
{
    menu.accessibility_context = accessibility::format_item_action_menu_context(
                                     remove_color_tags( item_name ) );
    menu.accessibility_details = [&info]() {
        item_info_data document = info;
        document.without_getch = false;
        document.handle_scrolling = true;
        iteminfo_window window( document, point( -1, -1 ), TERMX / 2, TERMY );
        window.execute();
    };
}

} // namespace accessibility_gameplay
