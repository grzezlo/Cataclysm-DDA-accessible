#include "accessibility.h"

#include <memory>
#include <set>
#include <utility>

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

void enable_game_messages( const bool enabled )
{
    if( game_messages_enabled() != enabled ) {
        previous_threats().clear();
        threats_initialized() = false;
        previous_movement_direction().clear();
    }
    game_messages_enabled() = enabled;
}

void set_backend_for_testing( std::unique_ptr<backend> replacement )
{
    active_backend() = replacement ? std::move( replacement ) : std::make_unique<null_backend>();
    last_message().clear();
    previous_threats().clear();
    threats_initialized() = false;
    previous_movement_direction().clear();
}

void reset_backend_after_testing()
{
    active_backend() = make_platform_backend();
    last_message().clear();
    game_messages_enabled() = false;
    previous_threats().clear();
    threats_initialized() = false;
    previous_movement_direction().clear();
}

} // namespace accessibility
