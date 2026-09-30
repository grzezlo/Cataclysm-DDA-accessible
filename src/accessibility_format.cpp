#include "accessibility_format.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
#include <string_view>
#include <tuple>
#include <utility>

#include "cata_utility.h"
#include "catacharset.h"
#include "output.h"
#include "string_formatter.h"
#include "translations.h"

namespace accessibility
{

namespace
{

bool is_space( const unsigned char ch )
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
}

/** Trim ASCII whitespace only; UTF-8 continuation bytes are never treated as spaces. */
std::string trim_ascii( const std::string_view text )
{
    size_t first = 0;
    size_t last = text.size();
    while( first < last && is_space( static_cast<unsigned char>( text[first] ) ) ) {
        ++first;
    }
    while( last > first && is_space( static_cast<unsigned char>( text[last - 1] ) ) ) {
        --last;
    }
    return std::string( text.substr( first, last - first ) );
}

/** Collapse runs of ASCII whitespace into single spaces and trim both ends. */
std::string collapse_whitespace( const std::string &text )
{
    std::string result;
    result.reserve( text.size() );
    bool pending_space = false;
    for( const char ch : text ) {
        if( is_space( static_cast<unsigned char>( ch ) ) ) {
            pending_space = !result.empty();
            continue;
        }
        if( pending_space ) {
            result.push_back( ' ' );
            pending_space = false;
        }
        result.push_back( ch );
    }
    return result;
}

bool ends_with( const std::string &text, const std::string &suffix )
{
    return text.size() >= suffix.size() &&
           text.compare( text.size() - suffix.size(), suffix.size(), suffix ) == 0;
}

/** Whether text already ends a sentence, including Unicode ellipsis and CJK full stops. */
bool ends_sentence( const std::string &text, const bool allow_clause_end )
{
    if( text.empty() ) {
        return false;
    }
    // A closing quote after the terminator, as in quoted dialogue, still ends the sentence.
    if( text.size() > 1 && text.back() == '"' ) {
        const char quoted = text[text.size() - 2];
        if( quoted == '.' || quoted == '!' || quoted == '?' ) {
            return true;
        }
    }
    const char last = text.back();
    if( last == '.' || last == '!' || last == '?' ) {
        return true;
    }
    if( allow_clause_end && ( last == ':' || last == ';' ) ) {
        return true;
    }
    return ends_with( text, "…" ) || ends_with( text, "。" ) ||
           ends_with( text, "！" ) || ends_with( text, "？" );
}

} // namespace

void message_log_review::set_entries( std::vector<message_log_entry> entries,
                                      const bool newest_first )
{
    entries_ = std::move( entries );
    newest_first_ = newest_first;
    matched_entries_.clear();
    matches_.clear();
    for( size_t index = 0; index < entries_.size(); ++index ) {
        matched_entries_.push_back( index );
        matches_.push_back( entries_[index].source_index );
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
    matched_entries_.clear();
    matches_.clear();
    for( size_t index = 0; index < entries_.size(); ++index ) {
        const message_log_entry &entry = entries_[index];
        if( ( type.empty() || entry.type == type ) && lcmatch( entry.text, text ) ) {
            matched_entries_.push_back( index );
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
    if( matched_entries_.empty() ) {
        return nullptr;
    }
    return &entries_[matched_entries_[focused_]];
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

std::string format_list_wrap( const int previous, const int current, const int direction )
{
    if( previous < 0 || current < 0 ) {
        return std::string();
    }
    if( direction > 0 && current < previous ) {
        return pgettext( "list wrap", "Top" );
    }
    if( direction < 0 && current > previous ) {
        return pgettext( "list wrap", "Bottom" );
    }
    return std::string();
}

std::string format_list_count( const size_t count )
{
    if( count == 0 ) {
        return _( "No items" );
    }
    return string_format( n_gettext( "%d item", "%d items", count ), static_cast<int>( count ) );
}

std::string format_list_position( const size_t position, const size_t count )
{
    if( position == 0 || count == 0 ) {
        return std::string();
    }
    return string_format( pgettext( "list position", "%1$d of %2$d" ), static_cast<int>( position ),
                          static_cast<int>( count ) );
}

std::string format_semantic_sequence( const std::vector<std::string> &parts )
{
    //~ Sentence terminator added between spoken accessibility phrases that lack one.
    const std::string terminator = pgettext( "accessibility sentence end", "." );
    //~ Separator between spoken accessibility sentences.
    const std::string separator = pgettext( "accessibility sentence separator", " " );
    std::string result;
    for( const std::string &part : parts ) {
        const std::string normalized = collapse_whitespace( part );
        if( normalized.empty() ) {
            continue;
        }
        if( !result.empty() ) {
            if( !ends_sentence( result, true ) ) {
                result += terminator;
            }
            result += separator;
        }
        result += normalized;
    }
    if( !result.empty() && !ends_sentence( result, false ) ) {
        result += terminator;
    }
    return result;
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

std::string format_main_menu_position( const std::string &tab, const int tab_position,
                                       const int tab_count, const int item_position, const int item_count )
{
    return format_semantic_sequence( { _( "Main menu" ),
                                       string_format( _( "Tab %1$s, %2$d of %3$d" ), tab, tab_position, tab_count ),
                                       item_position > 0 && item_count > 0 ?
                                       string_format( _( "Item %1$d of %2$d" ), item_position, item_count ) :
                                       std::string() } );
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
                                   const std::string &initial_topic, const std::string &up_binding,
                                   const std::string &down_binding, const std::string &confirm_binding,
                                   const std::string &quit_binding )
{
    if( topic_count == 0 ) {
        return string_format( _( "Help. 0 topics. %s to exit." ), format_spoken_key( quit_binding ) );
    }
    const std::string topic_str = format_help_topic_focus( initial_hotkey, initial_topic );
    return string_format(
               _( "Help. %1$d topics. Use %2$s and %3$s to browse, hotkey or %4$s to open, %5$s to exit. %6$s" ),
               static_cast<int>( topic_count ), format_spoken_key( up_binding ),
               format_spoken_key( down_binding ), format_spoken_key( confirm_binding ),
               format_spoken_key( quit_binding ), topic_str );
}

std::string format_help_menu_summary( const size_t topic_count, const size_t position )
{
    return format_semantic_sequence( { _( "Help" ),
                                       string_format( n_gettext( "%d topic", "%d topics", topic_count ),
                                               static_cast<int>( topic_count ) ),
                                       format_list_position( position, topic_count ) } );
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
                keys_str += format_spoken_key( entry.second[i] );
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
    switch( get_injury_tier( current, maximum ) ) {
        case injury_tier::uninjured:
            return _( "Health: uninjured" );
        case injury_tier::lightly_injured:
            return _( "Health: lightly injured" );
        case injury_tier::moderately_injured:
            return _( "Health: moderately injured" );
        case injury_tier::heavily_injured:
            return _( "Health: heavily injured" );
        case injury_tier::severely_injured:
            return _( "Health: severely injured" );
        case injury_tier::nearly_dead:
            return _( "Health: nearly dead" );
    }
    return {};
}

std::string format_nearby_list_entry( const nearby_list_entry &entry )
{
    std::vector<std::string> parts = { entry.name };
    if( entry.count > 0 ) {
        parts.push_back( string_format( _( "Count: %d" ), entry.count ) );
    }
    if( entry.position_count > 1 ) {
        parts.push_back( string_format( n_gettext( "%d location", "%d locations",
                                        entry.position_count ), entry.position_count ) );
    }
    parts.push_back( entry.location );
    parts.insert( parts.end(), entry.details.begin(), entry.details.end() );
    return format_semantic_sequence( parts );
}

std::string format_nearby_list_summary( const std::string &mode, const int result_count,
                                        const nearby_list_entry &entry )
{
    std::vector<std::string> parts = { format_nearby_list_context( mode, result_count ) };
    if( entry.result_count > 0 && entry.result_index > 0 ) {
        parts.push_back( format_list_position( entry.result_index, entry.result_count ) );
    }
    if( entry.position_count > 1 && entry.position_index > 0 ) {
        parts.push_back( string_format( _( "Location %1$d of %2$d" ), entry.position_index,
                                        entry.position_count ) );
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

std::string format_uilist_focus( const std::string &label, const std::string &column,
                                 const bool unavailable, const std::string &state )
{
    return format_semantic_sequence( { label, column,
                                       unavailable ? _( "Unavailable" ) : std::string(), state } );
}

std::string format_item_action_menu_context( const std::string &item_name )
{
    return string_format( _( "Actions for %s" ), item_name );
}

std::string format_item_action_availability( const item_action_availability availability )
{
    switch( availability ) {
        case item_action_availability::available:
            break;
        case item_action_availability::not_now:
            return _( "Not possible now" );
        case item_action_availability::not_applicable:
            return _( "Not applicable" );
    }
    return std::string();
}

std::string format_auto_pickup_rule_state( const bool has_rule )
{
    return has_rule ? _( "Rule set" ) : _( "No rule" );
}

std::string format_visual_hint( const std::string &hint )
{
    // Visual hints separate fields with box-drawing lines and line breaks;
    // speak each field as its own phrase instead of naming the symbols.
    static const std::string vertical_line = "\xe2\x94\x82";
    std::vector<std::string> fields;
    std::string current;
    const auto flush = [&]() {
        const std::string trimmed = trim_ascii( current );
        if( !trimmed.empty() ) {
            fields.push_back( trimmed );
        }
        current.clear();
    };
    for( size_t i = 0; i < hint.size(); ) {
        if( hint.compare( i, vertical_line.size(), vertical_line ) == 0 ) {
            flush();
            i += vertical_line.size();
        } else if( hint[i] == '\n' ) {
            flush();
            ++i;
        } else {
            current += hint[i];
            ++i;
        }
    }
    flush();
    return format_semantic_sequence( fields );
}

std::string format_uilist_open( const std::string &context, const size_t count,
                                const std::string &focus, const std::string &details_binding,
                                const bool has_details )
{
    return format_uilist_open( context, format_list_count( count ), focus, details_binding,
                               has_details );
}

std::string format_uilist_open( const std::string &context, const std::string &count,
                                const std::string &focus, const std::string &details_binding,
                                const bool has_details )
{
    return format_semantic_sequence( { context, count,
                                       _( "Use arrow keys to choose" ),
                                       has_details ? string_format( _( "%s for details" ),
                                               format_spoken_key( details_binding ) ) : std::string(), focus } );
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
                                      format_spoken_key( left_binding ), format_spoken_key( right_binding ),
                                      format_spoken_key( confirm_binding ) ) : std::string()
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

std::string format_spoken_key( const std::string &raw_key )
{
    if( raw_key.empty() ) {
        return raw_key;
    }

    if( raw_key.size() >= 3 && raw_key.front() == '[' && raw_key.back() == ']' ) {
        return "[" + format_spoken_key( raw_key.substr( 1, raw_key.size() - 2 ) ) + "]";
    }

    const size_t comma_pos = raw_key.find( ", " );
    if( comma_pos != std::string::npos ) {
        return format_spoken_key( raw_key.substr( 0, comma_pos ) ) + ", " +
               format_spoken_key( raw_key.substr( comma_pos + 2 ) );
    }

    const std::string or_sep = _( " or " );
    if( or_sep != " or " ) {
        const size_t tr_or_pos = raw_key.find( or_sep );
        if( tr_or_pos != std::string::npos ) {
            return format_spoken_key( raw_key.substr( 0, tr_or_pos ) ) + or_sep +
                   format_spoken_key( raw_key.substr( tr_or_pos + or_sep.size() ) );
        }
    }
    const size_t en_or_pos = raw_key.find( " or " );
    if( en_or_pos != std::string::npos ) {
        return format_spoken_key( raw_key.substr( 0, en_or_pos ) ) + " or " +
               format_spoken_key( raw_key.substr( en_or_pos + 4 ) );
    }

    std::string key = trim_ascii( raw_key );
    if( key.empty() ) {
        return key;
    }

    bool has_ctrl = false;
    bool has_alt = false;
    bool has_shift = false;

    auto strip_prefix = []( std::string & str, const std::string & prefix ) -> bool {
        if( prefix.empty() || str.size() < prefix.size() )
        {
            return false;
        }
        for( size_t i = 0; i < prefix.size(); ++i )
        {
            if( std::tolower( static_cast<unsigned char>( str[i] ) ) !=
                std::tolower( static_cast<unsigned char>( prefix[i] ) ) ) {
                return false;
            }
        }
        str.erase( 0, prefix.size() );
        return true;
    };

    const std::string tr_ctrl = to_translation( "key modifier", "CTRL-" ).translated();
    const std::string tr_alt = to_translation( "key modifier", "ALT-" ).translated();
    const std::string tr_shift = to_translation( "key modifier", "SHIFT-" ).translated();

    bool matched = true;
    while( matched && !key.empty() ) {
        matched = false;
        if( strip_prefix( key, "ctrl+" ) || strip_prefix( key, "ctrl-" ) ||
            strip_prefix( key, "control+" ) || strip_prefix( key, "control-" ) ||
            strip_prefix( key, tr_ctrl ) ) {
            has_ctrl = true;
            matched = true;
            continue;
        }
        if( strip_prefix( key, "alt+" ) || strip_prefix( key, "alt-" ) ||
            strip_prefix( key, tr_alt ) ) {
            has_alt = true;
            matched = true;
            continue;
        }
        if( strip_prefix( key, "shift+" ) || strip_prefix( key, "shift-" ) ||
            strip_prefix( key, tr_shift ) ) {
            has_shift = true;
            matched = true;
            continue;
        }
    }

    key = trim_ascii( key );

    if( key.size() == 1 ) {
        const unsigned char ch = static_cast<unsigned char>( key[0] );
        if( ch >= 'A' && ch <= 'Z' ) {
            has_shift = true;
            key = std::string( 1, static_cast<char>( std::tolower( ch ) ) );
        }
    }

    if( !has_ctrl && !has_alt && !has_shift ) {
        return key;
    }

    std::vector<std::string> parts;
    if( has_ctrl ) {
        parts.push_back( pgettext( "key modifier", "Ctrl" ) );
    }
    if( has_alt ) {
        parts.push_back( pgettext( "key modifier", "Alt" ) );
    }
    if( has_shift ) {
        parts.push_back( pgettext( "key modifier", "Shift" ) );
    }
    if( !key.empty() ) {
        parts.push_back( key );
    }
    return string_join( parts, "+" );
}

std::string format_keybinding_focus( const std::string &name, const std::string &primary_key,
                                     const bool local )
{
    const std::string spoken_key = format_spoken_key( primary_key );
    return format_semantic_sequence( { name,
                                       spoken_key.empty() ? _( "Unbound" ) : string_format( _( "Key: %s" ), spoken_key ),
                                       local ? _( "Local binding" ) : _( "Global binding" ) } );
}

std::string format_keybinding_details( const std::string &name,
                                       const std::vector<std::string> &keys, const bool local, const bool customized )
{
    std::string assigned_keys;
    for( const std::string &key : keys ) {
        if( !assigned_keys.empty() ) {
            assigned_keys += ", ";
        }
        assigned_keys += format_spoken_key( key );
    }
    return format_semantic_sequence( { name,
                                       keys.empty() ? _( "Unbound" ) : string_format( _( "Keys: %s" ), assigned_keys ),
                                       local ? _( "Local binding" ) : _( "Global binding" ),
                                       customized ? _( "Customized" ) : _( "Default" ) } );
}

std::string format_keybinding_open( const std::string &up_binding, const std::string &down_binding,
                                    const std::string &tab_binding, const std::string &confirm_binding,
                                    const std::string &details_binding, const size_t action_count,
                                    const std::string &focus )
{
    return format_semantic_sequence( {
        _( "Keybindings" ),
        string_format( n_gettext( "%d action", "%d actions", action_count ),
                       static_cast<int>( action_count ) ),
        string_format( _( "%1$s and %2$s choose an action. %3$s switches to the filter. %4$s opens actions. %5$s reads all keys." ),
                       format_spoken_key( up_binding ), format_spoken_key( down_binding ),
                       format_spoken_key( tab_binding ), format_spoken_key( confirm_binding ),
                       format_spoken_key( details_binding ) ),
        focus
    } );
}

std::string format_keybinding_filter( const std::string &query, const int results_count,
                                      const std::string &tab_binding, const bool include_hint )
{
    const std::string hint = include_hint
                             ? string_format( _( "Filter. Type to search. %s returns to the action list." ),
                                     format_spoken_key( tab_binding ) )
                             : std::string();
    const std::string query_text = query.empty() ? _( "empty" ) : query;
    const std::string results = string_format( _( "Text: %1$s. Results: %2$d." ), query_text,
                                results_count );
    return format_semantic_sequence( { hint, results } );
}

bool uilist_details_need_document( const std::string &details )
{
    return utf8_width( details ) > 160;
}

std::vector<std::string> split_uilist_detail_sentences( const std::string &details )
{
    const std::string normalized = collapse_whitespace( details );

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

std::vector<std::string> format_item_info_paragraphs( const std::string &type_name,
        const std::string &raw_info )
{
    std::vector<std::string> paragraphs;

    // Item info uses color tags and a few semantic tags such as <good>; a tag
    // never starts with whitespace, so comparisons like "< 5" are preserved.
    static const std::regex markup_tags( R"(<[^<>\s][^<>]*>)" );
    const std::string clean_type = trim_ascii( std::regex_replace( type_name, markup_tags, "" ) );
    if( !clean_type.empty() ) {
        paragraphs.push_back( clean_type );
    }

    for( std::string &paragraph : format_document_paragraphs( raw_info ) ) {
        paragraphs.push_back( std::move( paragraph ) );
    }
    return paragraphs;
}

std::vector<std::string> format_document_paragraphs( const std::string &text )
{
    static const std::regex markup_tags( R"(<[^<>\s][^<>]*>)" );
    const std::string clean_info = std::regex_replace( text, markup_tags, "" );
    std::vector<std::string> paragraphs;

    size_t start = 0;
    while( start <= clean_info.size() ) {
        size_t end = clean_info.find( '\n', start );
        if( end == std::string::npos ) {
            end = clean_info.size();
        }
        const std::string trimmed = trim_ascii( std::string_view( clean_info ).substr( start,
                                                end - start ) );
        start = end + 1;
        if( trimmed.empty() ) {
            continue;
        }
        if( trimmed.find_first_of( ".!?" ) != std::string::npos ) {
            for( std::string &sentence : split_uilist_detail_sentences( trimmed ) ) {
                if( !sentence.empty() ) {
                    paragraphs.push_back( std::move( sentence ) );
                }
            }
        } else {
            paragraphs.push_back( trimmed );
        }
    }

    return paragraphs;
}

std::string format_inventory_focus( const inventory_focus_entry &entry,
                                    const bool include_context, const bool include_all_cells )
{
    std::vector<std::string> parts;
    if( include_context && !entry.category.empty() ) {
        parts.push_back( entry.category );
    }
    parts.push_back( entry.caption );
    if( !entry.selection.empty() ) {
        parts.push_back( entry.selection );
    }
    if( entry.not_owned ) {
        parts.push_back( _( "Not owned" ) );
    }
    if( !entry.location.empty() ) {
        parts.push_back( string_format( _( "Location: %s" ), entry.location ) );
    }
    if( entry.favorite ) {
        parts.push_back( _( "Favorite" ) );
    }
    if( entry.has_contents ) {
        parts.push_back( entry.contents_collapsed ? _( "Contents collapsed" ) :
                         _( "Contents expanded" ) );
    }
    const size_t cell_count = include_all_cells ? entry.cells.size() :
                              std::min<size_t>( entry.cells.size(), 2 );
    for( size_t i = 0; i < cell_count; ++i ) {
        const auto &cell = entry.cells[i];
        if( !cell.second.empty() ) {
            parts.push_back( cell.first.empty() ? cell.second :
                             string_format( _( "%s: %s" ), cell.first, cell.second ) );
        }
    }
    if( !entry.denial.empty() ) {
        parts.push_back( string_format( _( "Unavailable: %s" ), entry.denial ) );
    }
    if( include_all_cells && !entry.owner.empty() ) {
        parts.push_back( string_format( _( "Owner: %s" ), entry.owner ) );
    }
    if( include_all_cells && !entry.destination.empty() ) {
        parts.push_back( string_format( _( "Goes into: %s" ), entry.destination ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_inventory_filter( const std::string &query, const size_t result_count )
{
    return format_semantic_sequence( {
        query.empty() ? _( "Filter cleared" ) : string_format( _( "Filter: %s" ), query ),
        string_format( n_gettext( "%d matching item", "%d matching items", result_count ),
                       static_cast<int>( result_count ) )
    } );
}

std::string format_inventory_selection_state( const size_t chosen, const size_t available,
        const bool selectable )
{
    if( chosen == 0 ) {
        return selectable ? _( "Not selected" ) : std::string();
    }
    if( available <= 1 ) {
        return _( "Selected" );
    }
    if( chosen >= available ) {
        return string_format( _( "Selected all %d" ), static_cast<int>( available ) );
    }
    return string_format( _( "Selected %1$d of %2$d" ), static_cast<int>( chosen ),
                          static_cast<int>( available ) );
}

std::string format_inventory_selection_total( const size_t selected_entries )
{
    if( selected_entries == 0 ) {
        return _( "Nothing selected" );
    }
    return string_format( n_gettext( "%d entry selected", "%d entries selected",
                                     selected_entries ), static_cast<int>( selected_entries ) );
}

std::string format_inventory_selection_change( const std::string &caption,
        const std::string &state, const size_t selected_entries )
{
    return format_semantic_sequence( {
        caption, state, format_inventory_selection_total( selected_entries )
    } );
}

std::string format_inventory_capacity( const inventory_capacity &capacity,
                                       const bool include_values )
{
    std::vector<std::string> parts;
    if( include_values ) {
        //~ Spoken selector statistics: %1$s weight, %2$s weight limit, %3$s weight unit, %4$s volume, %5$s volume limit, %6$s volume unit
        parts.push_back( string_format( _( "Weight %1$s of %2$s %3$s, volume %4$s of %5$s %6$s" ),
                                        capacity.weight, capacity.weight_limit, capacity.weight_unit,
                                        capacity.volume, capacity.volume_limit, capacity.volume_unit ) );
    }
    if( capacity.over_weight ) {
        parts.push_back( _( "Over weight limit" ) );
    }
    if( capacity.over_volume ) {
        parts.push_back( _( "Over volume limit" ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_trade_marked( const size_t marked, const size_t available )
{
    if( marked == 0 ) {
        return {};
    }
    if( available <= 1 ) {
        return _( "Marked" );
    }
    //~ Number of units of a trade row marked for the trade
    return string_format( n_gettext( "%d marked", "%d marked", marked ), static_cast<int>( marked ) );
}

std::string format_trade_row( const trade_row &row, const bool include_context,
                              const bool include_details )
{
    const inventory_focus_entry &item = row.item;
    std::vector<std::string> parts;
    if( include_context && !item.category.empty() ) {
        parts.push_back( item.category );
    }
    parts.push_back( item.caption );
    if( !row.unit_price.empty() ) {
        //~ Price of one unit of a trade row
        parts.push_back( string_format( _( "%s each" ), row.unit_price ) );
    }
    parts.push_back( format_trade_marked( row.marked, row.available ) );
    if( item.not_owned ) {
        parts.push_back( _( "Not owned" ) );
    }
    if( !item.denial.empty() ) {
        parts.push_back( string_format( _( "Unavailable: %s" ), item.denial ) );
    }
    if( include_details ) {
        if( !item.location.empty() ) {
            parts.push_back( string_format( _( "Location: %s" ), item.location ) );
        }
        if( item.favorite ) {
            parts.push_back( _( "Favorite" ) );
        }
        if( item.has_contents ) {
            parts.push_back( item.contents_collapsed ? _( "Contents collapsed" ) :
                             _( "Contents expanded" ) );
        }
        if( !item.owner.empty() ) {
            parts.push_back( string_format( _( "Owner: %s" ), item.owner ) );
        }
    }
    return format_semantic_sequence( parts );
}

std::string format_trade_mark_change( const std::string &caption, const size_t marked,
                                      const size_t available )
{
    return format_semantic_sequence( {
        caption, marked == 0 ? _( "Unmarked" ) : format_trade_marked( marked, available )
    } );
}

std::string format_trade_balance( const trade_balance &balance )
{
    std::string amount;
    if( balance.exchange ) {
        amount = _( "Free exchange" );
    } else if( balance.debt ) {
        amount = string_format( _( "Balance: you owe %s" ), balance.amount );
    } else {
        amount = string_format( _( "Balance: credit %s" ), balance.amount );
    }
    //~ Trade balance and whether the trader accepts it, e.g. "Balance: credit $3.00, acceptable"
    return string_format( _( "%1$s, %2$s" ), amount,
                          balance.acceptable ? _( "acceptable" ) : _( "trader refuses" ) );
}

std::string format_trade_pane_label( const bool yours )
{
    return yours ? _( "Your goods" ) : _( "Trader's goods" );
}

std::string format_trade_opening( const std::string &title, const std::string &trader,
                                  const trade_balance &balance )
{
    std::string purpose = collapse_whitespace( title );
    while( !purpose.empty() && ( purpose.back() == ':' || purpose.back() == ' ' ) ) {
        purpose.pop_back();
    }
    //~ Trade window purpose and the trader's name, e.g. "Trade: Rubik"
    const std::string heading = purpose.empty() ? trader :
                                string_format( _( "%1$s: %2$s" ), purpose, trader );
    return format_semantic_sequence( { heading, format_trade_balance( balance ) } );
}

std::string format_trade_summary( const trade_summary &summary )
{
    std::vector<std::string> parts = {
        format_trade_opening( summary.title, summary.trader, summary.balance )
    };
    if( !summary.buying.empty() ) {
        //~ Value of the trader's goods marked for the trade
        parts.push_back( string_format( _( "Buying: %s" ), summary.buying ) );
    }
    if( !summary.selling.empty() ) {
        //~ Value of the player's goods marked for the trade
        parts.push_back( string_format( _( "Selling: %s" ), summary.selling ) );
    }
    if( !summary.bank.empty() ) {
        parts.push_back( string_format( _( "Bank balance: %s" ), summary.bank ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_trade_bank_change( const trade_balance &balance, const std::string &bank )
{
    return format_semantic_sequence( {
        format_trade_balance( balance ), string_format( _( "Bank balance: %s" ), bank )
    } );
}

std::string format_trade_result( const bool traded )
{
    return traded ? _( "Trade complete" ) : _( "Trade cancelled" );
}

std::string format_mission_category( const mission_category category )
{
    switch( category ) {
        case mission_category::active:
            return pgettext( "missions tab", "Active" );
        case mission_category::completed:
            return pgettext( "missions tab", "Completed" );
        case mission_category::failed:
            return pgettext( "missions tab", "Failed" );
    }
    return std::string();
}

std::string format_mission_count( const size_t count )
{
    if( count == 0 ) {
        return _( "no missions" );
    }
    return string_format( n_gettext( "%d mission", "%d missions", count ),
                          static_cast<int>( count ) );
}

std::string format_mission_row( const mission_row &row )
{
    std::vector<std::string> parts = { collapse_whitespace( row.title ) };
    if( row.tracked ) {
        parts.emplace_back( _( "current objective" ) );
    }
    switch( row.category ) {
        case mission_category::active:
            if( row.deadline_passed ) {
                parts.emplace_back( _( "deadline passed" ) );
            } else if( !row.time.empty() ) {
                //~ Time left until an active mission fails, such as "deadline in 2 days".
                parts.push_back( string_format( _( "deadline in %s" ), row.time ) );
            }
            break;
        case mission_category::completed:
            if( !row.time.empty() ) {
                //~ Time since a mission was completed, such as "completed 3 hours ago".
                parts.push_back( string_format( _( "completed %s ago" ), row.time ) );
            }
            break;
        case mission_category::failed:
            if( !row.time.empty() ) {
                //~ Time since a mission failed, such as "failed 3 hours ago".
                parts.push_back( string_format( _( "failed %s ago" ), row.time ) );
            }
            break;
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_mission_tab( const mission_category category, const size_t count,
                                const std::string &focus )
{
    std::vector<std::string> parts = {
        string_join( std::vector<std::string> { format_mission_category( category ), format_mission_count( count ) },
        _( ", " ) )
    };
    if( count > 0 ) {
        parts.push_back( focus );
    }
    return format_semantic_sequence( parts );
}

std::string format_mission_screen_opening( const std::string &tab )
{
    return format_semantic_sequence( { _( "Your missions" ), tab } );
}

std::string format_mission_location( const std::string &label, const int omt_distance,
                                     const std::string &direction, const std::string &length )
{
    if( omt_distance <= 0 ) {
        //~ A mission location that is on the player's map tile, such as "Target: here".
        return format_semantic_sequence( { string_format( _( "%s: here" ), label ) } );
    }
    //~ Distance to a mission location: %1$s is its kind, %2$d the number of map tiles,
    //~ %3$s a compass direction, %4$s the approximate real distance.
    return format_semantic_sequence( { string_format( n_gettext( "%1$s: %2$d map tile %3$s, %4$s",
                                       "%1$s: %2$d map tiles %3$s, %4$s", omt_distance ), label, omt_distance,
                                       direction, length )
                                     } );
}

std::string format_mission_no_location()
{
    return _( "No known location." );
}

std::vector<std::string> format_mission_details( const mission_details &details )
{
    std::vector<std::string> paragraphs;
    paragraphs.push_back( format_semantic_sequence( {
        format_mission_row( details.row ),
        format_mission_category( details.row.category ),
        format_list_position( details.position, details.count )
    } ) );
    if( !details.giver.empty() ) {
        std::vector<std::string> giver = { string_format( _( "Given by: %s" ), details.giver ) };
        if( !details.faction.empty() ) {
            giver.push_back( string_format( _( "Faction: %s" ), details.faction ) );
        }
        paragraphs.push_back( format_semantic_sequence( giver ) );
        if( !details.giver_location.empty() ) {
            paragraphs.push_back( details.giver_location );
        }
    }
    bool first_line = true;
    for( const std::string &line : string_split( remove_color_tags( details.description ), '\n' ) ) {
        for( std::string &sentence : split_uilist_detail_sentences( line ) ) {
            if( sentence.empty() ) {
                continue;
            }
            if( first_line ) {
                //~ Start of a mission description, which states its objective and reward.
                sentence = string_format( _( "Objective: %s" ), sentence );
                first_line = false;
            }
            paragraphs.push_back( std::move( sentence ) );
        }
    }
    for( const std::string &line : details.time_lines ) {
        paragraphs.push_back( format_semantic_sequence( { remove_color_tags( line ) } ) );
    }
    paragraphs.push_back( details.location.empty() ? format_mission_no_location() :
                          details.location );
    return paragraphs;
}

std::string format_mission_summary( const mission_category category, const size_t position,
                                    const size_t count, const std::string &current_objective )
{
    return format_semantic_sequence( {
        string_join( std::vector<std::string> { _( "Your missions" ), format_mission_category( category ), format_mission_count( count ) },
        _( ", " ) ),
        format_list_position( position, count ),
        current_objective.empty() ? std::string( _( "No current objective" ) ) :
        string_format( _( "Current objective: %s" ), current_objective )
    } );
}

std::string format_mission_tracked( const std::string &title )
{
    return format_semantic_sequence( { string_format( _( "Current objective: %s" ), title ) } );
}

std::string format_mission_track_unavailable( const mission_category category,
        const size_t count )
{
    if( count == 0 ) {
        return format_semantic_sequence( { _( "Unavailable: no mission selected" ) } );
    }
    if( category != mission_category::active ) {
        return format_semantic_sequence( { _( "Unavailable: only an active mission can be the current objective" ) } );
    }
    return std::string();
}

std::string format_character_choice_row( const character_choice_row &row )
{
    std::vector<std::string> parts = { collapse_whitespace( row.name ) };
    if( row.show_points ) {
        if( row.points < 0 ) {
            //~ A character creation choice that adds points, such as "earns 2 points".
            parts.push_back( string_format( n_gettext( "earns %d point", "earns %d points", -row.points ),
                                            -row.points ) );
        } else {
            //~ The point cost of a character creation choice, such as "2 points".
            parts.push_back( string_format( n_gettext( "%d point", "%d points", row.points ),
                                            row.points ) );
        }
    }
    if( row.selected ) {
        parts.emplace_back( _( "selected" ) );
    }
    return format_semantic_sequence( {
        string_join( parts, _( ", " ) ),
        row.unavailable.empty() ? std::string() : string_format( _( "Unavailable: %s" ), row.unavailable ),
        row.short_of_points ? std::string( _( "Not enough points" ) ) : std::string()
    } );
}

std::string format_character_point_balance( const character_point_balance &balance )
{
    switch( balance.pool ) {
        case character_point_pool::freeform:
        case character_point_pool::transfer:
            return std::string();
        case character_point_pool::one_pool:
            if( balance.total < 0 ) {
                return string_format( n_gettext( "%d point over budget", "%d points over budget",
                                                 -balance.total ), -balance.total );
            }
            return string_format( n_gettext( "%d point left", "%d points left", balance.total ),
                                  balance.total );
        case character_point_pool::multi_pool:
            break;
    }
    //~ Points left in the legacy multiple pools: %1$d total, then stat, trait, and skill points.
    std::string result = string_format( _( "%1$d points left: stats %2$d, traits %3$d, skills %4$d" ),
                                        balance.total, balance.stat_points, balance.trait_points, balance.skill_points );
    if( !balance.valid ) {
        result = format_semantic_sequence( { result, _( "Over budget" ) } );
    }
    return result;
}

std::string format_character_creation_tab( const std::string &tab, const size_t count,
        const std::string &focus, const bool opening )
{
    return format_semantic_sequence( {
        opening ? std::string( _( "Character creation" ) ) : std::string(),
        string_join( std::vector<std::string> { tab, format_list_count( count ) }, _( ", " ) ),
        count > 0 ? focus : std::string()
    } );
}

std::string format_character_choice_selected( const std::string &name, const bool added,
        const std::string &balance )
{
    return format_semantic_sequence( {
        string_format( added ? _( "Selected %s" ) : _( "Removed %s" ), collapse_whitespace( name ) ),
        balance
    } );
}

std::string format_character_choice_filter( const std::string &filter, const size_t count,
        const std::string &focus )
{
    const std::string label = filter.empty() ? std::string( _( "Filter cleared" ) ) :
                              //~ Character creation list filter, such as "Filter doc".
                              string_format( _( "Filter %s" ), filter );
    return format_semantic_sequence( {
        string_join( std::vector<std::string> { label, format_list_count( count ) }, _( ", " ) ),
        count > 0 ? focus : std::string()
    } );
}

std::string format_character_choice_sort( const bool by_name, const std::string &focus )
{
    return format_semantic_sequence( {
        by_name ? _( "Sorted by name" ) : _( "Default sort order" ), focus
    } );
}

std::string format_character_tab_summary( const std::string &tab, const size_t position,
        const size_t count, const std::vector<std::string> &current, const std::string &balance,
        const std::string &difficulty )
{
    return format_semantic_sequence( {
        string_join( std::vector<std::string> { tab, format_list_count( count ) }, _( ", " ) ),
        format_list_position( position, count ),
        current.empty() ? std::string( _( "Current: none" ) ) :
        string_format( _( "Current: %s" ), string_join( current, _( ", " ) ) ),
        balance,
        difficulty
    } );
}

std::string format_overmap_offset( const int dx, const int dy, const int dz )
{
    std::vector<std::string> horizontal;
    if( dy != 0 ) {
        horizontal.push_back( string_format( pgettext( "map offset", "%1$d %2$s" ), std::abs( dy ),
                                             dy < 0 ? pgettext( "map offset", "north" ) : pgettext( "map offset", "south" ) ) );
    }
    if( dx != 0 ) {
        horizontal.push_back( string_format( pgettext( "map offset", "%1$d %2$s" ), std::abs( dx ),
                                             dx < 0 ? pgettext( "map offset", "west" ) : pgettext( "map offset", "east" ) ) );
    }
    std::vector<std::string> parts;
    if( !horizontal.empty() ) {
        //~ Separator between the north-south and east-west parts of "3 north 2 east".
        parts.push_back( string_join( horizontal, pgettext( "map offset", " " ) ) );
    }
    if( dz != 0 ) {
        const int levels = std::abs( dz );
        parts.push_back( dz > 0 ?
                         string_format( n_gettext( "%d level above", "%d levels above", levels ), levels ) :
                         string_format( n_gettext( "%d level below", "%d levels below", levels ), levels ) );
    }
    if( parts.empty() ) {
        return _( "here" );
    }
    return string_join( parts, _( ", " ) );
}

std::string format_overmap_level( const int z )
{
    //~ Overmap z-level, such as "Level -1"; 0 is the ground.
    return string_format( _( "Level %d" ), z );
}

namespace
{

std::string overmap_danger_marker( const int danger_radius, const bool in_dangerous_area )
{
    if( danger_radius >= 0 ) {
        //~ A map note marked dangerous with its radius in map tiles.
        return string_format( _( "dangerous area, radius %d" ), danger_radius );
    }
    if( in_dangerous_area ) {
        return _( "in dangerous area" );
    }
    return std::string();
}

void add_part( std::vector<std::string> &parts, const std::string &part )
{
    if( !part.empty() ) {
        parts.push_back( part );
    }
}

} // namespace

std::string format_overmap_tile( const overmap_tile &tile, const bool terrain_repeated )
{
    std::vector<std::string> parts = {
        tile.terrain.empty() ? std::string( _( "unexplored" ) ) : collapse_whitespace( tile.terrain )
    };
    if( !tile.note.empty() ) {
        //~ A map note at the overmap cursor.
        parts.push_back( string_format( _( "note: %s" ), collapse_whitespace( tile.note ) ) );
    }
    add_part( parts, overmap_danger_marker( tile.danger_radius, tile.in_dangerous_area ) );
    for( const std::string &mission : tile.missions ) {
        //~ An active mission whose target is at the overmap cursor.
        parts.push_back( string_format( _( "mission target: %s" ), collapse_whitespace( mission ) ) );
    }
    if( tile.route_destination ) {
        parts.emplace_back( _( "route destination" ) );
    } else if( tile.on_route ) {
        parts.emplace_back( _( "on route" ) );
    }
    if( !terrain_repeated ) {
        parts.push_back( format_overmap_offset( tile.dx, tile.dy, tile.dz ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_overmap_opening( const std::string &prompt, const std::string &tile )
{
    return format_semantic_sequence( {
        prompt.empty() ? std::string( _( "Overmap" ) ) : remove_color_tags( prompt ), tile
    } );
}

std::string format_overmap_level_change( const int z, const std::string &tile )
{
    return format_semantic_sequence( { format_overmap_level( z ), tile } );
}

std::string format_known_place_row( const known_place &place )
{
    std::vector<std::string> parts = { collapse_whitespace( place.name ) };
    std::string location;
    if( place.distance <= 0 ) {
        location = _( "here" );
    } else {
        location = string_format( n_gettext( "%1$d map tile %2$s", "%1$d map tiles %2$s",
                                             place.distance ), place.distance, place.direction );
    }
    if( place.dz != 0 ) {
        location = string_join( std::vector<std::string> { location, format_overmap_offset( 0, 0, place.dz ) },
                                _( ", " ) );
    }
    parts.push_back( location );
    if( place.city ) {
        parts.emplace_back( _( "city" ) );
    }
    if( place.camp ) {
        parts.emplace_back( _( "camp" ) );
    }
    if( !place.note.empty() ) {
        parts.push_back( string_format( _( "note: %s" ), collapse_whitespace( place.note ) ) );
    }
    add_part( parts, overmap_danger_marker( place.danger_radius, place.in_dangerous_area ) );
    for( const std::string &mission : place.missions ) {
        parts.push_back( string_format( _( "mission target: %s" ), collapse_whitespace( mission ) ) );
    }
    if( place.current_objective ) {
        parts.emplace_back( _( "current objective" ) );
    }
    if( place.route_destination ) {
        parts.emplace_back( _( "route destination" ) );
    }
    return string_join( parts, _( ", " ) );
}

std::string format_known_place_count( const size_t count )
{
    if( count == 0 ) {
        return _( "no places" );
    }
    return string_format( n_gettext( "%d place", "%d places", count ), static_cast<int>( count ) );
}

std::string format_overmap_route_preview( const std::string &destination,
        const std::string &location, const int length, const std::string &confirm_key )
{
    return format_semantic_sequence( {
        //~ %1$s: route destination; %2$s: its distance and direction such as "8 map tiles north"
        string_format( _( "Route to %1$s, %2$s" ), destination, location ),
        string_format( n_gettext( "%d map tile of travel", "%d map tiles of travel", length ), length ),
        //~ %s: key that starts travel along the previewed route
        string_format( _( "Press %s again to travel" ), confirm_key )
    } );
}

std::string format_overmap_no_route( const std::string &destination, const bool different_level )
{
    return format_semantic_sequence( {
        string_format( _( "No route to %s" ), destination ),
        different_level ? std::string( _( "Travel needs the destination on your level" ) ) : std::string()
    } );
}

std::string format_overmap_no_route_selected()
{
    return format_semantic_sequence( { _( "No route selected" ) } );
}

std::string format_overmap_route_state( const std::string &destination,
                                        const std::string &location )
{
    if( destination.empty() ) {
        return format_overmap_no_route_selected();
    }
    return format_semantic_sequence( {
        //~ %1$s: route destination; %2$s: its distance and direction from the player
        string_format( _( "Route: %1$s, %2$s" ), destination, location )
    } );
}

std::string format_overmap_summary( const std::string &prompt, const std::string &tile,
                                    const int z, const std::string &route )
{
    return format_semantic_sequence( {
        prompt.empty() ? std::string( _( "Overmap" ) ) : remove_color_tags( prompt ),
        string_format( _( "Cursor: %s" ), tile ), format_overmap_level( z ), route
    } );
}

std::string format_overmap_search_results( const size_t count, const std::string &focus )
{
    return format_semantic_sequence( {
        string_format( n_gettext( "Search results, %d result", "Search results, %d results", count ),
                       static_cast<int>( count ) ),
        focus
    } );
}

std::string format_overmap_search_position( const size_t position, const size_t count,
        const std::string &focus )
{
    return format_semantic_sequence( {
        string_format( _( "Result %s" ), format_list_position( position, count ) ), focus
    } );
}

std::string format_overmap_explored( const bool explored )
{
    return format_semantic_sequence( { explored ? _( "Marked explored" ) : _( "Marked unexplored" ) } );
}

namespace
{

std::string zone_row_parts( const zone_row &row )
{
    const std::string name = collapse_whitespace( row.name );
    std::vector<std::string> parts = { name };
    const std::string type = collapse_whitespace( row.type );
    if( !type.empty() && type != name ) {
        parts.push_back( type );
    }
    if( row.personal ) {
        parts.emplace_back( _( "personal" ) );
    }
    if( row.vehicle ) {
        parts.emplace_back( _( "vehicle" ) );
    }
    parts.emplace_back( row.enabled ? _( "enabled" ) : _( "disabled" ) );
    if( row.distance > 0 ) {
        //~ Distance and compass direction from the player to a zone, such as "5 tiles north".
        parts.push_back( string_format( n_gettext( "%1$d tile %2$s", "%1$d tiles %2$s", row.distance ),
                                        row.distance, row.direction ) );
    } else if( row.dz == 0 ) {
        parts.emplace_back( _( "here" ) );
    }
    if( row.dz != 0 ) {
        const int levels = std::abs( row.dz );
        parts.push_back( row.dz > 0 ?
                         string_format( n_gettext( "%d level above", "%d levels above", levels ), levels ) :
                         string_format( n_gettext( "%d level below", "%d levels below", levels ), levels ) );
    }
    return string_join( parts, _( ", " ) );
}

/** Trim spaces and trailing separators left by the options pane layout. */
std::string trim_zone_option_text( const std::string &text )
{
    std::string result = collapse_whitespace( text );
    while( !result.empty() && ( result.back() == ' ' || result.back() == ',' ) ) {
        result.pop_back();
    }
    size_t start = 0;
    while( start < result.size() && result[start] == ' ' ) {
        ++start;
    }
    return result.substr( start );
}

} // namespace

std::string format_zone_row( const zone_row &row )
{
    return format_semantic_sequence( { zone_row_parts( row ) } );
}

std::string format_zone_count( const size_t count )
{
    if( count == 0 ) {
        return _( "no zones" );
    }
    return string_format( n_gettext( "%d zone", "%d zones", count ), static_cast<int>( count ) );
}

std::string format_zones_manager_opening( const size_t count, const std::string &focus )
{
    return format_semantic_sequence( {
        string_join( std::vector<std::string> { _( "Zones manager" ), format_zone_count( count ) }, _( ", " ) ),
        count > 0 ? focus : std::string()
    } );
}

std::string format_zone_size( const int width, const int height, const int levels )
{
    //~ Size of a zone in tiles, such as "Zone 4 by 3".
    std::string size = string_format( _( "Zone %1$d by %2$d" ), width, height );
    if( levels > 1 ) {
        size = string_join( std::vector<std::string> { size,
                            string_format( n_gettext( "%d level", "%d levels", levels ), levels )
                                                     }, _( ", " ) );
    }
    return format_semantic_sequence( { size } );
}

std::string format_zone_enabled( const std::string &name, const bool enabled )
{
    //~ A zone was enabled or disabled: %s is the zone name.
    return format_semantic_sequence( { string_format( enabled ? _( "%s enabled" ) :
                                       _( "%s disabled" ), collapse_whitespace( name ) )
                                     } );
}

std::string format_zone_displayed( const std::string &name, const bool displayed )
{
    //~ A zone's map display was toggled: %s is the zone name.
    return format_semantic_sequence( { string_format( displayed ? _( "%s shown on map" ) :
                                       _( "%s hidden from map" ), collapse_whitespace( name ) )
                                     } );
}

std::string format_zone_moved( const std::string &name, const size_t position, const size_t count )
{
    return format_semantic_sequence( {
        string_join( std::vector<std::string> { collapse_whitespace( name ), format_list_position( position, count ) },
        _( ", " ) )
    } );
}

std::string format_zone_removed( const std::string &name, const std::string &focus )
{
    //~ A zone was removed: %s is the zone name.
    return format_semantic_sequence( {
        string_format( _( "Removed %s" ), collapse_whitespace( name ) ),
        focus.empty() ? std::string( _( "No zones" ) ) : focus
    } );
}

std::string format_personal_zones_changed( const bool enabled, const size_t changed )
{
    if( changed == 0 ) {
        return format_semantic_sequence( { _( "No personal zones changed" ) } );
    }
    const int count = static_cast<int>( changed );
    return format_semantic_sequence( { enabled ?
                                       string_format( n_gettext( "%d personal zone enabled", "%d personal zones enabled", count ), count ) :
                                       string_format( n_gettext( "%d personal zone disabled", "%d personal zones disabled", count ), count )
                                     } );
}

std::string format_zone_filter( const bool show_all )
{
    return show_all ? _( "Showing all zones" ) : _( "Hiding distant zones" );
}

std::string format_zone_list_change( const std::string &filter, const size_t count,
                                     const std::string &focus )
{
    return format_semantic_sequence( {
        string_join( std::vector<std::string> { filter, format_zone_count( count ) }, _( ", " ) ),
        count > 0 ? focus : std::string()
    } );
}

std::string format_zone_options( const std::vector<std::pair<std::string, std::string>> &options )
{
    std::vector<std::string> values;
    for( const std::pair<std::string, std::string> &option : options ) {
        std::string label = trim_zone_option_text( option.first );
        const std::string value = trim_zone_option_text( option.second );
        if( !label.empty() && label.back() == ':' ) {
            label.pop_back();
        }
        if( label.empty() ) {
            values.push_back( value );
        } else {
            //~ A zone option and its value, such as "Filter: food".
            values.push_back( string_format( _( "%1$s: %2$s" ), label, value ) );
        }
    }
    if( values.empty() ) {
        return std::string();
    }
    //~ Options of a zone, such as "Options: Filter: food".
    return format_semantic_sequence( { string_format( _( "Options: %s" ),
                                       string_join( values, _( ", " ) ) )
                                     } );
}

std::string format_zone_details( const zone_details &details )
{
    return format_semantic_sequence( {
        zone_row_parts( details.row ),
        format_list_position( details.position, details.count ),
        format_zone_size( details.width, details.height, details.levels ),
        details.displayed ? _( "Shown on map" ) : _( "Not shown on map" ),
        format_zone_options( details.options )
    } );
}

std::string format_zones_manager_summary( const std::string &filter, const std::string &faction,
        const size_t position, const size_t count, const std::string &focus )
{
    return format_semantic_sequence( {
        _( "Zones manager" ), filter,
        //~ Faction whose zones the zones manager shows.
        faction.empty() ? std::string() : string_format( _( "Faction: %s" ), faction ),
        count == 0 ? std::string( _( "No zones" ) ) : format_list_position( position, count ),
        count > 0 ? focus : std::string()
    } );
}

std::string format_vehicle_speed( const int speed, const std::string &units )
{
    if( speed < 0 ) {
        //~ Vehicle speed while reversing: %1$d is the speed, %2$s the unit such as "mph".
        return string_format( _( "%1$d %2$s reverse" ), -speed, units );
    }
    //~ Vehicle speed: %1$d is the speed, %2$s the unit such as "mph".
    return string_format( _( "%1$d %2$s" ), speed, units );
}

std::string format_vehicle_heading( const std::string &compass, const int degrees )
{
    //~ Vehicle heading: %1$s is a compass direction, %2$d degrees from north.
    return format_semantic_sequence( { string_format( _( "Heading %1$s, %2$d degrees" ), compass,
                                       degrees )
                                     } );
}

std::string format_vehicle_cruise( const int cruise, const std::string &units,
                                   const bool above_safe, const bool at_limit )
{
    //~ Vehicle cruise control target speed, such as "20 mph".
    std::string result = string_format( _( "Cruise %s" ), format_vehicle_speed( cruise, units ) );
    if( above_safe ) {
        result += _( ", above safe speed" );
    }
    if( at_limit ) {
        result += _( ", maximum" );
    }
    return format_semantic_sequence( { result } );
}

std::string format_vehicle_strain( const vehicle_strain strain )
{
    switch( strain ) {
        case vehicle_strain::none:
            break;
        case vehicle_strain::light:
            return _( "light engine strain" );
        case vehicle_strain::heavy:
            return _( "heavy engine strain" );
        case vehicle_strain::severe:
            return _( "severe engine strain" );
    }
    return std::string();
}

std::string format_vehicle_fuel( const vehicle_fuel_gauge &gauge )
{
    //~ Vehicle fuel gauge: %1$s is the fuel, %2$d the percentage of the tank.
    std::string result = string_format( _( "%1$s %2$d%%" ), gauge.fuel, gauge.percent );
    if( !gauge.estimate.empty() ) {
        result += gauge.filling ?
                  //~ Time until a vehicle tank or battery is full.
                  string_format( _( ", %s until full" ), gauge.estimate ) :
                  //~ Time until a vehicle tank or battery is empty.
                  string_format( _( ", %s until empty" ), gauge.estimate );
    }
    return format_semantic_sequence( { result } );
}

std::string format_vehicle_status( const vehicle_status &status )
{
    //~ Actual and cruise speed of the driven vehicle.
    std::string speed = string_format( _( "Speed %1$s, cruise %2$s" ),
                                       format_vehicle_speed( status.speed, status.units ),
                                       format_vehicle_speed( status.cruise, status.units ) );
    const std::string strain = format_vehicle_strain( status.strain );
    if( !strain.empty() ) {
        speed += ", " + strain;
    }
    //~ Vehicle heading: %1$s is a compass direction, %2$d degrees from north.
    std::string heading = string_format( _( "Heading %1$s, %2$d degrees" ), status.compass,
                                         status.heading );
    if( status.turning ) {
        //~ Steering target of a turning vehicle: %1$s is a compass direction, %2$d degrees.
        heading += string_format( _( ", turning to %1$s, %2$d degrees" ), status.steering_compass,
                                  status.steering );
    }
    std::vector<std::string> parts = {
        status.name, speed, heading,
        status.engine_on ? _( "Engine on" ) : _( "Engine off" )
    };
    for( const vehicle_fuel_gauge &gauge : status.fuels ) {
        parts.push_back( format_vehicle_fuel( gauge ) );
    }
    if( status.skidding ) {
        parts.emplace_back( _( "Skidding" ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_vehicle_obstacle( const std::string &name, const int distance,
                                     const bool reversing )
{
    return format_semantic_sequence( { reversing ?
                                       //~ Obstacle in the path of a reversing vehicle: %1$s is its name, %2$d the distance.
                                       string_format( n_gettext( "%1$s behind, %2$d tile", "%1$s behind, %2$d tiles", distance ),
                                               name, distance ) :
                                       //~ Obstacle in the path of a vehicle: %1$s is its name, %2$d the distance.
                                       string_format( n_gettext( "%1$s ahead, %2$d tile", "%1$s ahead, %2$d tiles", distance ),
                                               name, distance )
                                     } );
}

std::string format_vehicle_engine_row( const std::string &engine, const std::string &fuel,
                                       const bool active )
{
    //~ Engine selection row: %1$s is the engine, %2$s its fuel, %3$s "on" or "off".
    return string_format( _( "%1$s, %2$s, %3$s" ), engine, fuel,
                          active ? pgettext( "engine", "on" ) : pgettext( "engine", "off" ) );
}

namespace
{
/**
 * Third of a vehicle dimension: 0 for the rear or left, 1 for the middle, 2
 * for the front or right, or -1 when the vehicle is one tile long there.
 */
int vehicle_section( const int value, const int min, const int max )
{
    const int length = max - min + 1;
    if( length <= 1 ) {
        return -1;
    }
    const int offset = std::clamp( value, min, max ) - min;
    if( length == 2 ) {
        return offset == 0 ? 0 : 2;
    }
    // Symmetric thirds, so both ends of a vehicle get the same number of rows.
    return ( offset * 3 + 1 ) / length;
}

std::string without_empty_joined( const std::vector<std::string> &parts,
                                  const std::string &separator )
{
    std::vector<std::string> present;
    for( const std::string &part : parts ) {
        if( !part.empty() ) {
            present.push_back( part );
        }
    }
    return string_join( present, separator );
}
} // namespace

std::string format_vehicle_location( const vehicle_grid_position &position )
{
    const int along = vehicle_section( position.x, position.min_x, position.max_x );
    const int across = vehicle_section( position.y, position.min_y, position.max_y );
    std::string location;
    if( along == 2 || along == 0 ) {
        if( across == 0 ) {
            location = along == 2 ? pgettext( "vehicle location", "front left" ) :
                       pgettext( "vehicle location", "rear left" );
        } else if( across == 2 ) {
            location = along == 2 ? pgettext( "vehicle location", "front right" ) :
                       pgettext( "vehicle location", "rear right" );
        } else {
            location = along == 2 ? pgettext( "vehicle location", "front" ) :
                       pgettext( "vehicle location", "rear" );
        }
    } else if( across == 0 ) {
        location = pgettext( "vehicle location", "left" );
    } else if( across == 2 ) {
        location = pgettext( "vehicle location", "right" );
    } else {
        location = pgettext( "vehicle location", "center" );
    }
    const bool outside = position.x < position.min_x || position.x > position.max_x ||
                         position.y < position.min_y || position.y > position.max_y;
    if( outside ) {
        //~ Grid tile beyond a vehicle: %s is the nearest location on the vehicle, such as "front left".
        return string_format( pgettext( "vehicle location", "outside, %s" ), location );
    }
    return location;
}

std::string format_vehicle_position_details( const vehicle_grid_position &position )
{
    std::string along;
    if( position.x > position.max_x ) {
        const int distance = position.x - position.max_x;
        along = string_format( n_gettext( "%d tile in front of the vehicle",
                                          "%d tiles in front of the vehicle", distance ), distance );
    } else if( position.x < position.min_x ) {
        const int distance = position.min_x - position.x;
        along = string_format( n_gettext( "%d tile behind the vehicle",
                                          "%d tiles behind the vehicle", distance ), distance );
    } else {
        //~ Row of a vehicle grid tile: %1$d is the row counted from the front, %2$d the number of rows.
        along = string_format( _( "row %1$d of %2$d from the front" ),
                               position.max_x - position.x + 1, position.max_x - position.min_x + 1 );
    }
    std::string across;
    if( position.y < position.min_y ) {
        const int distance = position.min_y - position.y;
        across = string_format( n_gettext( "%d tile left of the vehicle",
                                           "%d tiles left of the vehicle", distance ), distance );
    } else if( position.y > position.max_y ) {
        const int distance = position.y - position.max_y;
        across = string_format( n_gettext( "%d tile right of the vehicle",
                                           "%d tiles right of the vehicle", distance ), distance );
    } else {
        //~ Column of a vehicle grid tile: %1$d is the column counted from the left, %2$d the number of columns.
        across = string_format( _( "column %1$d of %2$d from the left" ),
                                position.y - position.min_y + 1, position.max_y - position.min_y + 1 );
    }
    std::string result = string_join( std::vector<std::string> { along, across }, _( ", " ) );
    result[0] = static_cast<char>( std::toupper( static_cast<unsigned char>( result[0] ) ) );
    return format_semantic_sequence( { result } );
}

std::string format_vehicle_part_condition( const bool broken, const int damage_percent )
{
    if( broken ) {
        return pgettext( "vehicle part", "broken" );
    }
    if( damage_percent > 0 ) {
        //~ Damage of a vehicle part: %d is the percentage of its durability lost.
        return string_format( pgettext( "vehicle part", "damaged %d%%" ), damage_percent );
    }
    return std::string();
}

std::string format_vehicle_part_row( const vehicle_part_row &row )
{
    std::vector<std::string> parts = { row.name, row.location,
                                       format_vehicle_part_condition( row.broken, row.damage_percent )
                                     };
    parts.insert( parts.end(), row.states.begin(), row.states.end() );
    return without_empty_joined( parts, _( ", " ) );
}

std::string format_vehicle_tile_part( const vehicle_part_row &row )
{
    vehicle_part_row without_location = row;
    without_location.location.clear();
    return format_vehicle_part_row( without_location );
}

std::string format_vehicle_tile( const std::string &location, const std::vector<std::string> &parts,
                                 const bool obstructed )
{
    std::string contents = parts.empty() ? pgettext( "vehicle tile", "no parts" ) :
                           //~ Separator between the parts on one vehicle tile.
                           string_join( parts, pgettext( "vehicle tile", "; " ) );
    if( obstructed ) {
        //~ A vehicle grid tile where nothing can be installed because terrain or another vehicle is in the way.
        contents += pgettext( "vehicle tile", ", obstructed" );
    }
    //~ Vehicle grid tile: %1$s is its location, such as "front left", %2$s the parts on it.
    return format_semantic_sequence( { string_format( pgettext( "vehicle tile", "%1$s: %2$s" ),
                                       location, contents )
                                     } );
}

std::string format_vehicle_part_count( const size_t count )
{
    if( count == 0 ) {
        return _( "No parts" );
    }
    return string_format( n_gettext( "%d part", "%d parts", count ), static_cast<int>( count ) );
}

std::string format_vehicle_install_count( const size_t count )
{
    if( count == 0 ) {
        return _( "No installable parts" );
    }
    return string_format( n_gettext( "%d installable part", "%d installable parts", count ),
                          static_cast<int>( count ) );
}

std::string format_vehicle_task( const std::string &task, const std::string &unavailable_reason )
{
    return format_semantic_sequence( { task, unavailable_reason.empty() ? std::string() :
                                       string_format( _( "Unavailable: %s" ), unavailable_reason )
                                     } );
}

std::string format_vehicle_task_blocked( const std::string &task, const std::string &part,
        const std::string &reason )
{
    return format_semantic_sequence( { without_empty_joined( { task, part }, " " ),
                                       string_format( _( "Unavailable: %s" ), reason )
                                     } );
}

std::string format_vehicle_missing_requirements( const size_t missing )
{
    if( missing == 0 ) {
        return _( "requirements not met" );
    }
    //~ %d: number of unmet requirements of a vehicle task
    return string_format( n_gettext( "%d requirement missing", "%d requirements missing", missing ),
                          static_cast<int>( missing ) );
}

std::string format_vehicle_skills( const std::vector<vehicle_skill_requirement> &skills )
{
    std::vector<std::string> levels;
    for( const vehicle_skill_requirement &skill : skills ) {
        //~ Skill a vehicle task requires: %1$s is the skill, %2$d its level, %3$s "have" or "missing".
        levels.push_back( string_format( _( "%1$s %2$d, %3$s" ), skill.skill, skill.level,
                                         format_requirement_state( skill.met ? requirement_state::have :
                                                 requirement_state::missing ) ) );
    }
    //~ %s: skills with their levels and states, or "none"
    return format_semantic_sequence( { string_format( _( "Skills required: %s" ),
                                       levels.empty() ? pgettext( "recipe requirement", "none" ) :
                                       string_join( levels, pgettext( "recipe requirement", "; " ) ) )
                                     } );
}

std::string format_vehicle_extra_skill( const std::string &skill, const int level, const bool met,
                                        const std::string &purpose )
{
    //~ Additional skill a vehicle installation needs: %1$s is the skill, %2$d its level, %3$s its purpose such as "for extra engines", %4$s "have" or "missing".
    return format_semantic_sequence( { string_format( _( "%1$s %2$d %3$s, %4$s" ), skill, level,
                                       purpose, format_requirement_state( met ? requirement_state::have :
                                               requirement_state::missing ) )
                                     } );
}

std::string format_vehicle_task_time( const std::string &time )
{
    //~ %s: time needed for a vehicle task
    return format_semantic_sequence( { string_format( _( "Time required: %s" ), time ) } );
}

std::string format_vehicle_lift_requirement( const std::string &tool, const bool tool_met,
        const std::string &strength, const bool strength_met )
{
    const auto state = []( const bool met ) {
        return format_requirement_state( met ? requirement_state::have : requirement_state::missing );
    };
    //~ Lifting a vehicle part: %1$s is a lifting tool, %2$s its state, %3$s the strength needed instead, %4$s its state.
    return format_semantic_sequence( { string_format( _( "Lifting: %1$s, %2$s; or %3$s, %4$s" ),
                                       tool, state( tool_met ), strength, state( strength_met ) )
                                     } );
}

std::string format_vehicle_workshop_stats( const vehicle_workshop_stats &stats )
{
    std::vector<std::string> parts;
    parts.push_back( stats.owner.empty() ? stats.name :
                     //~ Examined vehicle: %1$s is its name, %2$s its owner.
                     string_format( _( "%1$s, owned by %2$s" ), stats.name, stats.owner ) );
    for( const vehicle_speed_stats &speed : stats.speeds ) {
        //~ Vehicle speeds: %1$d safe speed, %2$d top speed, %3$d acceleration per second, %4$s speed unit.
        std::string figures = string_format( _( "safe speed %1$d %4$s, top speed %2$d %4$s, "
                                                "acceleration %3$d %4$s per second" ),
                                             speed.safe, speed.top, speed.acceleration, stats.units );
        if( speed.medium.empty() ) {
            figures[0] = static_cast<char>( std::toupper( static_cast<unsigned char>( figures[0] ) ) );
        } else {
            //~ %1$s: "On water" or "In the air", %2$s: vehicle speeds there
            figures = string_format( _( "%1$s: %2$s" ), speed.medium, figures );
        }
        parts.push_back( figures );
    }
    parts.push_back( string_format( _( "Mass: %s" ), stats.mass ) );
    //~ Vehicle cargo: %1$s is the used volume, %2$s the total volume, both with units.
    parts.push_back( string_format( _( "Cargo: %1$s of %2$s" ), stats.cargo_used, stats.cargo_total ) );
    parts.push_back( string_format( _( "Status: %s" ), stats.status ) );
    parts.push_back( stats.wheels );
    if( !stats.most_damaged.empty() ) {
        parts.push_back( stats.most_damaged_repairable ?
                         string_format( _( "Most damaged: %s" ), stats.most_damaged ) :
                         string_format( _( "Most damaged, can't repair: %s" ), stats.most_damaged ) );
    }
    if( !stats.needs_repair.empty() ) {
        parts.push_back( string_format( _( "Needs repair: %s" ), stats.needs_repair ) );
    }
    for( const vehicle_fuel_gauge &fuel : stats.fuels ) {
        parts.push_back( format_vehicle_fuel( fuel ) );
    }
    parts.push_back( string_format( _( "Air drag %.2f" ), stats.air_drag ) );
    if( stats.has_water_drag ) {
        parts.push_back( string_format( _( "Water drag %.2f" ), stats.water_drag ) );
    }
    if( stats.has_rolling_drag ) {
        parts.push_back( string_format( _( "Rolling drag %.2f" ), stats.rolling_drag ) );
    }
    parts.push_back( string_format( _( "Static drag %d" ), stats.static_drag ) );
    parts.push_back( string_format( _( "Offroad %d%%" ), stats.offroad ) );
    if( stats.has_draft ) {
        //~ Boat draft and clearance in meters.
        parts.push_back( string_format( _( "Draft %1$.2f meters, clearance %2$.2f meters" ),
                                        stats.draft, stats.clearance ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_pickup_result( const std::vector<pickup_result_item> &picked,
                                  const std::vector<pickup_failure> &failed )
{
    std::vector<std::string> parts;
    if( !picked.empty() ) {
        std::vector<std::string> items;
        for( const pickup_result_item &entry : picked ) {
            items.push_back( entry.destination.empty() ? entry.item :
                             //~ Picked-up item and the container it went into
                             string_format( _( "%1$s into %2$s" ), entry.item, entry.destination ) );
        }
        parts.push_back( string_format( _( "Picked up: %s" ), string_join( items, _( ", " ) ) ) );
    }
    if( !failed.empty() ) {
        std::vector<std::string> items;
        for( const pickup_failure &entry : failed ) {
            items.push_back( entry.reason.empty() ? entry.item :
                             //~ Item left behind and the reason
                             string_format( _( "%1$s, %2$s" ), entry.item, entry.reason ) );
        }
        parts.push_back( string_format( _( "Not picked up: %s" ), string_join( items, _( "; " ) ) ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_drop_result( const std::string &destination,
                                const std::vector<std::string> &items, const bool onto )
{
    if( items.empty() ) {
        return std::string();
    }
    const std::string list = string_join( items, _( ", " ) );
    return format_semantic_sequence( {
        onto ? string_format( _( "Dropped on %1$s: %2$s" ), destination, list ) :
        string_format( _( "Put in %1$s: %2$s" ), destination, list )
    } );
}

std::string format_transfer_row( const transfer_row &row, const bool include_category,
                                 const bool detailed )
{
    std::vector<std::string> parts;
    if( ( include_category || detailed ) && !row.category.empty() ) {
        parts.push_back( row.category );
    }
    parts.push_back( row.count > 1 ? string_format( _( "%1$s, %2$d" ), row.name, row.count ) :
                     row.name );
    if( row.not_owned ) {
        parts.push_back( _( "Not owned" ) );
    }
    parts.push_back( row.weight );
    parts.push_back( row.volume );
    if( detailed ) {
        if( !row.source_area.empty() ) {
            parts.push_back( string_format( _( "From: %s" ), row.source_area ) );
        }
        if( row.auto_pickup ) {
            parts.push_back( _( "Auto pickup" ) );
        }
        if( row.open_in_other_pane ) {
            parts.push_back( _( "Open in the other pane" ) );
        }
    }
    return format_semantic_sequence( parts );
}

std::string format_transfer_pane( const transfer_pane &pane, const bool source,
                                  const bool include_capacity )
{
    std::vector<std::string> parts;
    parts.push_back( string_format( source ? _( "Source: %s" ) : _( "Destination: %s" ),
                                    pane.area ) );
    parts.push_back( string_format( n_gettext( "%d item", "%d items", pane.item_count ),
                                    static_cast<int>( pane.item_count ) ) );
    if( include_capacity ) {
        parts.push_back( pane.capacity );
    }
    return format_semantic_sequence( parts );
}

std::string format_transfer_status( const transfer_pane &source,
                                    const transfer_pane &destination )
{
    std::vector<std::string> parts;
    for( const transfer_pane *pane : {
             &source, &destination
         } ) {
        parts.push_back( format_transfer_pane( *pane, pane == &source, true ) );
        if( !pane->filter.empty() ) {
            parts.push_back( string_format( _( "Filter: %s" ), pane->filter ) );
        }
        if( !pane->sort.empty() ) {
            parts.push_back( string_format( _( "Sort: %s" ), pane->sort ) );
        }
    }
    return format_semantic_sequence( parts );
}

std::string format_transfer_result( const std::string &item_name, const int moved,
                                    const int requested, const std::string &destination )
{
    if( item_name.empty() ) {
        if( moved >= requested ) {
            return string_format( n_gettext( "Moved %1$d stack to %2$s.", "Moved %1$d stacks to %2$s.",
                                             moved ), moved, destination );
        }
        return string_format( n_gettext( "Moved %1$d of %2$d stack to %3$s.",
                                         "Moved %1$d of %2$d stacks to %3$s.", requested ),
                              moved, requested, destination );
    }
    if( moved >= requested && requested == 1 ) {
        return string_format( _( "Moved %1$s to %2$s." ), item_name, destination );
    }
    if( moved >= requested ) {
        return string_format( _( "Moved %1$s, %2$d, to %3$s." ), item_name, moved, destination );
    }
    return string_format( _( "Moved %1$d of %2$d %3$s to %4$s." ), moved, requested, item_name,
                          destination );
}

std::string format_inventory_comparison_choice( const std::string &first,
        const std::string &second )
{
    return string_format( _( "Second comparison item: %s. First comparison item: %s" ),
                          second, first );
}

inventory_comparison_relation compare_inventory_values( const double first,
        const double second, const bool lower_is_better )
{
    if( std::fabs( first - second ) < 0.01 ) {
        return inventory_comparison_relation::equal;
    }
    const bool first_better = lower_is_better ? first < second : first > second;
    return first_better ? inventory_comparison_relation::first_better :
           inventory_comparison_relation::second_better;
}

std::string format_inventory_comparison_stat( const std::string &label,
        const std::string &first_name, const std::string &first_value,
        const std::string &second_name, const std::string &second_value,
        const inventory_comparison_relation relation )
{
    std::string outcome;
    switch( relation ) {
        case inventory_comparison_relation::first_better:
            outcome = _( "First is better" );
            break;
        case inventory_comparison_relation::second_better:
            outcome = _( "Second is better" );
            break;
        case inventory_comparison_relation::equal:
            outcome = _( "Equal" );
            break;
        case inventory_comparison_relation::unknown:
            break;
    }
    return format_semantic_sequence( {
        label,
        string_format( _( "First, %s: %s" ), first_name, first_value ),
        string_format( _( "Second, %s: %s" ), second_name, second_value ),
        outcome
    } );
}

std::string normalize_status_announcement( const std::string &message, const bool progress )
{
    if( !progress ) {
        return collapse_whitespace( message );
    }
    static const std::regex fraction( R"(\s*\[\s*\d+\s*/\s*\d+\s*\])" );
    static const std::regex percentage( R"(\s*\d+(\.\d+)?\s*%)" );
    std::string result = std::regex_replace( message, fraction, "" );
    result = std::regex_replace( result, percentage, "" );
    return collapse_whitespace( result );
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
    const auto contains = []( const std::vector<std::string> &limbs, const std::string & limb ) {
        return std::find( limbs.begin(), limbs.end(), limb ) != limbs.end();
    };

    for( const std::string &limb : current.bleeding_limbs ) {
        if( !contains( tracker.bleeding_limbs, limb ) ) {
            alerts.push_back( string_format( _( "Bleeding: %s!" ), limb ) );
        }
    }
    for( const std::string &limb : tracker.bleeding_limbs ) {
        if( !contains( current.bleeding_limbs, limb ) ) {
            alerts.push_back( string_format( _( "Stopped bleeding: %s." ), limb ) );
        }
    }
    for( const std::string &limb : current.broken_limbs ) {
        if( !contains( tracker.broken_limbs, limb ) ) {
            alerts.push_back( string_format( _( "Broken limb: %s!" ), limb ) );
        }
    }

    if( current.stamina_critical && !tracker.stamina_critical ) {
        alerts.push_back( _( "Stamina critical!" ) );
    }

    const auto tier_changed = [&]( const std::string & previous, const std::string & now,
    const char *format ) {
        if( !now.empty() && now != previous ) {
            alerts.push_back( string_format( format, now ) );
        }
    };
    tier_changed( tracker.pain_tier, current.pain_tier, _( "Pain: %s." ) );
    tier_changed( tracker.hunger_tier, current.hunger_tier, _( "Hunger: %s." ) );
    tier_changed( tracker.thirst_tier, current.thirst_tier, _( "Thirst: %s." ) );
    tier_changed( tracker.sleepiness_tier, current.sleepiness_tier, _( "Sleepiness: %s." ) );
    tier_changed( tracker.weariness_tier, current.weariness_tier, _( "Weariness: %s." ) );

    tracker = current;
    tracker.initialized = true;
    return alerts;
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

std::string format_threat_description( const std::string &name, const int distance,
                                       const std::string &direction )
{
    return string_format( pgettext( "monster description", "%1$s %2$s %3$s" ), name,
                          string_format( n_gettext( "%d tile", "%d tiles", distance ), distance ),
                          string_format( _( "to the %s" ), direction ) );
}

std::string format_threat_announcement( const std::vector<std::string> &descriptions,
                                        const size_t others )
{
    if( descriptions.empty() ) {
        return _( "No threats in view" );
    }
    std::vector<std::string> parts = descriptions;
    if( others > 0 ) {
        parts.push_back( string_format( n_gettext( "%d other", "%d others", others ),
                                        static_cast<int>( others ) ) );
    }
    return string_format( _( "%s spotted!" ), enumerate_as_string( parts ) );
}

std::string format_game_arrival( const bool loaded, const std::string &name,
                                 const std::string &location )
{
    return loaded ?
           string_format( _( "Welcome back, %1$s. Location: %2$s" ), name, location ) :
           string_format( _( "Welcome, %1$s. Location: %2$s" ), name, location );
}

std::string format_direction_prompt( const std::string &prompt,
                                     const std::vector<direction_target> &targets, const std::string &hint )
{
    std::vector<std::pair<std::string, std::vector<std::string>>> groups;
    for( const direction_target &entry : targets ) {
        const auto group = std::find_if( groups.begin(), groups.end(),
        [&entry]( const std::pair<std::string, std::vector<std::string>> &candidate ) {
            return candidate.first == entry.target;
        } );
        if( group == groups.end() ) {
            groups.emplace_back( entry.target, std::vector<std::string> { entry.direction } );
        } else {
            group->second.push_back( entry.direction );
        }
    }
    std::string target_text;
    if( groups.empty() ) {
        target_text = _( "No valid targets." );
    } else {
        std::vector<std::string> described;
        described.reserve( groups.size() );
        for( const std::pair<std::string, std::vector<std::string>> &group : groups ) {
            //~ %1$s: target such as "wooden door"; %2$s: directions such as "north, east"
            described.push_back( string_format( _( "%1$s, %2$s" ), group.first,
                                                string_join( group.second, _( ", " ) ) ) );
        }
        //~ %s: valid targets of a direction prompt separated by semicolons
        target_text = string_format( _( "Targets: %s." ), string_join( described, _( "; " ) ) );
    }
    std::vector<std::string> parts;
    for( const std::string &part : {
             trim_ascii( prompt ), target_text, trim_ascii( hint )
         } ) {
        if( !part.empty() ) {
            parts.push_back( part );
        }
    }
    return string_join( parts, " " );
}

std::string format_direction_auto_selected( const direction_target &target )
{
    //~ %1$s: target such as "wooden door"; %2$s: direction such as "north"
    return string_format( _( "Auto-selected: %1$s, %2$s." ), target.target, target.direction );
}

std::string format_direction_invalid( const direction_target &target )
{
    //~ %1$s: what is in the chosen direction; %2$s: direction such as "north"
    return string_format( _( "%1$s, %2$s: not a valid target." ), target.target,
                          target.direction );
}

std::vector<travel_segment> merge_travel_segments( const std::vector<std::string> &names )
{
    std::vector<travel_segment> segments;
    for( const std::string &name : names ) {
        if( segments.empty() || segments.back().name != name ) {
            segments.push_back( { name, 0 } );
        }
        segments.back().count++;
    }
    return segments;
}

std::string format_travel_start( const std::string &destination, const std::string &location,
                                 const int steps, const std::vector<travel_segment> &segments )
{
    constexpr size_t max_segments = 4;
    //~ %1$s: destination such as "wooden door"; %2$s: distance and direction such as "12 tiles northeast"
    std::string result = string_format( _( "Traveling to %1$s, %2$s" ), destination, location );
    if( steps > 0 ) {
        //~ %1$s: travel destination and distance; %2$d: number of steps along the route
        result = string_format( n_gettext( "%1$s, %2$d step", "%1$s, %2$d steps", steps ), result,
                                steps );
    }
    if( segments.empty() ) {
        return result + _( "." );
    }
    std::vector<std::string> described;
    for( size_t i = 0; i < segments.size() && i < max_segments; ++i ) {
        //~ %1$s: terrain such as "grass"; %2$d: number of tiles of it along the route
        described.push_back( string_format( _( "%1$s %2$d" ), segments[i].name, segments[i].count ) );
    }
    if( segments.size() > max_segments ) {
        const int more = static_cast<int>( segments.size() - max_segments );
        //~ %d: number of route segments not described
        described.push_back( string_format( n_gettext( "%d more", "%d more", more ), more ) );
    }
    //~ %1$s: travel destination, distance, and steps; %2$s: route segments separated by commas
    return string_format( _( "%1$s: %2$s." ), result, string_join( described, _( ", " ) ) );
}

std::string format_travel_arrival( const std::string &tile )
{
    //~ %s: terrain or furniture where auto-travel ended
    return string_format( _( "Arrived: %s." ), tile );
}

std::string format_travel_stopped( const std::string &destination, const std::string &location )
{
    //~ %1$s: travel destination; %2$s: its distance and direction such as "8 tiles north"
    return string_format( _( "Travel stopped: %1$s, %2$s." ), destination, location );
}

std::string format_target_hit_chance( const int chance )
{
    //~ %d: percent chance to hit the target in target mode
    return string_format( _( "%d%% hit" ), chance );
}

std::string format_target_aim_level( const target_aim_level &level )
{
    //~ %s: aim level such as "Careful"
    std::string name = string_format( _( "%s aim" ), level.name );
    if( level.selected ) {
        //~ %s: aim level name; the level used by the fire key
        name = string_format( _( "%s, selected" ), name );
    }
    if( !level.key.empty() ) {
        //~ %1$s: aim level name; %2$s: key that fires with this aim level
        name = string_format( _( "%1$s, key %2$s" ), name, level.key );
    }
    std::vector<std::string> outcomes;
    outcomes.reserve( level.outcomes.size() );
    for( const std::pair<std::string, int> &outcome : level.outcomes ) {
        //~ %1$s: hit outcome such as "Graze"; %2$d: its percent chance
        outcomes.push_back( string_format( _( "%1$s %2$d%%" ), outcome.first, outcome.second ) );
    }
    //~ %d: moves needed to aim and fire
    const std::string moves = string_format( n_gettext( "%d move to fire", "%d moves to fire",
                              level.moves ), level.moves );
    return format_semantic_sequence( {
        //~ %1$s: aim level name; %2$s: hit chance such as "62% hit"
        string_format( _( "%1$s: %2$s" ), name, format_target_hit_chance( level.hit_chance ) ),
        string_join( outcomes, _( ", " ) ), moves
    } );
}

std::string format_target_problems( const target_focus &focus )
{
    std::vector<std::string> parts;
    if( focus.out_of_ammo ) {
        parts.emplace_back( _( "Out of ammo" ) );
    }
    if( focus.no_turret_in_range ) {
        parts.emplace_back( _( "No turret in range" ) );
    }
    if( focus.out_of_range ) {
        parts.emplace_back( _( "Out of range" ) );
    }
    if( focus.invalid ) {
        parts.emplace_back( _( "Not a valid target" ) );
    }
    if( !focus.blocked_by.empty() ) {
        //~ %s: visible obstacle such as "brick wall" between the player and the target
        parts.push_back( string_format( _( "No line of fire, blocked by %s" ), focus.blocked_by ) );
    }
    if( !focus.in_line_of_fire.empty() ) {
        //~ %s: creatures standing between the player and the target
        parts.push_back( string_format( _( "In line of fire: %s" ),
                                        string_join( focus.in_line_of_fire, _( ", " ) ) ) );
    }
    if( focus.in_area_of_effect ) {
        parts.emplace_back( _( "You are in the area of effect" ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_target_focus( const target_focus &focus, const bool as_tile )
{
    std::vector<std::string> parts;
    if( as_tile || focus.target.empty() ) {
        parts = focus.tile;
        parts.push_back( focus.hit );
    } else {
        std::vector<std::string> fields;
        for( const std::string &field : {
                 focus.target, focus.attitude, focus.location, focus.hit
             } ) {
            if( !field.empty() ) {
                fields.push_back( field );
            }
        }
        parts.push_back( string_join( fields, _( ", " ) ) );
    }
    parts.push_back( format_target_problems( focus ) );
    return format_semantic_sequence( parts );
}

std::string format_target_count( const size_t count )
{
    if( count == 0 ) {
        return _( "No targets" );
    }
    const int targets = static_cast<int>( count );
    return string_format( n_gettext( "%d target", "%d targets", targets ), targets );
}

std::string format_target_ammo( const std::string &ammo, const int remaining,
                                const int capacity )
{
    if( remaining <= 0 ) {
        //~ %s: ammunition name
        return string_format( _( "Ammo: %s" ), ammo );
    }
    //~ %1$s: ammunition name; %2$d: loaded rounds; %3$d: capacity
    return string_format( _( "Ammo: %1$s, %2$d of %3$d" ), ammo, remaining, capacity );
}

std::string format_target_open( const std::string &title, const std::string &weapon,
                                const size_t target_count, const std::string &focus )
{
    return format_semantic_sequence( { title, weapon, format_target_count( target_count ), focus } );
}

std::string format_target_status( const target_status &status )
{
    std::vector<std::string> parts;
    if( !status.firing_mode.empty() ) {
        //~ %s: gun firing mode such as "semi-auto (1)"
        parts.push_back( string_format( _( "Firing mode: %s" ), status.firing_mode ) );
    }
    if( !status.aim_mode.empty() ) {
        //~ %s: selected aim level such as "Careful"
        parts.push_back( string_format( _( "Aim: %s" ), status.aim_mode ) );
    }
    parts.push_back( status.ammo );
    if( !status.recoil.empty() ) {
        //~ %s: recoil level such as "Low"
        parts.push_back( string_format( _( "Recoil: %s" ), status.recoil ) );
    }
    if( status.steadiness >= 0 ) {
        //~ %d: percent steadiness of the aim
        parts.push_back( string_format( _( "Steadiness: %d%%" ), status.steadiness ) );
    }
    parts.push_back( status.distance > 0 ?
                     //~ %1$d: distance to the cursor; %2$d: maximum range
                     string_format( _( "Range: %1$d of %2$d" ), status.distance, status.range ) :
                     //~ %d: maximum range
                     string_format( _( "Range: %d" ), status.range ) );
    parts.insert( parts.end(), status.extra.begin(), status.extra.end() );
    return format_semantic_sequence( parts );
}

std::string format_requirement_state( const requirement_state state )
{
    switch( state ) {
        case requirement_state::have:
            return pgettext( "recipe requirement", "have" );
        case requirement_state::not_enough:
            return pgettext( "recipe requirement", "not enough" );
        case requirement_state::missing:
            break;
        case requirement_state::uses_favorite:
            return pgettext( "recipe requirement", "have, uses a favorite" );
        case requirement_state::uses_filled_container:
            return pgettext( "recipe requirement", "have, uses a filled container" );
        case requirement_state::uses_hallucinogen_or_poison:
            return pgettext( "recipe requirement", "have, hallucinogenic or poisonous" );
    }
    return pgettext( "recipe requirement", "missing" );
}

bool requirement_group_met( const requirement_group &group )
{
    return group.options.empty() ||
    std::any_of( group.options.begin(), group.options.end(), []( const requirement_option & option ) {
        return option.state != requirement_state::missing &&
               option.state != requirement_state::not_enough;
    } );
}

size_t count_missing_requirement_groups( const std::vector<requirement_group> &groups )
{
    return std::count_if( groups.begin(), groups.end(), []( const requirement_group & group ) {
        return !requirement_group_met( group );
    } );
}

std::string format_requirement_group( const requirement_group &group )
{
    std::vector<std::string> options;
    options.reserve( group.options.size() );
    for( const requirement_option &option : group.options ) {
        //~ %1$s: tool or component such as "2 rags", %2$s: state such as "have" or "missing"
        options.push_back( string_format( pgettext( "recipe requirement", "%1$s, %2$s" ),
                                          option.name, format_requirement_state( option.state ) ) );
    }
    const std::string label = group.kind == requirement_kind::tools ?
                              pgettext( "recipe requirement", "Tools" ) :
                              pgettext( "recipe requirement", "Components" );
    //~ %1$s: "Tools" or "Components", %2$s: alternatives with their states
    return format_semantic_sequence( { string_format( pgettext( "recipe requirement", "%1$s: %2$s" ),
                                       label, options.empty() ? pgettext( "recipe requirement", "none" ) :
                                       //~ Separator between alternatives of one recipe requirement.
                                       string_join( options, pgettext( "recipe requirement", "; or " ) ) )
                                     } );
}

std::string format_recipe_availability( const bool craftable, const size_t missing )
{
    if( craftable ) {
        return pgettext( "recipe", "craftable" );
    }
    if( missing > 0 ) {
        //~ %d: number of unmet recipe requirements
        return string_format( n_gettext( "%d missing", "%d missing", missing ),
                              static_cast<int>( missing ) );
    }
    return pgettext( "recipe", "not craftable" );
}

std::string format_recipe_row( const recipe_row &row )
{
    std::vector<std::string> parts = { row.name };
    if( row.group ) {
        parts.emplace_back( pgettext( "recipe", "group" ) );
        parts.emplace_back( row.expanded ? pgettext( "recipe group", "expanded" ) :
                            pgettext( "recipe group", "collapsed" ) );
        parts.emplace_back( row.craftable ? pgettext( "recipe", "craftable" ) :
                            pgettext( "recipe group", "nothing craftable" ) );
    } else {
        parts.push_back( format_recipe_availability( row.craftable, row.missing ) );
    }
    parts.insert( parts.end(), row.warnings.begin(), row.warnings.end() );
    if( row.favorite ) {
        parts.emplace_back( pgettext( "recipe", "favorite" ) );
    }
    if( row.unread ) {
        parts.emplace_back( pgettext( "recipe", "new" ) );
    }
    parts.erase( std::remove( parts.begin(), parts.end(), std::string() ), parts.end() );
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_recipe_count( const size_t count )
{
    if( count == 0 ) {
        return _( "No recipes" );
    }
    return string_format( n_gettext( "%d recipe", "%d recipes", count ), static_cast<int>( count ) );
}

std::string format_recipe_list( const std::string &name, const size_t count, const size_t hidden,
                                const bool unread )
{
    std::vector<std::string> parts;
    if( !name.empty() ) {
        parts.push_back( name );
    }
    parts.push_back( format_recipe_count( count ) );
    if( hidden > 0 ) {
        //~ %d: number of hidden recipes in the category
        parts.push_back( string_format( n_gettext( "%d hidden", "%d hidden", hidden ),
                                        static_cast<int>( hidden ) ) );
    }
    if( unread ) {
        parts.emplace_back( _( "new recipes" ) );
    }
    return string_join( parts, _( ", " ) );
}

std::string format_recipe_batch( const int batch_size, const std::string &time,
                                 const std::string &availability )
{
    std::vector<std::string> parts = {
        //~ %d: number of recipe copies crafted together
        string_format( _( "Batch size %d" ), batch_size )
    };
    if( !time.empty() ) {
        parts.push_back( time );
    }
    if( !availability.empty() ) {
        parts.push_back( availability );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_construction_row( const std::string &name, const bool available,
                                     const std::string &unavailable_reason, const size_t variants )
{
    std::vector<std::string> parts = { name };
    parts.push_back( available ? _( "available" ) :
                     string_format( _( "Unavailable: %s" ), unavailable_reason ) );
    if( variants > 1 ) {
        parts.push_back( string_format( n_gettext( "%d variant", "%d variants", variants ),
                                        static_cast<int>( variants ) ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_construction_list( const std::string &category, const size_t count )
{
    const std::string count_text = count == 0 ? _( "No projects" ) :
                                   string_format( n_gettext( "%d project", "%d projects", count ),
                                           static_cast<int>( count ) );
    return string_join( std::vector<std::string> { category, count_text }, _( ", " ) );
}

std::string format_character_stat( const std::string &name, const int current, const int base )
{
    //~ Character sheet stat: %1$s: stat name, %2$d: current value, %3$d: base value
    return format_semantic_sequence( { string_format( _( "%1$s %2$d, base %3$d" ), name, current,
                                       base ) } );
}

std::string format_character_value( const std::string &label, const std::string &value )
{
    return format_semantic_sequence( { string_join( std::vector<std::string> {
            label, remove_color_tags( value )
        }, _( ", " ) )
    } );
}

std::string format_body_warmth( const body_warmth warmth )
{
    switch( warmth ) {
        case body_warmth::none:
            return std::string();
        case body_warmth::freezing:
            return pgettext( "body warmth", "freezing" );
        case body_warmth::very_cold:
            return pgettext( "body warmth", "very cold" );
        case body_warmth::chilly:
            return pgettext( "body warmth", "chilly" );
        case body_warmth::comfortable:
            return pgettext( "body warmth", "comfortable" );
        case body_warmth::warm:
            return pgettext( "body warmth", "warm" );
        case body_warmth::very_hot:
            return pgettext( "body warmth", "very hot" );
        case body_warmth::scorching:
            return pgettext( "body warmth", "scorching" );
    }
    return std::string();
}

std::string format_encumbrance_row( const encumbrance_row &row, const bool detailed )
{
    std::vector<std::string> parts = {
        row.part,
        //~ %d: encumbrance of a body part without the layering penalty
        string_format( _( "encumbrance %d" ), row.encumbrance )
    };
    if( row.layering != 0 || detailed ) {
        //~ %d: extra encumbrance of a body part from layering clothing
        parts.push_back( string_format( _( "layering %d" ), row.layering ) );
    }
    if( detailed ) {
        //~ %d: total encumbrance of a body part
        parts.push_back( string_format( _( "total %d" ), row.encumbrance + row.layering ) );
    }
    const std::string state = format_body_warmth( row.warmth_state );
    if( detailed ) {
        //~ %d: temperature number of a body part as the character sheet shows it
        parts.push_back( string_format( _( "warmth %d" ), row.warmth ) );
        if( !state.empty() ) {
            parts.push_back( state );
        }
    } else if( !state.empty() ) {
        //~ %s: warmth band of a body part, such as "comfortable"
        parts.push_back( string_format( _( "warmth %s" ), state ) );
    }
    if( row.covered ) {
        //~ Sort armor encumbrance block: the body part is covered by the focused item
        parts.emplace_back( _( "covered by this item" ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_speed_row( const std::string &cause, const bool speed, const int value,
                              const bool percent )
{
    std::vector<std::string> parts = { remove_color_tags( cause ) };
    if( value != 0 ) {
        const std::string amount = percent ? string_format( "%+d%%", value ) :
                                   string_format( "%+d", value );
        //~ %s: signed change of speed, such as "+10" or "-5%"
        parts.push_back( string_format( speed ? _( "speed %s" ) : _( "move cost %s" ), amount ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_skill_row( const skill_row &row )
{
    //~ Skill on the character sheet: %1$s: skill name, %2$d: level
    std::vector<std::string> parts = { string_format( _( "%1$s %2$d" ), row.name, row.level ) };
    if( row.set_by_bionic ) {
        parts.emplace_back( _( "set by bionic" ) );
    } else {
        parts.push_back( string_format( "%d%%", std::max( row.exercise, 0 ) ) );
    }
    if( !row.effective.empty() ) {
        //~ %s: effective dodge value
        parts.push_back( string_format( _( "effective %s" ), row.effective ) );
    }
    if( !row.set_by_bionic ) {
        if( row.rusty ) {
            parts.emplace_back( _( "rusty" ) );
        }
        parts.emplace_back( row.training ? _( "training" ) : _( "not training" ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_trait_row( const std::string &name, const trait_kind kind )
{
    std::string state;
    switch( kind ) {
        case trait_kind::positive:
            state = pgettext( "trait kind", "positive" );
            break;
        case trait_kind::negative:
            state = pgettext( "trait kind", "negative" );
            break;
        case trait_kind::neutral:
            state = pgettext( "trait kind", "neutral" );
            break;
        case trait_kind::mixed:
            state = pgettext( "trait kind", "mixed" );
            break;
        case trait_kind::threshold:
            state = pgettext( "trait kind", "threshold" );
            break;
        case trait_kind::profession:
            state = pgettext( "trait kind", "profession" );
            break;
        case trait_kind::attunement:
            state = pgettext( "trait kind", "attunement" );
            break;
        case trait_kind::debug:
            state = pgettext( "trait kind", "debug" );
            break;
    }
    return format_semantic_sequence( { string_join( std::vector<std::string> {
            remove_color_tags( name ), state
        }, _( ", " ) )
    } );
}

std::string format_bionic_row( const std::string &name, const int count )
{
    std::vector<std::string> parts = { name };
    if( count > 1 ) {
        //~ %d: number of installed bionics of one kind
        parts.push_back( string_format( n_gettext( "%d installed", "%d installed", count ), count ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_proficiency_row( const std::string &name, const bool known,
                                    const bool learning, const int progress )
{
    std::vector<std::string> parts = { remove_color_tags( name ) };
    if( known ) {
        parts.emplace_back( pgettext( "proficiency", "known" ) );
    } else if( learning ) {
        parts.push_back( string_format( "%d%%", progress ) );
        parts.emplace_back( pgettext( "proficiency", "learning" ) );
    } else {
        parts.emplace_back( pgettext( "proficiency", "not known" ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_character_tab( const std::string &tab, const std::vector<std::string> &header,
                                  const size_t count, const std::string &focus )
{
    std::vector<std::string> parts = { tab };
    for( const std::string &value : header ) {
        parts.push_back( remove_color_tags( value ) );
    }
    parts.push_back( format_list_count( count ) );
    if( count > 0 ) {
        parts.push_back( focus );
    }
    return format_semantic_sequence( parts );
}

std::string format_power_menu_mode( const power_menu_mode mode )
{
    switch( mode ) {
        case power_menu_mode::activating:
            return format_semantic_sequence( { _( "Activating" ) } );
        case power_menu_mode::examining:
            return format_semantic_sequence( { _( "Examining" ) } );
        case power_menu_mode::reassigning:
            return format_semantic_sequence( { _( "Reassigning" ),
                                               _( "Select an entry to assign a new letter" )
                                         } );
        case power_menu_mode::hiding:
            return format_semantic_sequence( { _( "Hiding" ),
                                               _( "Select an entry to hide or show its sprite" )
                                         } );
    }
    return std::string();
}

std::string format_assigned_key( const std::string &key )
{
    const std::string spoken = format_spoken_key( key );
    if( spoken.empty() ) {
        return _( "No key" );
    }
    //~ %s: the letter that selects a bionic or mutation
    return string_format( _( "Key %s" ), spoken );
}

std::string format_bionic_power_row( const bionic_power_row &row )
{
    std::vector<std::string> parts = { remove_color_tags( row.name ) };
    if( row.powered ) {
        parts.emplace_back( pgettext( "bionic state", "on" ) );
    } else if( row.toggled ) {
        parts.emplace_back( pgettext( "bionic state", "off" ) );
    }
    if( row.power_source ) {
        parts.emplace_back( _( "power source" ) );
    }
    if( !row.activate.empty() ) {
        //~ %s: energy used to activate a bionic, such as "50 kJ"
        parts.push_back( string_format( _( "%s to activate" ), row.activate ) );
    }
    if( !row.deactivate.empty() ) {
        //~ %s: energy used to deactivate a bionic
        parts.push_back( string_format( _( "%s to deactivate" ), row.deactivate ) );
    }
    if( !row.trigger.empty() ) {
        //~ %s: energy used each time a bionic triggers
        parts.push_back( string_format( _( "%s per trigger" ), row.trigger ) );
    }
    if( !row.over_time.empty() ) {
        parts.push_back( row.charge_turns <= 1 ?
                         //~ %s: energy a running bionic uses each turn
                         string_format( _( "%s per turn" ), row.over_time ) :
                         //~ %1$s: energy a running bionic uses, %2$d: number of turns
                         string_format( _( "%1$s per %2$d turns" ), row.over_time, row.charge_turns ) );
    }
    if( row.incapacitated ) {
        parts.emplace_back( _( "incapacitated" ) );
    }
    if( row.sprite_hidden ) {
        parts.emplace_back( _( "sprite hidden" ) );
    }
    if( row.fuel_saving >= 0 ) {
        //~ %d: power percentage above which a bionic stops burning fuel
        parts.push_back( string_format( _( "fuel saving above %d%%" ), row.fuel_saving ) );
        if( row.fuel_saving_inactive ) {
            parts.emplace_back( _( "fuel saving inactive" ) );
        }
    }
    return format_semantic_sequence( {
        string_join( parts, _( ", " ) ),
        row.not_enough_power ? _( "Unavailable: not enough power" ) : std::string()
    } );
}

std::string format_bionic_power( const std::string &current, const std::string &maximum,
                                 const std::vector<std::string> &fuels )
{
    std::vector<std::string> fuel_parts;
    for( const std::string &fuel : fuels ) {
        fuel_parts.push_back( remove_color_tags( fuel ) );
    }
    return format_semantic_sequence( {
        //~ %1$s: current bionic power, %2$s: maximum bionic power
        string_format( _( "Bionic power %1$s of %2$s" ), current, maximum ),
        fuel_parts.empty() ? std::string() :
        //~ %s: list of fuels available to bionics with their amounts
        string_format( _( "Available fuel: %s" ), string_join( fuel_parts, _( ", " ) ) )
    } );
}

std::string format_mutation_power_row( const mutation_power_row &row )
{
    std::vector<std::string> parts = { remove_color_tags( row.name ) };
    if( row.activated ) {
        parts.emplace_back( row.powered ? pgettext( "mutation state", "on" ) :
                            pgettext( "mutation state", "off" ) );
    }
    const std::string resources = enumerate_as_string( row.resources );
    if( row.cost > 0 ) {
        //~ %1$d: activation cost of a mutation, %2$s: resources, such as "kcal and thirst"
        parts.push_back( string_format( _( "%1$d %2$s" ), row.cost, resources ) );
    }
    if( !row.cooldown.empty() ) {
        //~ %s: cooldown of a mutation, such as "5 minutes"
        parts.push_back( string_format( _( "cooldown %s" ), row.cooldown ) );
    }
    if( row.starting_trait ) {
        parts.emplace_back( _( "starting trait" ) );
    }
    if( row.sprite_hidden ) {
        parts.emplace_back( _( "sprite hidden" ) );
    }
    return format_semantic_sequence( {
        string_join( parts, _( ", " ) ),
        row.insufficient ?
        //~ %s: resources a mutation needs, such as "kcal and thirst"
        string_format( _( "Unavailable: not enough %s" ), resources.empty() ? _( "resources" ) : resources ) :
        std::string()
    } );
}

std::string format_spell_row( const spell_row &row )
{
    const bool castable = row.unavailable_reason.empty();
    std::vector<std::string> parts = { remove_color_tags( row.name ) };
    std::string spell_class = remove_color_tags( row.spell_class );
    if( !spell_class.empty() ) {
        parts.push_back( std::move( spell_class ) );
    }
    if( castable ) {
        parts.emplace_back( pgettext( "spell", "castable" ) );
    }
    if( !row.cost.empty() ) {
        parts.push_back( row.cost );
        if( row.cost_impeded ) {
            parts.emplace_back( pgettext( "spell", "cost impeded" ) );
        }
    }
    if( !row.casting_time.empty() ) {
        parts.push_back( row.casting_time );
        if( row.time_impeded ) {
            parts.emplace_back( pgettext( "spell", "time impeded" ) );
        }
    }
    if( row.favorite ) {
        parts.emplace_back( pgettext( "spell", "favorite" ) );
    }
    return format_semantic_sequence( {
        string_join( parts, _( ", " ) ),
        castable ? std::string() : string_format( _( "Unavailable: %s" ), row.unavailable_reason )
    } );
}

std::string format_spell_count( const size_t count )
{
    if( count == 0 ) {
        return _( "No spells" );
    }
    return string_format( n_gettext( "%d spell", "%d spells", count ), static_cast<int>( count ) );
}

std::string format_martial_style_row( const std::string &name, const bool current,
                                      const std::string &description )
{
    std::vector<std::string> parts = { remove_color_tags( name ) };
    if( current ) {
        parts.emplace_back( pgettext( "martial arts style", "current" ) );
    }
    const std::vector<std::string> sentences =
        split_uilist_detail_sentences( remove_color_tags( description ) );
    return format_semantic_sequence( {
        string_join( parts, _( ", " ) ),
        sentences.empty() ? std::string() : sentences.front()
    } );
}

std::string format_morale_line( const std::string &label, const std::string &value )
{
    const std::string plain_label = trim( remove_color_tags( label ) );
    std::string plain_value = trim( remove_color_tags( value ) );
    if( plain_value == "-" ) {
        // The window shows a zero total as a dash.
        plain_value = "0";
    }
    if( plain_value.empty() ) {
        return format_semantic_sequence( { plain_label } );
    }
    if( plain_label.empty() ) {
        return format_semantic_sequence( { plain_value } );
    }
    if( plain_label.back() == ':' ) {
        return format_semantic_sequence( { plain_label + " " + plain_value } );
    }
    return format_semantic_sequence( { string_join( std::vector<std::string> {
            plain_label, plain_value
        }, _( ", " ) )
    } );
}

std::string format_health_bar( const std::string &bar )
{
    const std::string plain = remove_color_tags( bar );
    int tenths = 0;
    bool under_one = false;
    for( const char c : plain ) {
        if( c == '|' ) {
            tenths += 2;
        } else if( c == '\\' ) {
            tenths += 1;
        } else if( c == ':' ) {
            under_one = true;
        }
    }
    if( under_one && tenths == 0 ) {
        //~ Health bar with less than one tenth of its length left
        return _( "health under 1 of 10" );
    }
    //~ Health bar in tenths as the five-character bar shows it: %d: tenths left
    return string_format( _( "health %d of 10" ), std::min( tenths, 10 ) );
}

std::string format_mending_bar( const int filled )
{
    //~ Splinted limb in the medical menu; %d: filled fifths of the mending bar
    return string_format( _( "mending %d of 5" ), std::clamp( filled, 0, 5 ) );
}

std::string format_body_part_row( const std::string &name, const std::string &health,
                                  const bool missing, const bool has_details )
{
    std::vector<std::string> parts = { name };
    if( missing ) {
        parts.emplace_back( pgettext( "body part", "missing" ) );
    } else if( !health.empty() ) {
        parts.push_back( health );
    }
    if( has_details ) {
        parts.emplace_back( pgettext( "body status", "has details" ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_medical_health_row( const std::string &part, const std::string &health,
                                       const std::vector<std::string> &conditions )
{
    std::vector<std::string> parts = { remove_color_tags( part ) };
    if( !health.empty() ) {
        parts.push_back( remove_color_tags( health ) );
    }
    for( const std::string &condition : conditions ) {
        if( !condition.empty() ) {
            parts.push_back( remove_color_tags( condition ) );
        }
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_proficiency_window_row( const std::string &name, const bool known,
        const int progress )
{
    std::vector<std::string> parts = { remove_color_tags( name ) };
    if( known ) {
        parts.emplace_back( pgettext( "proficiency", "known" ) );
    } else {
        parts.emplace_back( pgettext( "proficiency", "learning" ) );
        parts.push_back( string_format( "%d%%", progress ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_armor_row( const armor_row &row )
{
    std::vector<std::string> parts = { remove_color_tags( row.name ) };
    if( !row.side.empty() ) {
        parts.push_back( row.side );
    }
    if( row.encumbrance >= 0 ) {
        //~ %d: encumbrance of a worn item on a body part
        parts.push_back( string_format( _( "encumbrance %d" ), row.encumbrance ) );
    }
    if( row.hidden ) {
        parts.emplace_back( pgettext( "armor sprite", "hidden" ) );
    }
    if( row.stacking_conflict ) {
        parts.emplace_back( _( "layering conflict" ) );
    }
    if( row.out_of_order ) {
        parts.emplace_back( _( "out of order" ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_armor_position( const std::string &inner, const std::string &outer )
{
    const std::string plain_inner = remove_color_tags( inner );
    const std::string plain_outer = remove_color_tags( outer );
    std::vector<std::string> parts;
    if( plain_inner.empty() && plain_outer.empty() ) {
        parts.emplace_back( _( "only item" ) );
    } else if( plain_inner.empty() ) {
        parts.emplace_back( _( "innermost" ) );
        //~ Armor position while moving: %s: the next item outwards
        parts.push_back( string_format( _( "above %s" ), plain_outer ) );
    } else if( plain_outer.empty() ) {
        parts.emplace_back( _( "outermost" ) );
        //~ Armor position while moving: %s: the next item inwards
        parts.push_back( string_format( _( "below %s" ), plain_inner ) );
    } else {
        parts.push_back( string_format( _( "above %s" ), plain_outer ) );
        parts.push_back( string_format( _( "below %s" ), plain_inner ) );
    }
    return format_semantic_sequence( { string_join( parts, _( ", " ) ) } );
}

std::string format_armor_moving( const std::string &name )
{
    //~ Sort armor: an item was picked up to change its layer; %s: item name
    return format_semantic_sequence( { string_format( _( "Moving %s" ), remove_color_tags( name ) ) } );
}

std::string format_armor_placed( const std::string &row )
{
    //~ Sort armor: the moved item was put down at its new layer
    return format_semantic_sequence( { pgettext( "armor", "Placed" ), row } );
}

std::string format_armor_sprite( const bool hidden, const std::string &row )
{
    return format_semantic_sequence( { hidden ? _( "Sprite hidden" ) : _( "Sprite shown" ), row } );
}

std::string format_armor_summary( const std::string &filter, const std::vector<armor_row> &rows,
                                  const int total, const int layering )
{
    std::vector<std::string> parts = { string_join( std::vector<std::string> {
            filter, format_list_count( rows.size() )
        }, _( ", " ) )
    };
    for( const armor_row &row : rows ) {
        parts.push_back( format_armor_row( row ) );
    }
    if( total >= 0 ) {
        //~ Sort armor summary: %1$d: total encumbrance of the body part, %2$d: its layering penalty
        parts.push_back( string_format( _( "Total encumbrance %1$d, layering %2$d" ), total,
                                        layering ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_dialogue_utterance( const std::string &speaker, const std::string &text )
{
    const std::string spoken_text = remove_color_tags( text );
    if( speaker.empty() ) {
        return spoken_text;
    }
    //~ Dialogue history element: %1$s: speaker name, %2$s: what they said or did
    return string_format( pgettext( "dialogue utterance", "%1$s: %2$s" ),
                          remove_color_tags( speaker ), spoken_text );
}

std::string format_dialogue_response( const dialogue_response_row &row )
{
    const std::string text = remove_color_tags( row.text );
    std::string hint;
    switch( row.hint ) {
        case dialogue_response_hint::none:
            break;
        case dialogue_response_hint::action:
            hint = _( "Action" );
            break;
        case dialogue_response_hint::helpless:
            hint = _( "Leaves you helpless" );
            break;
        case dialogue_response_hint::hostile:
            hint = _( "May provoke an attack" );
            break;
    }
    std::string unavailable;
    if( row.unavailable ) {
        unavailable = row.reason.empty() ? _( "Unavailable" ) :
                      string_format( _( "Unavailable: %s" ), remove_color_tags( row.reason ) );
    }
    return format_semantic_sequence( {
        //~ Dialogue response row: %1$s: hotkey, %2$s: response text
        row.key.empty() ? text : string_format( pgettext( "dialogue response", "%1$s, %2$s" ),
                                                format_spoken_key( row.key ), text ),
        hint, unavailable
    } );
}

std::string format_dialogue_prompt( const std::string &label, const size_t count )
{
    //~ Dialogue response prompt: %1$s: label such as "Your response", %2$d: number of responses
    return format_semantic_sequence( { string_format( n_gettext( "%1$s, %2$d option",
                                       "%1$s, %2$d options", count ), label, static_cast<int>( count ) )
                                     } );
}

void dialogue_speech_model::set_title( const std::string &new_title )
{
    title = new_title;
}

void dialogue_speech_model::set_prompt_label( const std::string &label )
{
    prompt_label = label;
}

void dialogue_speech_model::add_utterance( const std::string &text, const bool own )
{
    const std::string spoken = format_semantic_sequence( { remove_color_tags( text ) } );
    if( !spoken.empty() ) {
        history.emplace_back( spoken, own );
    }
}

void dialogue_speech_model::set_options( const std::vector<option> &new_options,
        const size_t responses, const int cursor )
{
    options = new_options;
    response_count = std::min( responses, options.size() );
    option_focus = options.empty() ? 0 : std::clamp( cursor, 0,
                   static_cast<int>( options.size() ) - 1 );
}

std::string dialogue_speech_model::prompt() const
{
    return format_dialogue_prompt( prompt_label, response_count );
}

std::string dialogue_speech_model::focus() const
{
    if( responses_open ) {
        return option_row( option_focus );
    }
    return history_focus < history.size() ? history[history_focus].first : prompt();
}

std::string dialogue_speech_model::option_row( const size_t index ) const
{
    return index < options.size() ? options[index].spoken : std::string();
}

std::string dialogue_speech_model::summary() const
{
    if( responses_open ) {
        return format_semantic_sequence( { title, prompt_label,
                                           format_list_position( option_focus + 1, options.size() )
                                         } );
    }
    if( history_focus < history.size() ) {
        return format_semantic_sequence( { title, _( "Conversation" ),
                                           format_list_position( history_focus + 1, history.size() )
                                         } );
    }
    return format_semantic_sequence( { title, prompt() } );
}

std::string dialogue_speech_model::begin_topic()
{
    std::vector<std::string> parts;
    for( size_t i = first_unspoken; i < history.size(); ++i ) {
        if( !history[i].second ) {
            parts.push_back( history[i].first );
        }
    }
    first_unspoken = history.size();
    history_focus = history.size();
    responses_open = false;
    parts.push_back( prompt() );
    return format_semantic_sequence( parts );
}

dialogue_speech_model::result dialogue_speech_model::handle( const std::string &action )
{
    result res;
    res.consumed = true;
    if( action == "ACCESSIBILITY_STATUS" ) {
        res.speech = summary();
        return res;
    }
    if( !responses_open ) {
        const int last = static_cast<int>( history.size() );
        const auto move_to = [&]( const int target ) {
            history_focus = std::clamp( target, 0, last );
            res.speech = focus();
        };
        const int current = static_cast<int>( history_focus );
        if( action == "UP" ) {
            move_to( current - 1 );
        } else if( action == "DOWN" ) {
            move_to( current + 1 );
        } else if( action == "PAGE_UP" ) {
            move_to( current - 10 );
        } else if( action == "PAGE_DOWN" ) {
            move_to( current + 10 );
        } else if( action == "HOME" ) {
            move_to( 0 );
        } else if( action == "END" ) {
            move_to( last );
        } else if( action == "CONFIRM" ) {
            if( history_focus < history.size() || options.empty() ) {
                res.speech = focus();
            } else {
                responses_open = true;
                res.speech = format_semantic_sequence( { prompt(), focus() } );
                if( option_focus < static_cast<int>( response_count ) ) {
                    res.response_cursor = option_focus;
                }
            }
        } else if( action == "ACCESSIBILITY_DETAILS" ) {
            if( history_focus < history.size() ) {
                res.document = history[history_focus].first;
            } else {
                res.speech = prompt();
            }
        } else {
            res.consumed = false;
            res.action = action;
        }
        return res;
    }

    const int count = static_cast<int>( options.size() );
    const auto move_to = [&]( const int target, const int direction, const bool wrap ) {
        if( count == 0 ) {
            return;
        }
        const int previous = option_focus;
        option_focus = wrap ? ( target % count + count ) % count : std::clamp( target, 0, count - 1 );
        res.speech = format_semantic_sequence( {
            wrap ? format_list_wrap( previous, option_focus, direction ) : std::string(),
            focus()
        } );
        if( option_focus < static_cast<int>( response_count ) ) {
            res.response_cursor = option_focus;
        }
    };
    if( action == "UP" ) {
        move_to( option_focus - 1, -1, true );
    } else if( action == "DOWN" ) {
        move_to( option_focus + 1, 1, true );
    } else if( action == "PAGE_UP" ) {
        move_to( option_focus - 5, -1, false );
    } else if( action == "PAGE_DOWN" ) {
        move_to( option_focus + 5, 1, false );
    } else if( action == "HOME" ) {
        move_to( 0, -1, false );
    } else if( action == "END" ) {
        move_to( count - 1, 1, false );
    } else if( action == "QUIT" ) {
        responses_open = false;
        history_focus = history.size();
        res.speech = prompt();
    } else if( action == "ACCESSIBILITY_DETAILS" ) {
        res.document = focus();
    } else if( action == "CONFIRM" && option_focus < count ) {
        res.consumed = false;
        if( option_focus < static_cast<int>( response_count ) ) {
            res.action = action;
            res.response_cursor = option_focus;
        } else {
            res.action = options[option_focus].action;
        }
    } else {
        res.consumed = false;
        res.action = action;
    }
    return res;
}

std::string format_activity_start( const std::string &label, const std::string &duration,
                                   const bool resuming )
{
    return format_semantic_sequence( {
        string_format( resuming ? _( "Resumed: %s" ) : _( "Started: %s" ), remove_color_tags( label ) ),
        duration.empty() ? std::string() : string_format( _( "About %s" ), duration )
    } );
}

std::string format_activity_result( const std::string &label, const bool finished,
                                    const bool canceled, const std::string &reason,
                                    const std::vector<std::string> &messages )
{
    std::vector<std::string> parts;
    for( const std::string &message : messages ) {
        parts.push_back( remove_color_tags( message ) );
    }
    if( !reason.empty() ) {
        parts.push_back( remove_color_tags( reason ) );
    }
    if( parts.empty() || canceled ) {
        parts.insert( parts.begin(), string_format( finished && !canceled ?
                      _( "Finished: %s" ) : _( "Stopped: %s" ), remove_color_tags( label ) ) );
    }
    return format_semantic_sequence( parts );
}

std::string format_activity_progress( const std::string &label, const std::string &progress,
                                      const std::string &remaining )
{
    const std::string semantic_label = remove_color_tags( label );
    const std::string semantic_progress = remove_color_tags( progress );
    return format_semantic_sequence( {
        semantic_label == normalize_status_announcement( semantic_progress, true ) ? semantic_progress :
        format_semantic_sequence( { semantic_label, semantic_progress } ),
        remaining.empty() ? std::string() : string_format( _( "About %s remaining" ), remaining )
    } );
}

} // namespace accessibility
