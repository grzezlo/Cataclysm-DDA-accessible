#include "accessibility_gameplay.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

#include "accessibility.h"
#include "activity_type.h"
#include "avatar.h"
#include "basecamp.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character.h"
#include "character_martial_arts.h"
#include "city.h"
#include "construction.h"
#include "construction_group.h"
#include "coordinates.h"
#include "creature.h"
#include "creature_tracker.h"
#include "display.h"
#include "effect.h"
#include "enums.h"
#include "field.h"
#include "flag.h"
#include "input_context.h"
#include "inventory.h"
#include "item.h"
#include "item_category.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "itype.h"
#include "line.h"
#include "localized_comparator.h"
#include "map.h"
#include "map_item_stack.h"
#include "mapdata.h"
#include "martialarts.h"
#include "mission.h"
#include "monster.h"
#include "mtype.h"
#include "move_mode.h"
#include "npc.h"
#include "omdata.h"
#include "output.h"
#include "overmap_ui.h"
#include "overmapbuffer.h"
#include "player_activity.h"
#include "recipe.h"
#include "requirements.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "ui_iteminfo.h"
#include "uilist.h"
#include "units.h"
#include "units_utility.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"

static const efftype_id effect_bite( "bite" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_blind( "blind" );
static const efftype_id effect_infected( "infected" );

static const flag_id json_flag_SPLINT( "SPLINT" );

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );
static const trait_id trait_INATTENTIVE( "INATTENTIVE" );

namespace accessibility_gameplay
{

namespace
{
struct activity_speech_state {
    std::string id;
    std::string label;
    std::string reason;
    std::vector<std::string> results;
    std::unique_ptr<accessibility::progress_reporter> progress;
    unsigned generation = 0;
    int turns = 0;
    bool canceled = false;
    bool completed = false;
};

activity_speech_state &activity_state()
{
    // Construct after the service so reporters are destroyed before it at exit.
    static activity_speech_state instance = []() {
        accessibility::get_screen_reader_mode();
        return activity_speech_state{};
    }
    ();
    return instance;
}
activity_turn_speech *active_activity_turn = nullptr;

std::string activity_label( const player_activity &act, const avatar &viewer )
{
    if( act.id().str() == "ACT_BUILD" ) {
        const partial_con *pc = get_map().partial_con_at( get_map().get_bub( act.placement ) );
        if( pc && pc->id.is_valid() ) {
            return string_format( _( "Constructing %s" ), pc->id->group->name() );
        }
    }
    if( act.id().str() == "ACT_READ" && !act.targets.empty() && act.targets.front() ) {
        return string_format( _( "Reading %s" ), act.targets.front()->tname() );
    }
    return accessibility::normalize_status_announcement(
               act.get_progress_message( viewer ).value_or( act.get_verb().translated() ), true );
}

void add_activity_results( const std::vector<std::string> &messages )
{
    for( const std::string &message : messages ) {
        if( std::find( activity_state().results.begin(), activity_state().results.end(), message ) ==
            activity_state().results.end() ) {
            activity_state().results.push_back( message );
        }
    }
}

void end_activity_speech( const bool finished, const std::vector<std::string> &messages )
{
    add_activity_results( messages );
    const std::string result = accessibility::format_activity_result( activity_state().label,
                               finished, activity_state().canceled, activity_state().reason,
                               activity_state().results );
    activity_state().id.clear();
    activity_state().progress.reset();
    activity_state().reason.clear();
    activity_state().results.clear();
    activity_state().canceled = false;
    activity_state().completed = false;
    accessibility::queue_gameplay_announcement( result,
            accessibility::gameplay_speech::meaningful );
}
} // namespace

void reset_activity_speech()
{
    const unsigned next_generation = activity_state().generation + 1;
    activity_state() = activity_speech_state{};
    activity_state().generation = next_generation;
}

void activity_started( const player_activity &act, const avatar &viewer, const bool resuming )
{
    if( !accessibility::is_active() || !accessibility::game_messages_enabled() ||
        !act.get_progress_message( viewer ) ) {
        return;
    }
    // Construction assigns its placement after start_or_resume; its owning
    // feature calls this hook once the selected project is available.
    if( act.id().str() == "ACT_BUILD" && act.placement == tripoint_abs_ms::min ) {
        return;
    }
    if( active_activity_turn && activity_state().turns > 0 ) {
        if( activity_state().completed && activity_state().id == act.id().str() ) {
            // Continuous crafting creates a new actor for every batch.  It is
            // still the same long action, with one start and one final result.
            activity_state().completed = false;
            activity_state().canceled = false;
            update_activity_progress( viewer );
            return;
        }
        active_activity_turn->conclude();
    }
    reset_activity_speech();
    activity_state().id = act.id().str();
    activity_state().label = activity_label( act, viewer );
    std::string duration;
    // Only use durations shown by these interfaces; arbitrary move counts can
    // represent counters rather than an estimate (crafting, stamina, etc.).
    if( act.moves_left > 0 && ( act.id().str() == "ACT_WAIT" ||
                                act.id().str() == "ACT_READ" || act.id().str() == "ACT_WAIT_WEATHER" ) ) {
        duration = to_string_approx( time_duration::from_moves( act.moves_left ) );
    }
    if( act.id().str() == "ACT_BUILD" ) {
        const partial_con *pc = get_map().partial_con_at( get_map().get_bub( act.placement ) );
        if( pc && pc->id.is_valid() ) {
            duration = to_string_approx( time_duration::from_moves( pc->id->adjusted_time() ) );
        }
    } else if( act.id().str() == "ACT_CRAFT" && !act.targets.empty() && act.targets.front() ) {
        const item &craft = *act.targets.front();
        if( craft.is_craft() ) {
            duration = to_string_approx( time_duration::from_moves(
                                             viewer.expected_time_to_craft( craft.get_making(), craft.get_making_batch_size() ) ) );
        }
    }
    accessibility::queue_gameplay_announcement( accessibility::format_activity_start(
                activity_state().label, duration, resuming ), accessibility::gameplay_speech::meaningful );
    accessibility::flush_gameplay_announcements();
    activity_state().progress = std::make_unique<accessibility::progress_reporter>();
    update_activity_progress( viewer );
}

void activity_interruption_reason( const std::string &reason )
{
    activity_state().reason = reason;
}

void activity_canceled( const avatar &viewer )
{
    if( activity_state().id.empty() || activity_state().id != viewer.activity.id().str() ) {
        return;
    }
    activity_state().canceled = !activity_state().completed;
    if( activity_state().turns == 0 ) {
        end_activity_speech( false, {} );
    }
}

void activity_completed( const Character &viewer )
{
    if( viewer.is_avatar() && activity_state().id == viewer.activity.id().str() ) {
        activity_state().completed = true;
    }
}

void sleep_started( const avatar & )
{
    if( !accessibility::is_active() || !accessibility::game_messages_enabled() ) {
        return;
    }
    reset_activity_speech();
    activity_state().id = "sleep";
    activity_state().label = _( "Sleeping" );
    // The sleep effect's apply message is the spoken start.  Do not duplicate
    // it with a second lifecycle announcement.
    activity_state().progress = std::make_unique<accessibility::progress_reporter>();
    activity_state().progress->update( _( "Sleeping" ) );
}

void sleep_finished( const avatar & )
{
    if( activity_state().id != "sleep" ) {
        return;
    }
    reset_activity_speech();
    // The effect's remove message reports waking once it is actually removed.
}

void update_activity_progress( const avatar &viewer )
{
    if( !accessibility::is_active() || !accessibility::game_messages_enabled() ) {
        return;
    }
    if( viewer.has_effect( efftype_id( "sleep" ) ) &&
        viewer.get_effect( efftype_id( "sleep" ) ).get_duration() > 0_turns &&
        activity_state().id != "sleep" ) {
        // Loading a sleeping character resumes its progress without claiming
        // that it has just fallen asleep.
        reset_activity_speech();
        activity_state().id = "sleep";
        activity_state().label = _( "Sleeping" );
        accessibility::queue_gameplay_announcement( _( "Sleeping" ),
                accessibility::gameplay_speech::meaningful );
        activity_state().progress = std::make_unique<accessibility::progress_reporter>();
    }
    if( !activity_state().progress ) {
        return;
    }
    if( activity_state().id == "sleep" ) {
        // No remaining duration is visible while sleeping.
        activity_state().progress->update( _( "Sleeping" ) );
    } else if( const auto progress = viewer.activity.get_progress_message( viewer ) ) {
        std::string remaining;
        const player_activity &act = viewer.activity;
        if( act.moves_left > 0 && ( act.id().str() == "ACT_WAIT" ||
                                    act.id().str() == "ACT_READ" || act.id().str() == "ACT_WAIT_WEATHER" ) ) {
            remaining = to_string_approx( time_duration::from_moves( act.moves_left ) );
        }
        activity_state().progress->update( accessibility::format_activity_progress(
                                               activity_state().label, *progress, remaining ) );
    }
}

activity_turn_speech::activity_turn_speech( Character &who )
{
    if( !who.is_avatar() || !accessibility::is_active() ||
        !accessibility::game_messages_enabled() ||
        !who.activity.get_progress_message( *who.as_avatar() ) ) {
        return;
    }
    viewer = who.as_avatar();
    if( activity_state().id != viewer->activity.id().str() ) {
        activity_started( viewer->activity, *viewer, true );
    }
    generation = activity_state().generation;
    ++activity_state().turns;
    capture = std::make_unique<accessibility::game_message_speech_capture>( true );
    previous = active_activity_turn;
    active_activity_turn = this;
}

activity_turn_speech::~activity_turn_speech()
{
    conclude();
    if( active_activity_turn == this ) {
        active_activity_turn = previous;
    }
}

void activity_turn_speech::conclude()
{
    if( concluded || !viewer ) {
        return;
    }
    concluded = true;
    if( generation != activity_state().generation ) {
        if( activity_state().id == "sleep" ) {
            // Falling asleep ends ACT_TRY_SLEEP inside its handler.  Preserve
            // the effect's apply message and any warmth/comfort messages.
            accessibility::queue_gameplay_announcement(
                accessibility::format_semantic_sequence( capture->messages() ),
                accessibility::gameplay_speech::meaningful );
        }
        return;
    }
    --activity_state().turns;
    if( activity_state().canceled || viewer->activity.id().str() != activity_state().id ) {
        end_activity_speech( finished || activity_state().completed, capture->messages() );
    } else {
        // Keep skill gains and other handler messages for the final result;
        // repeated routine messages become one entry rather than turn speech.
        add_activity_results( capture->messages() );
        update_activity_progress( *viewer );
    }
}

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
std::string creature_name( const Creature &creature )
{
    const monster *const mon = dynamic_cast<const monster *>( &creature );
    return mon != nullptr ? mon->name() : creature.disp_name();
}

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
    result.name = creature_name( creature );
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

accessibility::target_focus target_focus( map &here, const avatar &viewer,
        const tripoint_bub_ms &origin, const tripoint_bub_ms &cursor,
        const std::vector<tripoint_bub_ms> &trajectory, const Creature *critter, const vehicle *veh,
        const bool line_of_fire )
{
    const visibility_variables &cache = here.get_visibility_variables_cache();
    accessibility::target_focus result;
    result.tile = look_parts( here, viewer, origin, cursor, cache );
    result.location = relative_z_location( origin, cursor );
    const std::string location = situation_location( origin, cursor );
    result.location = result.location.empty() ? location :
                      //~ %1$s: level difference such as "1 level above"; %2$s: distance and direction
                      string_format( _( "%1$s, %2$s" ), result.location, location );
    if( critter != nullptr ) {
        if( viewer.sees( here, *critter ) ) {
            result.target = creature_name( *critter );
            if( !viewer.has_trait( trait_INATTENTIVE ) ) {
                result.attitude = Creature::get_attitude_ui_data(
                                      critter->attitude_to( viewer ) ).first.translated();
            }
        } else {
            result.target = _( "sensed creature" );
        }
    } else if( veh != nullptr && viewer.sees( here, veh->pos_bub( here ) ) ) {
        result.target = veh->name;
    }

    if( !line_of_fire ) {
        return result;
    }
    creature_tracker &creatures = get_creature_tracker();
    for( const tripoint_bub_ms &position : trajectory ) {
        if( position == cursor ) {
            break;
        }
        if( position == origin ||
            tile_visibility( here, position, cache ) != visibility_type::CLEAR ) {
            continue;
        }
        const Creature *in_line = creatures.creature_at( position, true );
        if( in_line != nullptr && in_line != &viewer && viewer.sees( here, *in_line ) ) {
            result.in_line_of_fire.push_back( creature_name( *in_line ) );
        }
        if( here.impassable( position ) ) {
            const optional_vpart_position vp = here.veh_at( position );
            std::optional<vpart_reference> part;
            if( vp ) {
                part = vp.part_displayed();
            }
            result.blocked_by = part ? part->part().name() : route_tile_name( here, position );
            break;
        }
    }
    return result;
}

std::vector<std::string> target_details( map &here, const avatar &viewer,
        const Creature *critter, const vehicle *veh )
{
    std::vector<std::string> result;
    if( critter != nullptr ) {
        if( !viewer.sees( here, *critter ) ) {
            result.emplace_back( _( "You sense a creature here, but cannot see it." ) );
            return result;
        }
        const accessibility::nearby_list_entry entry = nearby_creature_entry( here, viewer, *critter,
                0, 0, !viewer.has_trait( trait_INATTENTIVE ) );
        std::vector<std::string> summary = { entry.name };
        summary.insert( summary.end(), entry.details.begin(), entry.details.end() );
        result.push_back( accessibility::format_semantic_sequence( summary ) );
        if( const monster *const mon = dynamic_cast<const monster *>( critter ) ) {
            const std::string effects = remove_color_tags( mon->get_effect_status() );
            if( !effects.empty() ) {
                //~ %s: effects on the target such as "stunned"
                result.push_back( string_format( _( "Effects: %s" ), effects ) );
            }
            result.push_back( remove_color_tags( mon->type->get_description() ) );
        }
    } else if( veh != nullptr && viewer.sees( here, veh->pos_bub( here ) ) ) {
        //~ %s: vehicle name
        result.push_back( string_format( _( "Vehicle: %s" ), veh->name ) );
    }
    return result;
}

namespace
{

bool requirement_option_available( const accessibility::requirement_option &option )
{
    return option.state != accessibility::requirement_state::missing &&
           option.state != accessibility::requirement_state::not_enough;
}

/** Group alternatives as the requirement list does: available ones first, duplicates once. */
template<typename T, typename NameFn, typename StateFn>
void append_requirement_groups( std::vector<accessibility::requirement_group> &groups,
                                const accessibility::requirement_kind kind,
                                const std::vector<std::vector<T>> &objs, NameFn name_of, StateFn state_of )
{
    for( const std::vector<T> &alternatives : objs ) {
        accessibility::requirement_group group;
        group.kind = kind;
        for( const T &alternative : alternatives ) {
            accessibility::requirement_option option;
            option.name = name_of( alternative );
            option.state = state_of( alternative );
            const bool duplicate = std::any_of( group.options.begin(), group.options.end(),
            [&option]( const accessibility::requirement_option & other ) {
                return other.name == option.name && other.state == option.state;
            } );
            if( !duplicate ) {
                group.options.push_back( option );
            }
        }
        if( group.options.empty() ) {
            continue;
        }
        std::stable_sort( group.options.begin(), group.options.end(),
                          []( const accessibility::requirement_option & a,
        const accessibility::requirement_option & b ) {
            const bool a_available = requirement_option_available( a );
            const bool b_available = requirement_option_available( b );
            if( a_available != b_available ) {
                return a_available;
            }
            return localized_compare( a.name, b.name );
        } );
        groups.push_back( group );
    }
}

std::string without_final_period( std::string text )
{
    while( !text.empty() && ( text.back() == '.' || text.back() == ' ' ) ) {
        text.pop_back();
    }
    return text;
}

} // namespace

std::vector<accessibility::requirement_group> requirement_groups( const requirement_data &req,
        const inventory &inv, const std::function<bool( const item & )> &component_filter,
        const int batch, const bool include_components )
{
    using accessibility::requirement_state;
    std::vector<accessibility::requirement_group> groups;
    const bool debug_hammerspace = get_player_character().has_trait( trait_DEBUG_HS );
    // Refresh the cached availability that the visual requirement list colors by.
    req.can_make_with_inventory( inv, return_true<item> );
    append_requirement_groups( groups, accessibility::requirement_kind::tools, req.get_qualities(),
    [batch]( const quality_requirement & quality ) {
        return without_final_period( remove_color_tags( quality.to_string( batch ) ) );
    }, [debug_hammerspace]( const quality_requirement & quality ) {
        return debug_hammerspace || quality.available == available_status::a_true ?
               requirement_state::have : requirement_state::missing;
    } );
    append_requirement_groups( groups, accessibility::requirement_kind::tools, req.get_tools(),
    [batch]( const tool_comp & tool ) {
        return remove_color_tags( tool.to_string( batch ) );
    }, [&inv, batch]( const tool_comp & tool ) {
        if( tool.available == available_status::a_insufficient ) {
            return requirement_state::not_enough;
        }
        return tool.has( inv, return_true<item>, batch ) ? requirement_state::have :
               requirement_state::missing;
    } );
    if( !include_components ) {
        return groups;
    }
    req.can_make_with_inventory( inv, component_filter );
    append_requirement_groups( groups, accessibility::requirement_kind::components,
                               req.get_components(),
    [&inv, &component_filter, batch]( const item_comp & component ) {
        const int quantity = item::count_by_charges( component.type ) ?
                             inv.charges_of( component.type, INT_MAX, component_filter ) :
                             inv.amount_of( component.type, false, INT_MAX, component_filter );
        return remove_color_tags( component.to_string( batch, quantity ) );
    }, [&inv, &component_filter, batch]( const item_comp & component ) {
        if( component.available == available_status::a_insufficient ) {
            return requirement_state::not_enough;
        }
        if( !component.has( inv, component_filter, batch ) ) {
            return requirement_state::missing;
        }
        const int used = component.count * batch;
        const bool liquid_container = std::any_of( component.type->pockets.begin(),
        component.type->pockets.end(), []( const pocket_data & pocket ) {
            return pocket.type == pocket_type::CONTAINER && pocket.watertight;
        } );
        if( liquid_container && inv.must_use_liq_container( component.type, used ) ) {
            return requirement_state::uses_filled_container;
        }
        if( inv.must_use_hallu_poison( component.type, used ) ) {
            return requirement_state::uses_hallucinogen_or_poison;
        }
        if( !component.has( inv, [&component_filter]( const item & it ) {
        return component_filter( it ) && !it.is_favorite;
        }, batch ) ) {
            return requirement_state::uses_favorite;
        }
        return requirement_state::have;
    } );
    return groups;
}

std::vector<std::string> disassembly_details( const recipe &uncraft, const std::string &yield,
        const std::string &time, const inventory &inv )
{
    std::vector<std::string> lines;
    if( !yield.empty() ) {
        //~ %s: components returned by disassembly, such as "2 rags and 1 thread"
        lines.push_back( accessibility::format_semantic_sequence( { string_format( _( "Returns: %s" ), yield ) } ) );
    }
    if( !time.empty() ) {
        //~ %s: time needed to disassemble an item
        lines.push_back( accessibility::format_semantic_sequence( { string_format( _( "Time: %s" ), time ) } ) );
    }
    const std::vector<accessibility::requirement_group> groups = requirement_groups(
                uncraft.disassembly_requirements(), inv, uncraft.get_component_filter(), 1, false );
    if( groups.empty() ) {
        lines.push_back( accessibility::format_requirement_group( accessibility::requirement_group() ) );
    }
    for( const accessibility::requirement_group &group : groups ) {
        lines.push_back( accessibility::format_requirement_group( group ) );
    }
    return lines;
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

namespace
{

/** Terrain name as the viewer knows it, or empty when the tile is unexplored. */
std::string overmap_known_terrain( const tripoint_abs_omt &position )
{
    const om_vision_level vision = overmap_buffer.seen( position );
    if( vision == om_vision_level::unseen ) {
        return std::string();
    }
    const oter_id &ter = overmap_buffer.ter( position );
    if( ter->blends_adjacent( vision ) ) {
        return oter_vision::get_blended_omt_info( position, vision ).name;
    }
    return ter->get_name( vision );
}

/** Note text without its glyph and color prefix. */
std::string overmap_note_text( const tripoint_abs_omt &position )
{
    if( !overmap_buffer.has_note( position ) ) {
        return std::string();
    }
    const std::string &note = overmap_buffer.note( position );
    return note.substr( std::min( note.size(), std::get<2>( overmap_ui::get_note_display_info(
                                      note ) ) ) );
}

std::vector<std::string> overmap_mission_targets( const avatar &viewer,
        const tripoint_abs_omt &position )
{
    std::vector<std::string> titles;
    for( const mission *miss : viewer.get_active_missions() ) {
        if( miss->has_target() && miss->get_target() == position ) {
            titles.push_back( miss->name() );
        }
    }
    return titles;
}

const std::string &overmap_vision_text( const om_vision_level vision )
{
    static const std::string none;
    static const std::string vague = _( "You can only make out vague details of what's here." );
    static const std::string outlines = _( "You can only make out outlines of what's here." );
    static const std::string details = _( "You can make out some details of what's here." );
    switch( vision ) {
        case om_vision_level::vague:
            return vague;
        case om_vision_level::outlines:
            return outlines;
        case om_vision_level::details:
            return details;
        default:
            return none;
    }
}

} // namespace

accessibility::overmap_tile overmap_tile( const avatar &viewer, const tripoint_abs_omt &position )
{
    accessibility::overmap_tile tile;
    tile.terrain = overmap_known_terrain( position );
    tile.note = overmap_note_text( position );
    tile.danger_radius = overmap_buffer.note_danger_radius( position );
    tile.in_dangerous_area = tile.danger_radius < 0 && overmap_buffer.is_marked_dangerous( position );
    tile.missions = overmap_mission_targets( viewer, position );
    const std::vector<tripoint_abs_omt> &path = viewer.omt_path;
    tile.route_destination = !path.empty() && path.front() == position;
    tile.on_route = std::find( path.begin(), path.end(), position ) != path.end();
    const tripoint_abs_omt origin = viewer.pos_abs_omt();
    tile.dx = position.x() - origin.x();
    tile.dy = position.y() - origin.y();
    tile.dz = position.z() - origin.z();
    return tile;
}

std::vector<std::string> overmap_tile_details( const avatar &viewer,
        const tripoint_abs_omt &position, const std::string &weather )
{
    std::vector<std::string> lines = {
        accessibility::format_overmap_tile( overmap_tile( viewer, position ), false )
    };
    const om_vision_level vision = overmap_buffer.seen( position );
    if( vision != om_vision_level::unseen ) {
        // The description names the nearest known city, or else the terrain's origin.
        const std::string description = remove_color_tags( overmap_buffer.get_description_at(
                                            project_to<coords::sm>( position ) ) );
        for( const std::string &line : string_split( description, '\n' ) ) {
            lines.push_back( accessibility::format_semantic_sequence( { line } ) );
        }
        lines.push_back( overmap_vision_text( vision ) );
    }
    const tripoint_abs_omt target = viewer.get_active_mission_target();
    if( !target.is_invalid() ) {
        const int distance = rl_dist( position, target );
        std::string location = accessibility::format_mission_location(
                                   _( "Current objective from the cursor" ), distance,
                                   direction_name( direction_from( position.xy().raw(), target.xy().raw() ) ),
                                   // One map tile is 24 meters across.
                                   length_to_string_approx( distance * 24_meter ) );
        if( target.z() != position.z() ) {
            location = accessibility::format_semantic_sequence( {
                location, accessibility::format_overmap_offset( 0, 0, target.z() - position.z() )
            } );
        }
        lines.push_back( location );
    }
    lines.push_back( weather );
    if( overmap_buffer.is_explored( position ) ) {
        lines.push_back( accessibility::format_overmap_explored( true ) );
    }
    lines.push_back( accessibility::format_semantic_sequence( {
        accessibility::format_overmap_level( position.z() )
    } ) );
    lines.erase( std::remove( lines.begin(), lines.end(), std::string() ), lines.end() );
    return lines;
}

std::string overmap_route_state( const avatar &viewer )
{
    if( viewer.omt_path.empty() ) {
        return accessibility::format_overmap_route_state( std::string(), std::string() );
    }
    const tripoint_abs_omt &destination = viewer.omt_path.front();
    const std::string terrain = overmap_known_terrain( destination );
    return accessibility::format_overmap_route_state(
               terrain.empty() ? std::string( _( "unexplored" ) ) : terrain,
               overmap_location( viewer.pos_abs_omt(), destination ) );
}

std::vector<std::pair<tripoint_abs_omt, accessibility::known_place>> known_overmap_places(
            const avatar &viewer )
{
    std::vector<std::pair<tripoint_abs_omt, accessibility::known_place>> places;
    const auto place_at = [&places]( const tripoint_abs_omt & position ) ->
    accessibility::known_place & {
        const auto found = std::find_if( places.begin(), places.end(),
                                         [&position]( const std::pair<tripoint_abs_omt, accessibility::known_place> &place )
        {
            return place.first == position;
        } );
        if( found != places.end() )
        {
            return found->second;
        }
        places.emplace_back( position, accessibility::known_place() );
        return places.back().second;
    };
    const tripoint_abs_omt origin = viewer.pos_abs_omt();
    const tripoint_abs_sm center = project_to<coords::sm>( origin );
    // Five overmaps in each direction, like the overmap search; two submaps per map tile.
    const int radius = 2 * OMAPX * 5;
    for( const city_reference &ref : overmap_buffer.get_cities_near( center, radius ) ) {
        const tripoint_abs_omt position( project_to<coords::omt>( ref.abs_sm_pos.xy() ), 0 );
        // The overmap labels only cities whose center has been seen.
        if( overmap_buffer.seen_more_than( position, om_vision_level::outlines ) ) {
            accessibility::known_place &place = place_at( position );
            place.city = true;
            place.name = ref.city->name;
        }
    }
    for( const camp_reference &ref : overmap_buffer.get_camps_near( center, radius ) ) {
        const tripoint_abs_omt position = ref.camp->camp_omt_pos();
        if( overmap_buffer.seen_more_than( position, om_vision_level::outlines ) ) {
            accessibility::known_place &place = place_at( position );
            place.camp = true;
            place.name = ref.camp->camp_name();
        }
    }
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        for( const overmapbuffer::t_point_with_note &note : overmap_buffer.get_all_notes( z ) ) {
            const tripoint_abs_omt position( note.first, z );
            accessibility::known_place &place = place_at( position );
            place.note = overmap_note_text( position );
            place.danger_radius = overmap_buffer.note_danger_radius( position );
            place.in_dangerous_area = place.danger_radius < 0 &&
                                      overmap_buffer.is_marked_dangerous( position );
        }
    }
    const mission *objective = viewer.get_active_mission();
    for( const mission *miss : viewer.get_active_missions() ) {
        if( miss->has_target() ) {
            accessibility::known_place &place = place_at( miss->get_target() );
            place.missions.push_back( miss->name() );
            place.current_objective = place.current_objective || miss == objective;
        }
    }
    if( !viewer.omt_path.empty() ) {
        place_at( viewer.omt_path.front() ).route_destination = true;
    }
    for( std::pair<tripoint_abs_omt, accessibility::known_place> &entry : places ) {
        accessibility::known_place &place = entry.second;
        if( place.name.empty() ) {
            place.name = overmap_known_terrain( entry.first );
            if( place.name.empty() ) {
                place.name = _( "unexplored" );
            }
        }
        place.distance = rl_dist( origin.xy(), entry.first.xy() );
        place.direction = direction_name( direction_from( origin.xy().raw(), entry.first.xy().raw() ) );
        place.dz = entry.first.z() - origin.z();
    }
    std::stable_sort( places.begin(), places.end(), []( const auto & lhs, const auto & rhs ) {
        return std::make_tuple( lhs.second.distance, std::abs( lhs.second.dz ) ) <
               std::make_tuple( rhs.second.distance, std::abs( rhs.second.dz ) );
    } );
    return places;
}

std::optional<tripoint_abs_omt> choose_known_overmap_place( const avatar &viewer )
{
    const std::vector<std::pair<tripoint_abs_omt, accessibility::known_place>> places =
                known_overmap_places( viewer );
    const std::string title = _( "Known places" );
    if( places.empty() ) {
        accessibility::announce( accessibility::format_semantic_sequence( {
            string_join( std::vector<std::string> { title, accessibility::format_known_place_count( 0 ) },
            _( ", " ) )
        } ) );
        return std::nullopt;
    }
    uilist menu;
    menu.title = title;
    menu.accessibility_count = []( const size_t count ) {
        return accessibility::format_known_place_count( count );
    };
    for( size_t index = 0; index < places.size(); ++index ) {
        const std::string row = accessibility::format_known_place_row( places[index].second );
        menu.addentry( static_cast<int>( index ), true, 0, row );
        menu.entries.back().accessibility_focus = accessibility::format_semantic_sequence( { row } );
    }
    menu.query();
    if( menu.ret >= 0 && static_cast<size_t>( menu.ret ) < places.size() ) {
        return places[menu.ret].first;
    }
    return std::nullopt;
}

void announce_overmap_route( const avatar &viewer, const tripoint_abs_omt &destination,
                             const std::string &confirm_key )
{
    if( !accessibility::is_active() ) {
        return;
    }
    const std::string terrain = overmap_known_terrain( destination );
    const std::string name = terrain.empty() ? std::string( _( "unexplored" ) ) : terrain;
    if( viewer.omt_path.empty() ) {
        accessibility::announce( accessibility::format_overmap_no_route( name,
                                 destination.z() != viewer.posz() ) );
        return;
    }
    // The path is stored destination first and may end with the current tile.
    int length = static_cast<int>( viewer.omt_path.size() );
    if( viewer.omt_path.back() == viewer.pos_abs_omt() ) {
        --length;
    }
    accessibility::announce( accessibility::format_overmap_route_preview( name,
                             overmap_location( viewer.pos_abs_omt(), destination ), length, confirm_key ) );
}

std::string overmap_note_row( const avatar &viewer, const tripoint_abs_omt &position )
{
    accessibility::known_place place;
    place.name = overmap_note_text( position );
    const tripoint_abs_omt origin = viewer.pos_abs_omt();
    place.distance = rl_dist( origin.xy(), position.xy() );
    place.direction = direction_name( direction_from( origin.xy().raw(), position.xy().raw() ) );
    place.dz = position.z() - origin.z();
    place.danger_radius = overmap_buffer.note_danger_radius( position );
    place.in_dangerous_area = place.danger_radius < 0 && overmap_buffer.is_marked_dangerous( position );
    const std::string terrain = overmap_known_terrain( position );
    return accessibility::format_semantic_sequence( {
        string_join( std::vector<std::string> {
            accessibility::format_known_place_row( place ),
            terrain.empty() ? std::string( _( "unexplored" ) ) : terrain
        }, _( ", " ) )
    } );
}

void overmap_speech::open( const avatar &viewer, const std::string &opening_prompt,
                           const tripoint_abs_omt &cursor )
{
    prompt = opening_prompt;
    sync( viewer, cursor );
    if( accessibility::is_active() ) {
        accessibility::announce( accessibility::format_overmap_opening( prompt,
                                 accessibility::format_overmap_tile( overmap_tile( viewer, cursor ), false ) ) );
    }
}

void overmap_speech::update( const avatar &viewer, const tripoint_abs_omt &cursor )
{
    if( cursor.raw() == spoken ) {
        return;
    }
    const tripoint previous = spoken;
    const std::string previous_terrain = terrain;
    sync( viewer, cursor );
    if( !accessibility::is_active() ) {
        return;
    }
    const accessibility::overmap_tile tile = overmap_tile( viewer, cursor );
    if( !previous.is_invalid() && previous.z != cursor.z() ) {
        accessibility::announce( accessibility::format_overmap_level_change( cursor.z(),
                                 accessibility::format_overmap_tile( tile, false ) ) );
        return;
    }
    const bool one_step = !previous.is_invalid() &&
                          std::abs( previous.x - cursor.x() ) <= 1 && std::abs( previous.y - cursor.y() ) <= 1;
    accessibility::announce( accessibility::format_overmap_tile( tile, one_step &&
                             previous_terrain == terrain ) );
}

void overmap_speech::announce( const avatar &viewer, const tripoint_abs_omt &cursor )
{
    const bool level_changed = !spoken.is_invalid() && spoken.z != cursor.z();
    sync( viewer, cursor );
    if( !accessibility::is_active() ) {
        return;
    }
    const std::string tile = accessibility::format_overmap_tile( overmap_tile( viewer, cursor ),
                             false );
    accessibility::announce( level_changed ? accessibility::format_overmap_level_change( cursor.z(),
                             tile ) : tile );
}

void overmap_speech::sync( const avatar &, const tripoint_abs_omt &cursor )
{
    spoken = cursor.raw();
    terrain = overmap_known_terrain( cursor );
}

void overmap_speech::summary( const avatar &viewer, const tripoint_abs_omt &cursor ) const
{
    accessibility::announce( accessibility::format_overmap_summary( prompt,
                             accessibility::format_overmap_tile( overmap_tile( viewer, cursor ), false ), cursor.z(),
                             overmap_route_state( viewer ) ) );
}

} // namespace accessibility_gameplay
