#include "accessibility.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <tuple>
#include <utility>

#include "cata_utility.h"
#include "catacharset.h"
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

void message_log_review::set_entries( std::vector<message_log_entry> entries,
                                      const bool newest_first )
{
    entries_ = std::move( entries );
    newest_first_ = newest_first;
    matches_.clear();
    for( const message_log_entry &entry : entries_ ) {
        matches_.push_back( entry.source_index );
    }
    focus_newest();
}

bool message_log_review::set_filter( const std::string &query,
                                     const std::vector<std::string> &types )
{
    std::string text = query;
    std::string type;
    const size_t colon = query.find( ':' );
    if( colon != std::string::npos ) {
        type = query.substr( 0, colon );
        text = query.substr( colon + 1 );
        if( !type.empty() && std::find( types.begin(), types.end(), type ) == types.end() ) {
            return false;
        }
    }

    const message_log_entry *previous = focused();
    const std::optional<size_t> previous_source = previous ?
            std::optional<size_t>( previous->source_index ) : std::nullopt;
    matches_.clear();
    for( const message_log_entry &entry : entries_ ) {
        if( ( type.empty() || entry.type == type ) && lcmatch( entry.text, text ) ) {
            matches_.push_back( entry.source_index );
        }
    }
    if( !previous_source || !focus_source( *previous_source ) ) {
        focus_newest();
    }
    return true;
}

const std::vector<size_t> &message_log_review::matches() const
{
    return matches_;
}

const message_log_entry *message_log_review::focused() const
{
    if( matches_.empty() ) {
        return nullptr;
    }
    const size_t source_index = matches_[focused_];
    const auto found = std::find_if( entries_.begin(), entries_.end(),
    [source_index]( const message_log_entry & entry ) {
        return entry.source_index == source_index;
    } );
    return found == entries_.end() ? nullptr : &*found;
}

size_t message_log_review::focused_position() const
{
    return matches_.empty() ? 0 : focused_ + 1;
}

bool message_log_review::focus_source( const size_t source_index )
{
    const auto found = std::find( matches_.begin(), matches_.end(), source_index );
    if( found == matches_.end() ) {
        return false;
    }
    focused_ = std::distance( matches_.begin(), found );
    return true;
}

void message_log_review::focus_newest()
{
    focused_ = newest_first_ || matches_.empty() ? 0 : matches_.size() - 1;
}

bool message_log_review::move( const int delta )
{
    if( matches_.empty() || delta == 0 ) {
        return false;
    }
    const size_t old = focused_;
    if( delta > 0 ) {
        focused_ = std::min( matches_.size() - 1, focused_ + static_cast<size_t>( delta ) );
    } else {
        const size_t step = static_cast<size_t>( -( static_cast<int64_t>( delta ) ) );
        focused_ = step > focused_ ? 0 : focused_ - step;
    }
    return focused_ != old;
}

bool message_log_review::page( const int direction, const size_t page_size )
{
    if( matches_.empty() || direction == 0 ) {
        return false;
    }
    const size_t old = focused_;
    const size_t step = std::max<size_t>( 1, page_size );
    if( direction > 0 ) {
        focused_ = std::min( matches_.size() - 1, focused_ + step );
    } else {
        focused_ = step > focused_ ? 0 : focused_ - step;
    }
    return focused_ != old;
}

std::string format_message_log_focus( const message_log_entry &entry )
{
    return format_semantic_sequence( { entry.text, entry.elapsed } );
}

std::string format_message_log_detail( const message_log_entry &entry, const size_t position,
                                       const size_t count )
{
    return format_semantic_sequence( {
        string_format( _( "Message %d of %d" ), static_cast<int>( position ), static_cast<int>( count ) ),
        entry.spoken_type, entry.text, entry.elapsed
    } );
}

std::string format_message_log_open( const size_t count, const message_log_entry *entry )
{
    const std::string count_text = string_format( n_gettext( "%d message", "%d messages", count ),
                                   static_cast<int>( count ) );
    return format_semantic_sequence( { _( "Message log" ), count_text,
                                       entry ? format_message_log_focus( *entry ) : _( "Empty" ) } );
}

std::string format_message_log_filter( const std::string &query, const size_t count,
                                       const message_log_entry *entry )
{
    const std::string filter = query.empty() ? _( "Filter cleared" ) :
                               string_format( _( "Filter: %s" ), query );
    const std::string count_text = string_format( n_gettext( "%d message", "%d messages", count ),
                                   static_cast<int>( count ) );
    return format_semantic_sequence( { filter, count_text,
                                       entry ? format_message_log_focus( *entry ) : _( "No matching messages" ) } );
}

std::string format_message_log_filter_field( const std::string &query,
        const std::vector<std::string> &types )
{
    std::string type_list;
    for( const std::string &type : types ) {
        if( !type_list.empty() ) {
            type_list += ", ";
        }
        type_list += type;
    }
    return format_semantic_sequence( { _( "Message log filter" ),
                                       query.empty() ? _( "Empty" ) : string_format( _( "Current query: %s" ), query ),
                                       _( "Search text or use type:text" ),
                                       type_list.empty() ? std::string() : string_format( _( "Types: %s" ), type_list ),
                                       _( "Enter to apply, Escape to cancel" ) } );
}

std::string format_message_log_filter_value( const std::string &query )
{
    return query.empty() ? _( "Filter text empty" ) : string_format( _( "Filter text: %s" ), query );
}

std::string format_message_log_invalid_type( const std::string &type )
{
    return string_format( _( "Unknown message type: %s" ), type );
}

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

std::optional<std::string> evaluate_threat_transition(
    const std::vector<std::string> &threat_ids, const std::string &message )
{
    if( !game_messages_enabled() ) {
        return std::nullopt;
    }

    const std::set<std::string> current_threats( threat_ids.begin(), threat_ids.end() );
    if( !threats_initialized() ) {
        threats_initialized() = true;
        previous_threats() = current_threats;
        return current_threats.empty() ? std::nullopt : std::optional<std::string>( message );
    }
    if( current_threats == previous_threats() ) {
        return std::nullopt;
    }

    previous_threats() = current_threats;
    return message;
}

bool announce_threats( const std::vector<std::string> &threat_ids, const std::string &message )
{
    const std::optional<std::string> transition = evaluate_threat_transition( threat_ids, message );
    return transition && announce( *transition );
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

std::string format_main_menu_world( const std::string &name, const size_t save_count )
{
    return format_semantic_sequence( {
        name,
        string_format( n_gettext( "%d save", "%d saves", save_count ),
                       static_cast<int>( save_count ) )
    } );
}

std::string format_main_menu_focus( const std::string &menu_name, const std::string &tab,
                                    const std::string &focused, const std::string &title,
                                    const std::string &version, const std::string &tip )
{
    return format_semantic_sequence( { title, version, tip, menu_name, tab, focused } );
}

std::vector<semantic_document_line> select_document_lines(
    const std::vector<std::string> &source_lines )
{
    std::vector<semantic_document_line> lines;
    for( size_t index = 0; index < source_lines.size(); ++index ) {
        const std::string &source = source_lines[index];
        const size_t first = source.find_first_not_of( " \t\r" );
        if( first == std::string::npos ) {
            continue;
        }
        const size_t last = source.find_last_not_of( " \t\r" );
        const std::string text = source.substr( first, last - first + 1 );
        if( text.find_first_not_of( '*' ) != std::string::npos ) {
            lines.push_back( { text, index } );
        }
    }
    return lines;
}

int move_document_cursor( const int current, const int line_count, const int delta )
{
    return line_count <= 0 ? 0 : std::clamp( current + delta, 0, line_count - 1 );
}

document_cursor_memory::document_cursor_memory( const size_t document_count ) :
    positions_( document_count, 0 )
{
}

int document_cursor_memory::position( const size_t document ) const
{
    return positions_.at( document );
}

int document_cursor_memory::move( const size_t document, const int line_count, const int delta )
{
    int &position = positions_.at( document );
    position = move_document_cursor( position, line_count, delta );
    return position;
}

void document_cursor_memory::reset()
{
    std::fill( positions_.begin(), positions_.end(), 0 );
}

std::string format_document_focus( const std::string &title, const std::string &text )
{
    if( title.empty() ) {
        return text;
    }
    if( text.empty() ) {
        return title;
    }
    return string_format( "%s. %s", title, text );
}

std::string format_help_topic_focus( const std::string &hotkey, const std::string &topic_name )
{
    if( hotkey.empty() ) {
        return topic_name;
    }
    if( topic_name.empty() ) {
        return hotkey;
    }
    return string_format( "%s: %s", hotkey, topic_name );
}

std::string format_help_menu_open( const size_t topic_count, const std::string &initial_hotkey,
                                   const std::string &initial_topic )
{
    if( topic_count == 0 ) {
        return _( "Help. 0 topics. Escape to exit." );
    }
    const std::string topic_str = format_help_topic_focus( initial_hotkey, initial_topic );
    return string_format(
               _( "Help. %1$d topics. Use Up and Down to browse, hotkey or Enter to open, Escape to exit. %2$s" ),
               static_cast<int>( topic_count ), topic_str );
}

std::string format_help_directions(
    const std::vector<std::pair<std::string, std::vector<std::string>>> &direction_bindings )
{
    if( direction_bindings.empty() ) {
        return {};
    }
    std::vector<std::string> parts;
    parts.reserve( direction_bindings.size() );
    for( const auto &entry : direction_bindings ) {
        std::string keys_str;
        if( entry.second.empty() ) {
            keys_str = _( "unbound" );
        } else {
            for( size_t i = 0; i < entry.second.size(); ++i ) {
                if( i > 0 ) {
                    keys_str += ", ";
                }
                keys_str += entry.second[i];
            }
        }
        parts.push_back( string_format( "%s: %s", entry.first, keys_str ) );
    }

    std::string result = _( "Movement keys: " );
    for( size_t i = 0; i < parts.size(); ++i ) {
        if( i > 0 ) {
            result += "; ";
        }
        result += parts[i];
    }
    result += ".";
    return result;
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

std::string format_uilist_focus( const std::string &label, const std::string &column,
                                 const bool unavailable )
{
    return format_semantic_sequence( { label, column,
                                       unavailable ? _( "Unavailable" ) : std::string() } );
}

std::string format_uilist_open( const std::string &context, const std::string &focus,
                                const std::string &details_binding, const bool has_details )
{
    return format_semantic_sequence( { context, _( "List. Use arrow keys to choose" ),
                                       has_details ? string_format( _( "%s for details" ),
                                               details_binding ) : std::string(), focus } );
}

std::string format_popup_option_focus( const std::string &option )
{
    return string_format( _( "%s selected" ), option );
}

std::string format_popup_prompt( const std::string &question, const std::string &selected,
                                 const std::vector<std::string> &choices,
                                 const std::string &left_binding,
                                 const std::string &right_binding,
                                 const std::string &confirm_binding, const bool include_hint )
{
    return format_semantic_sequence( {
        question,
        choices.size() >= 3 ? string_format( _( "Choices: %s" ), string_join( choices, _( ", " ) ) ) :
        std::string(),
        selected.empty() ? std::string() : format_popup_option_focus( selected ),
        include_hint ? string_format( _( "Choose with %1$s or %2$s; confirm with %3$s" ),
                                      left_binding, right_binding, confirm_binding ) : std::string()
    } );
}

std::string format_text_input( const std::string &prompt, const std::string &description,
                               const std::string &value )
{
    return format_semantic_sequence( { prompt, description,
                                       string_format( _( "Value: %s" ), value.empty() ? _( "empty" ) : value ) } );
}

std::string format_text_character( const std::string &character )
{
    if( character == " " ) {
        return _( "Space" );
    }
    return character == "-" ? _( "Minus" ) : character;
}

std::string format_text_cursor( const std::string &value, const size_t cursor )
{
    const utf8_wrapper characters( value );
    if( cursor >= characters.length() ) {
        return _( "End of field" );
    }
    return format_text_character( characters.substr( cursor, 1 ).str() );
}

bool uilist_details_need_document( const std::string &details )
{
    return utf8_width( details ) > 160;
}

std::vector<std::string> split_uilist_detail_sentences( const std::string &details )
{
    std::string normalized;
    normalized.reserve( details.size() );
    for( const unsigned char ch : details ) {
        if( std::isspace( ch ) ) {
            if( !normalized.empty() && normalized.back() != ' ' ) {
                normalized.push_back( ' ' );
            }
        } else {
            normalized.push_back( static_cast<char>( ch ) );
        }
    }
    if( !normalized.empty() && normalized.back() == ' ' ) {
        normalized.pop_back();
    }

    std::vector<std::string> sentences;
    size_t start = 0;
    for( size_t i = 0; i < normalized.size(); ++i ) {
        const char ch = normalized[i];
        if( ch != '.' && ch != '!' && ch != '?' ) {
            continue;
        }
        size_t end = i + 1;
        while( end < normalized.size() &&
               ( normalized[end] == '"' || normalized[end] == '\'' || normalized[end] == ')' ) ) {
            ++end;
        }
        if( end < normalized.size() && normalized[end] != ' ' ) {
            continue;
        }
        const size_t next = std::min( end + 1, normalized.size() );
        if( ch == '.' && next < normalized.size() &&
            std::islower( static_cast<unsigned char>( normalized[next] ) ) ) {
            continue;
        }
        sentences.push_back( normalized.substr( start, end - start ) );
        start = next;
        i = end;
    }
    if( start < normalized.size() ) {
        sentences.push_back( normalized.substr( start ) );
    }
    return sentences;
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

std::string format_safe_mode_status( const safe_mode_state state, const bool auto_safe_mode,
                                     const int turns_until_auto_safe )
{
    if( state == safe_mode_state::stopped ) {
        return _( "Stopped by threat" );
    }
    if( state == safe_mode_state::on ) {
        return _( "On" );
    }
    if( !auto_safe_mode ) {
        return _( "Off" );
    }
    if( turns_until_auto_safe <= 0 ) {
        return _( "Off; auto-safe pending" );
    }
    return string_format( n_gettext( "Off; auto-safe in %d turn",
                                     "Off; auto-safe in %d turns",
                                     turns_until_auto_safe ), turns_until_auto_safe );
}

std::vector<status_summary_entry> format_status_summary( const status_snapshot &snapshot )
{
    std::vector<status_summary_entry> entries;
    entries.reserve( 7 );

    std::string health = snapshot.health + ", " + snapshot.pain;
    if( snapshot.bleeding ) {
        health += ", ";
        health += _( "Bleeding!" );
    }
    if( snapshot.broken_limb ) {
        health += ", ";
        health += _( "Broken limb!" );
    }
    entries.push_back( { _( "Health & Pain" ), health } );

    entries.push_back( {
        _( "Stamina & Condition" ),
        string_format( _( "Stamina: %1$d%%, Focus: %2$d, %3$s: %4$s" ),
                       snapshot.stamina_percent, snapshot.focus, _( "Weariness" ), snapshot.weariness )
    } );
    entries.push_back( {
        _( "Needs" ),
        string_format( _( "Hunger: %1$s, Thirst: %2$s, Sleepiness: %3$s" ),
                       snapshot.hunger, snapshot.thirst, snapshot.sleepiness )
    } );

    std::string temperature = string_format( _( "Body: %1$s, Env: %2$s" ),
                              snapshot.body_temperature, snapshot.environment_temperature );
    if( snapshot.active_effect_count > 0 ) {
        const int effect_count = static_cast<int>( snapshot.active_effect_count );
        temperature += ", " + string_format(
                           n_gettext( "%d effect", "%d effects", effect_count ), effect_count );
    }
    entries.push_back( { _( "Temperature & Effects" ), temperature } );
    entries.push_back( { _( "Wielded weapon" ), snapshot.wielded_weapon } );
    entries.push_back( {
        _( "Movement & Safety" ),
        string_format( _( "Mode: %1$s, Speed: %2$d, Safe mode: %3$s" ),
                       snapshot.movement_mode, snapshot.speed, snapshot.safe_mode )
    } );
    entries.push_back( {
        _( "Time & Environment" ),
        string_format( _( "%1$s, %2$s, %3$s" ), snapshot.time, snapshot.date, snapshot.weather )
    } );
    return entries;
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

std::vector<std::string> evaluate_condition_transitions( const condition_tracker &current )
{
    return evaluate_condition_transitions( global_condition_tracker(), current );
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
    return announce_semantic_sequence( evaluate_condition_transitions( current ) );
}

void reset_game_state()
{
    previous_threats().clear();
    threats_initialized() = false;
    previous_movement_direction().clear();
    reset_condition_tracker();
}

void set_backend_for_testing( std::unique_ptr<backend> replacement )
{
    active_backend() = replacement ? std::move( replacement ) : std::make_unique<null_backend>();
    last_message().clear();
    reset_game_state();
}

void reset_backend_after_testing()
{
    active_backend() = make_platform_backend();
    last_message().clear();
    game_messages_enabled() = false;
    reset_game_state();
}

} // namespace accessibility
