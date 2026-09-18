#include "accessibility.h"

#include <memory>
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

void set_backend_for_testing( std::unique_ptr<backend> replacement )
{
    active_backend() = replacement ? std::move( replacement ) : std::make_unique<null_backend>();
    last_message().clear();
}

void reset_backend_after_testing()
{
    active_backend() = make_platform_backend();
    last_message().clear();
}

} // namespace accessibility
