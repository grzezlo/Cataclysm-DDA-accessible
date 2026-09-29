#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "calendar.h"
#include "coords_fwd.h"
#include "fake_messages.h"
#include "messages.h"

class Creature;
class JsonObject;
class JsonOut;
namespace debugmode
{
enum debug_filter : int;
}  // namespace debugmode
struct game_message_params;

namespace catacurses
{
class window;
}  // namespace catacurses

/**
 * For unit testing we just store all messages so they can be dumped in the
 * event of a test failure.
 */

static std::vector<std::pair<std::string, std::string>> messages;
static std::function<void( const std::string & )> message_observer;

void set_test_message_observer( std::function<void( const std::string & )> observer )
{
    message_observer = std::move( observer );
}

static void record_message( std::string m, const bool notify )
{
    if( !m.empty() ) {
        if( notify && message_observer ) {
            message_observer( m );
        }
        messages.emplace_back( to_string_time_of_day( calendar::turn ), std::move( m ) );
    }
}

std::vector<std::pair<std::string, std::string>> Messages::recent_messages( size_t )
{
    return messages;
}
bool Messages::has_debug_filter( debugmode::debug_filter )
{
    return true;
}
void Messages::add_msg( std::string m )
{
    record_message( std::move( m ), true );
}
void Messages::add_msg( const game_message_params &params, std::string m )
{
    record_message( std::move( m ), params.type != m_debug );
}
void Messages::clear_messages()
{
    messages.clear();
}
void Messages::deactivate() {}
size_t Messages::size()
{
    return messages.size();
}
bool Messages::has_undisplayed_messages()
{
    return false;
}
void Messages::display_messages() {}
void Messages::display_messages( const catacurses::window &, int, int, int, int ) {}
void Messages::serialize( JsonOut & ) {}
void Messages::deserialize( const JsonObject & ) {}
void add_msg( std::string m )
{
    Messages::add_msg( std::move( m ) );
}
void add_msg( const game_message_params &params, std::string m )
{
    Messages::add_msg( params, std::move( m ) );
}
void add_msg_if_player_sees( const tripoint_bub_ms &, std::string m )
{
    Messages::add_msg( std::move( m ) );
}
void add_msg_if_player_sees( const Creature &, std::string m )
{
    Messages::add_msg( std::move( m ) );
}
void add_msg_if_player_sees( const tripoint_bub_ms &, const game_message_params &params,
                             std::string m )
{
    Messages::add_msg( params, std::move( m ) );
}
void add_msg_if_player_sees( const Creature &, const game_message_params &params, std::string m )
{
    Messages::add_msg( params, std::move( m ) );
}
