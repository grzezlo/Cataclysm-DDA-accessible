#include "accessibility.h"

#include <chrono>
#include <memory>
#include <set>
#include <utility>

#include "accessibility_format.h"

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

// Automatic mode keeps probing until a screen reader is found, but never more
// often than this, because each probe is a round trip to the screen reader.
constexpr std::chrono::seconds automatic_probe_interval( 5 );

struct service_state {
    std::unique_ptr<backend> active_backend;
    screen_reader_mode mode = screen_reader_mode::automatic;
    bool detected = false;
    std::optional<std::chrono::steady_clock::time_point> last_probe;
    std::string last_message;
    bool game_messages_enabled = false;
    std::set<std::string> previous_threats;
    bool threats_initialized = false;
    std::string previous_movement_direction;
    condition_tracker conditions;
};

service_state &state()
{
    static service_state instance;
    if( !instance.active_backend ) {
        instance.active_backend = make_platform_backend();
        if( !instance.active_backend ) {
            instance.active_backend = std::make_unique<null_backend>();
        }
    }
    return instance;
}

void reset_detection()
{
    state().detected = false;
    state().last_probe.reset();
}

} // namespace

void set_screen_reader_mode( const screen_reader_mode mode )
{
    service_state &current = state();
    if( current.mode != mode ) {
        current.mode = mode;
        reset_detection();
    }
    // Detect at startup so the accessible interface is ready on the first screen.
    is_active();
}

screen_reader_mode get_screen_reader_mode()
{
    return state().mode;
}

bool is_active()
{
    service_state &current = state();
    switch( current.mode ) {
        case screen_reader_mode::on:
            return true;
        case screen_reader_mode::off:
            return false;
        case screen_reader_mode::automatic:
            break;
    }
    if( current.detected ) {
        return true;
    }
    const auto now = std::chrono::steady_clock::now();
    if( current.last_probe && now - *current.last_probe < automatic_probe_interval ) {
        return false;
    }
    current.last_probe = now;
    current.detected = current.active_backend->is_available();
    return current.detected;
}

bool announce( const std::string &message )
{
    if( message.empty() || !is_active() ) {
        return false;
    }

    state().last_message = message;
    return state().active_backend->announce( message );
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
    if( !state().game_messages_enabled ) {
        return false;
    }
    return announce( message );
}

bool announce_movement( const std::string &direction, const std::string &tile )
{
    service_state &current = state();
    if( !current.game_messages_enabled ) {
        return false;
    }

    std::string message = tile;
    if( direction != current.previous_movement_direction ) {
        if( !message.empty() && !direction.empty() ) {
            message += ". ";
        }
        message += direction;
        current.previous_movement_direction = direction;
    }
    return announce( message );
}

std::optional<std::string> evaluate_threat_transition(
    const std::vector<std::string> &threat_ids, const std::function<std::string()> &describe )
{
    service_state &current = state();
    if( !current.game_messages_enabled ) {
        return std::nullopt;
    }

    std::set<std::string> current_threats( threat_ids.begin(), threat_ids.end() );
    if( !current.threats_initialized ) {
        current.threats_initialized = true;
        current.previous_threats = std::move( current_threats );
        return current.previous_threats.empty() ? std::nullopt :
               std::optional<std::string>( describe() );
    }
    if( current_threats == current.previous_threats ) {
        return std::nullopt;
    }

    current.previous_threats = std::move( current_threats );
    return describe();
}

bool announce_threats( const std::vector<std::string> &threat_ids, const std::string &message )
{
    const std::optional<std::string> transition = evaluate_threat_transition( threat_ids,
    [&message]() {
        return message;
    } );
    return transition && announce( *transition );
}

bool announce_semantic_sequence( const std::vector<std::string> &parts )
{
    return announce( format_semantic_sequence( parts ) );
}

bool announce_nearby_list_focus( const nearby_list_entry &entry, const std::string &context )
{
    return announce_semantic_sequence( { context, format_nearby_list_entry( entry ) } );
}

bool repeat_last_announcement()
{
    const std::string &message = state().last_message;
    if( message.empty() || !is_active() ) {
        return false;
    }

    return state().active_backend->announce( message );
}

std::string last_announcement()
{
    return state().last_message;
}

void set_last_announcement( const std::string &message )
{
    if( !message.empty() ) {
        state().last_message = message;
    }
}

void enable_game_messages( const bool enabled )
{
    service_state &current = state();
    if( current.game_messages_enabled != enabled ) {
        current.previous_threats.clear();
        current.threats_initialized = false;
        current.previous_movement_direction.clear();
    }
    current.game_messages_enabled = enabled;
}

bool game_messages_enabled()
{
    return state().game_messages_enabled;
}

std::vector<std::string> evaluate_condition_transitions( const condition_tracker &current )
{
    return evaluate_condition_transitions( state().conditions, current );
}

void reset_condition_tracker()
{
    state().conditions = condition_tracker{};
}

bool check_and_announce_condition_transitions( const condition_tracker &current )
{
    return announce_semantic_sequence( evaluate_condition_transitions( current ) );
}

void reset_game_state()
{
    service_state &current = state();
    current.previous_threats.clear();
    current.threats_initialized = false;
    current.previous_movement_direction.clear();
    reset_condition_tracker();
}

void set_backend_for_testing( std::unique_ptr<backend> replacement )
{
    service_state &current = state();
    current.active_backend = replacement ? std::move( replacement ) :
                             std::make_unique<null_backend>();
    current.mode = screen_reader_mode::automatic;
    reset_detection();
    current.last_message.clear();
    reset_game_state();
}

void reset_backend_after_testing()
{
    service_state &current = state();
    current.active_backend = make_platform_backend();
    if( !current.active_backend ) {
        current.active_backend = std::make_unique<null_backend>();
    }
    current.mode = screen_reader_mode::automatic;
    reset_detection();
    current.last_message.clear();
    current.game_messages_enabled = false;
    reset_game_state();
}

} // namespace accessibility
