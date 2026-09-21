#include "accessibility.h"

#include <algorithm>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <tuple>
#include <utility>

#include "string_formatter.h"
#include "translations.h"

namespace accessibility
{

std::unique_ptr<backend> make_platform_backend();

namespace
{

class null_backend final : public backend
{
    public:
        bool is_available() const override {
            return false;
        }

        bool announce( const std::string & ) override {
            return false;
        }
};

std::unique_ptr<backend> &active_backend()
{
    static std::unique_ptr<backend> instance = make_platform_backend();
    if( !instance ) {
        instance = std::make_unique<null_backend>();
    }
    return instance;
}

std::string &last_message()
{
    static std::string message;
    return message;
}

bool &game_messages_enabled()
{
    static bool enabled = false;
    return enabled;
}

std::set<std::string> &previous_threats()
{
    static std::set<std::string> threats;
    return threats;
}

bool &threats_initialized()
{
    static bool initialized = false;
    return initialized;
}

std::string &previous_movement_direction()
{
    static std::string direction;
    return direction;
}

} // namespace

void sort_semantic_list_entries( std::vector<semantic_list_entry> &entries )
{
    std::sort( entries.begin(), entries.end(),
    []( const semantic_list_entry & lhs, const semantic_list_entry & rhs ) {
        return std::tie( lhs.distance, lhs.z, lhs.y, lhs.x, lhs.kind, lhs.label ) <
               std::tie( rhs.distance, rhs.z, rhs.y, rhs.x, rhs.kind, rhs.label );
    } );
}

std::vector<connected_semantic_list_group> group_connected_semantic_list_entries(
    std::vector<semantic_list_entry> entries )
{
    sort_semantic_list_entries( entries );

    using entry_key = std::tuple<int, int, int, int, std::string>;
    std::map<entry_key, size_t> entries_by_position;
    for( size_t index = 0; index < entries.size(); ++index ) {
        const semantic_list_entry &entry = entries[index];
        entries_by_position.emplace( entry_key( entry.z, entry.y, entry.x, entry.kind, entry.label ),
                                     index );
    }

    std::vector<bool> visited( entries.size(), false );
    std::vector<connected_semantic_list_group> groups;
    for( size_t index = 0; index < entries.size(); ++index ) {
        if( visited[index] ) {
            continue;
        }

        visited[index] = true;
        size_t count = 0;
        std::vector<size_t> pending = { index };
        while( !pending.empty() ) {
            const size_t current_index = pending.back();
            pending.pop_back();
            ++count;

            const semantic_list_entry &current = entries[current_index];
            for( int y_offset = -1; y_offset <= 1; ++y_offset ) {
                for( int x_offset = -1; x_offset <= 1; ++x_offset ) {
                    if( x_offset == 0 && y_offset == 0 ) {
                        continue;
                    }
                    const entry_key neighbor_key( current.z, current.y + y_offset,
                                                  current.x + x_offset, current.kind, current.label );
                    const auto neighbor = entries_by_position.find( neighbor_key );
                    if( neighbor != entries_by_position.end() && !visited[neighbor->second] ) {
                        visited[neighbor->second] = true;
                        pending.push_back( neighbor->second );
                    }
                }
            }
        }
        groups.push_back( { entries[index], count } );
    }
    return groups;
}

bool is_available()
{
    return active_backend()->is_available();
}

bool announce( const std::string &message )
{
    if( message.empty() ) {
        return false;
    }

    last_message() = message;
    return active_backend()->announce( message );
}

bool announce_focus( const std::string &label, const std::string &context )
{
    if( label.empty() ) {
        return announce( context );
    }
    if( context.empty() ) {
        return announce( label );
    }

    return announce( context + ". " + label );
}

bool announce_status( const std::string &message )
{
    return announce( message );
}

bool announce_game_message( const std::string &message )
{
    if( !game_messages_enabled() ) {
        return false;
    }
    return announce( message );
}

bool announce_movement( const std::string &direction, const std::string &tile )
{
    if( !game_messages_enabled() ) {
        return false;
    }

    std::string message = tile;
    if( direction != previous_movement_direction() ) {
        if( !message.empty() && !direction.empty() ) {
            message += ". ";
        }
        message += direction;
        previous_movement_direction() = direction;
    }
    return announce( message );
}

bool announce_threats( const std::vector<std::string> &threat_ids, const std::string &message )
{
    if( !game_messages_enabled() ) {
        return false;
    }

    const std::set<std::string> current_threats( threat_ids.begin(), threat_ids.end() );
    if( !threats_initialized() ) {
        threats_initialized() = true;
        previous_threats() = current_threats;
        return current_threats.empty() ? false : announce( message );
    }
    if( current_threats == previous_threats() ) {
        return false;
    }

    previous_threats() = current_threats;
    return announce( message );
}

std::string format_semantic_sequence( const std::vector<std::string> &parts )
{
    std::string result;
    for( const std::string &part : parts ) {
        const std::string normalized = normalize_status_announcement( part, false );
        if( normalized.empty() ) {
            continue;
        }
        if( !result.empty() ) {
            const char last = result.back();
            if( last != '.' && last != '!' && last != '?' && last != ':' && last != ';' ) {
                result += '.';
            }
            result += ' ';
        }
        result += normalized;
    }
    if( !result.empty() ) {
        const char last = result.back();
        if( last != '.' && last != '!' && last != '?' ) {
            result += '.';
        }
    }
    return result;
}

bool announce_semantic_sequence( const std::vector<std::string> &parts )
{
    return announce( format_semantic_sequence( parts ) );
}

std::string format_creature_attitude( const std::string &attitude )
{
    if( attitude.empty() ) {
        return {};
    }
    return string_format( _( "Attitude: %s" ), attitude );
}

std::string format_creature_health( const int current, const int maximum )
{
    if( maximum <= 0 ) {
        return {};
    }
    if( current >= maximum ) {
        return _( "Health: uninjured" );
    }
    if( current >= maximum * 0.8 ) {
        return _( "Health: lightly injured" );
    }
    if( current >= maximum * 0.6 ) {
        return _( "Health: moderately injured" );
    }
    if( current >= maximum * 0.3 ) {
        return _( "Health: heavily injured" );
    }
    if( current >= maximum * 0.1 ) {
        return _( "Health: severely injured" );
    }
    return _( "Health: nearly dead" );
}

std::string format_nearby_list_entry( const nearby_list_entry &entry )
{
    std::vector<std::string> parts = { entry.name };
    if( entry.count > 0 ) {
        parts.push_back( string_format( _( "Count: %d" ), entry.count ) );
    }
    if( entry.position_count > 1 && entry.position_index > 0 ) {
        parts.push_back( string_format( _( "Location %1$d of %2$d" ), entry.position_index,
                                        entry.position_count ) );
    }
    parts.push_back( entry.location );
    parts.insert( parts.end(), entry.details.begin(), entry.details.end() );
    if( entry.result_count > 0 && entry.result_index > 0 ) {
        parts.push_back( string_format( _( "%1$d of %2$d" ), entry.result_index,
                                        entry.result_count ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_nearby_list_context( const std::string &mode, const int result_count,
                                        const std::string &change )
{
    std::vector<std::string> parts = { mode, change };
    if( result_count == 0 ) {
        parts.push_back( _( "No results" ) );
    } else {
        parts.push_back( string_format( n_gettext( "%d result", "%d results", result_count ),
                                        result_count ) );
    }
    return format_semantic_sequence( parts );
}

bool announce_nearby_list_focus( const nearby_list_entry &entry, const std::string &context )
{
    return announce_semantic_sequence( { context, format_nearby_list_entry( entry ) } );
}

bool repeat_last_announcement()
{
    if( last_message().empty() ) {
        return false;
    }

    return active_backend()->announce( last_message() );
}

std::string last_announcement()
{
    return last_message();
}

void set_last_announcement( const std::string &message )
{
    if( !message.empty() ) {
        last_message() = message;
    }
}

std::string normalize_status_announcement( const std::string &message, const bool progress )
{
    std::string result = message;
    if( progress ) {
        static const std::regex fraction( R"(\s*\[\s*\d+\s*/\s*\d+\s*\])" );
        static const std::regex percentage( R"(\s*\d+(\.\d+)?\s*%)" );
        result = std::regex_replace( result, fraction, "" );
        result = std::regex_replace( result, percentage, "" );
    }
    static const std::regex whitespace( R"(\s+)" );
    result = std::regex_replace( result, whitespace, " " );
    const size_t first = result.find_first_not_of( ' ' );
    if( first == std::string::npos ) {
        return {};
    }
    const size_t last = result.find_last_not_of( ' ' );
    return result.substr( first, last - first + 1 );
}

void enable_game_messages( const bool enabled )
{
    if( game_messages_enabled() != enabled ) {
        previous_threats().clear();
        threats_initialized() = false;
        previous_movement_direction().clear();
    }
    game_messages_enabled() = enabled;
}

condition_tracker &global_condition_tracker()
{
    static condition_tracker tracker;
    return tracker;
}

std::vector<std::string> evaluate_condition_transitions( condition_tracker &tracker,
                                                         const condition_tracker &current )
{
    if( !tracker.initialized ) {
        tracker = current;
        tracker.initialized = true;
        return {};
    }

    std::vector<std::string> alerts;

    // Bleeding started
    for( const std::string &limb : current.bleeding_limbs ) {
        if( std::find( tracker.bleeding_limbs.begin(), tracker.bleeding_limbs.end(), limb ) ==
            tracker.bleeding_limbs.end() ) {
            alerts.push_back( string_format( _( "Bleeding: %s!" ), limb ) );
        }
    }
    // Bleeding stopped
    for( const std::string &limb : tracker.bleeding_limbs ) {
        if( std::find( current.bleeding_limbs.begin(), current.bleeding_limbs.end(), limb ) ==
            current.bleeding_limbs.end() ) {
            alerts.push_back( string_format( _( "Stopped bleeding: %s." ), limb ) );
        }
    }

    // Broken limbs
    for( const std::string &limb : current.broken_limbs ) {
        if( std::find( tracker.broken_limbs.begin(), tracker.broken_limbs.end(), limb ) ==
            tracker.broken_limbs.end() ) {
            alerts.push_back( string_format( _( "Broken limb: %s!" ), limb ) );
        }
    }

    // Stamina critical drop
    if( current.stamina_critical && !tracker.stamina_critical ) {
        alerts.push_back( _( "Stamina critical!" ) );
    }

    // Needs & pain & weariness transitions
    if( !current.pain_tier.empty() && current.pain_tier != tracker.pain_tier ) {
        alerts.push_back( string_format( _( "Pain: %s." ), current.pain_tier ) );
    }
    if( !current.hunger_tier.empty() && current.hunger_tier != tracker.hunger_tier ) {
        alerts.push_back( string_format( _( "Hunger: %s." ), current.hunger_tier ) );
    }
    if( !current.thirst_tier.empty() && current.thirst_tier != tracker.thirst_tier ) {
        alerts.push_back( string_format( _( "Thirst: %s." ), current.thirst_tier ) );
    }
    if( !current.sleepiness_tier.empty() && current.sleepiness_tier != tracker.sleepiness_tier ) {
        alerts.push_back( string_format( _( "Sleepiness: %s." ), current.sleepiness_tier ) );
    }
    if( !current.weariness_tier.empty() && current.weariness_tier != tracker.weariness_tier ) {
        alerts.push_back( string_format( _( "Weariness: %s." ), current.weariness_tier ) );
    }

    tracker = current;
    tracker.initialized = true;
    return alerts;
}

bool announce_condition_transitions( condition_tracker &tracker,
                                     const condition_tracker &current )
{
    const std::vector<std::string> alerts = evaluate_condition_transitions( tracker, current );
    if( alerts.empty() ) {
        return false;
    }
    return announce_semantic_sequence( alerts );
}

void reset_condition_tracker()
{
    global_condition_tracker() = condition_tracker{};
}

bool check_and_announce_condition_transitions( const condition_tracker &current )
{
    return announce_condition_transitions( global_condition_tracker(), current );
}

void set_backend_for_testing( std::unique_ptr<backend> replacement )
{
    active_backend() = replacement ? std::move( replacement ) : std::make_unique<null_backend>();
    last_message().clear();
    previous_threats().clear();
    threats_initialized() = false;
    previous_movement_direction().clear();
    reset_condition_tracker();
}

void reset_backend_after_testing()
{
    active_backend() = make_platform_backend();
    last_message().clear();
    game_messages_enabled() = false;
    previous_threats().clear();
    threats_initialized() = false;
    previous_movement_direction().clear();
    reset_condition_tracker();
}

} // namespace accessibility
