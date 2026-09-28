#include "accessibility.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <set>
#include <utility>

#include "accessibility_format.h"
#include "debug.h"

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
    // Incremented for every key press; speech_generation records the press
    // after which something was last spoken.
    uint64_t input_generation = 0;
    uint64_t speech_generation = 0;
    struct queued_speech {
        std::string text;
        gameplay_speech kind;
    };
    std::vector<queued_speech> gameplay_queue;
    bool movement_suppressed = false;
    std::string last_game_message;
    uint64_t last_game_message_generation = 0;
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

/**
 * Send speech to the backend.  Only the first focus speech after a key press
 * may replace unfinished speech; everything else queues behind it.
 */
/** Set CATA_ACCESSIBILITY_TRACE to log every speech request to debug.log. */
bool trace_enabled()
{
    static const bool enabled = std::getenv( "CATA_ACCESSIBILITY_TRACE" ) != nullptr;
    return enabled;
}

bool speak( const std::string &message, const bool may_interrupt )
{
    service_state &current = state();
    const bool interrupt = may_interrupt && current.speech_generation != current.input_generation;
    if( interrupt ) {
        current.active_backend->cancel();
    }
    current.speech_generation = current.input_generation;
    const bool spoken = current.active_backend->announce( message );
    if( trace_enabled() ) {
        DebugLog( D_INFO, DC_ALL ) << "accessibility speech" << ( interrupt ? " (interrupting)" : "" )
                                   << ( spoken ? "" : " (not delivered)" ) << ": " << message;
    }
    return spoken;
}

} // namespace

void note_player_input()
{
    ++state().input_generation;
}

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

    // Keep chronological order: pending turn speech (such as the alert that
    // triggered a prompt) is spoken first and is not cut off by this focus.
    flush_gameplay_announcements();
    state().last_message = message;
    return speak( message, true );
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

void queue_gameplay_announcement( const std::string &message, const gameplay_speech kind )
{
    service_state &current = state();
    if( message.empty() || !current.game_messages_enabled || !is_active() ) {
        return;
    }
    current.gameplay_queue.push_back( { message, kind } );
    if( trace_enabled() ) {
        DebugLog( D_INFO, DC_ALL ) << "accessibility queued: " << message;
    }
}

bool flush_gameplay_announcements()
{
    service_state &current = state();
    if( current.gameplay_queue.empty() ) {
        return false;
    }
    std::vector<std::string> all_parts;
    std::vector<std::string> meaningful_parts;
    for( const service_state::queued_speech &entry : current.gameplay_queue ) {
        all_parts.push_back( entry.text );
        if( entry.kind == gameplay_speech::meaningful ) {
            meaningful_parts.push_back( entry.text );
        }
    }
    current.gameplay_queue.clear();
    if( !is_active() ) {
        return false;
    }
    if( !meaningful_parts.empty() ) {
        current.last_message = format_semantic_sequence( meaningful_parts );
    }
    return speak( format_semantic_sequence( all_parts ), false );
}

bool announce_game_message( const std::string &message )
{
    service_state &current = state();
    if( message.empty() || !current.game_messages_enabled ) {
        return false;
    }
    // Repeat a message only in response to a new key press, such as another
    // attack.  Activities like smashing repeat it every turn without input.
    if( message == current.last_game_message &&
        current.input_generation == current.last_game_message_generation ) {
        return false;
    }
    current.last_game_message = message;
    current.last_game_message_generation = current.input_generation;
    queue_gameplay_announcement( message, gameplay_speech::meaningful );
    return is_active();
}

void note_suppressed_movement()
{
    state().movement_suppressed = true;
}

bool take_suppressed_movement()
{
    const bool suppressed = state().movement_suppressed;
    state().movement_suppressed = false;
    return suppressed;
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
    queue_gameplay_announcement( message, gameplay_speech::routine );
    return !message.empty() && is_active();
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

    return speak( message, true );
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
        current.gameplay_queue.clear();
        current.last_game_message.clear();
        current.movement_suppressed = false;
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
    current.gameplay_queue.clear();
    current.last_game_message.clear();
    current.movement_suppressed = false;
    reset_condition_tracker();
}

void set_backend_for_testing( std::unique_ptr<backend> replacement )
{
    service_state &current = state();
    current.active_backend = replacement ? std::move( replacement ) :
                             std::make_unique<null_backend>();
    current.mode = screen_reader_mode::automatic;
    reset_detection();
    current.input_generation = 0;
    current.speech_generation = 0;
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
    current.input_generation = 0;
    current.speech_generation = 0;
    current.last_message.clear();
    current.game_messages_enabled = false;
    reset_game_state();
}

} // namespace accessibility
