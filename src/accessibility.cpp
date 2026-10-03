#include "accessibility.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <utility>

#include "accessibility_format.h"
#include "debug.h"

namespace accessibility
{

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
    // Sorted and unique.
    std::vector<threat_identity> previous_threats;
    bool threats_initialized = false;
    std::string previous_movement_direction;
    condition_tracker conditions;
    // The raw inputs of `conditions`; empty when they are unknown.
    std::vector<int64_t> condition_inputs;
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
    bool travel_in_progress = false;
    std::string last_game_message;
    uint64_t last_game_message_generation = 0;
    int game_message_suppressions = 0;
    std::vector<std::string> *game_message_capture = nullptr;
    std::vector<progress_reporter *> progress_sources;
    int progress_interval = 5;
    std::optional<std::chrono::steady_clock::time_point> last_speech;
    std::string last_spoken_message;
    std::function<std::chrono::steady_clock::time_point()> clock;
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

std::chrono::steady_clock::time_point now()
{
    return state().clock ? state().clock() : std::chrono::steady_clock::now();
}

/** Set CATA_ACCESSIBILITY_TRACE to log every speech request to debug.log. */
bool trace_enabled()
{
    static const bool enabled = std::getenv( "CATA_ACCESSIBILITY_TRACE" ) != nullptr;
    return enabled;
}

/**
 * Send speech to the backend.  Only the first focus speech after a key press
 * may replace unfinished speech; everything else queues behind it.
 */
bool speak( const std::string &message, const bool may_interrupt )
{
    service_state &current = state();
    const bool interrupt = may_interrupt && current.speech_generation != current.input_generation;
    if( interrupt ) {
        current.active_backend->cancel();
    }
    current.speech_generation = current.input_generation;
    const bool spoken = current.active_backend->announce( message );
    if( spoken ) {
        current.last_speech = now();
        current.last_spoken_message = message;
    }
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
    const std::chrono::steady_clock::time_point probe_time = now();
    if( current.last_probe && probe_time - *current.last_probe < automatic_probe_interval ) {
        return false;
    }
    current.last_probe = probe_time;
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
    // A prompt owns speech until its running operation updates progress again.
    for( progress_reporter *source : state().progress_sources ) {
        source->message.clear();
    }
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
    if( !is_active() ) {
        current.gameplay_queue.clear();
        return false;
    }
    // Format the queued texts in place before clearing the queue.
    std::vector<std::string_view> all_parts;
    std::vector<std::string_view> meaningful_parts;
    all_parts.reserve( current.gameplay_queue.size() );
    for( const service_state::queued_speech &entry : current.gameplay_queue ) {
        if( entry.kind == gameplay_speech::meaningful ) {
            meaningful_parts.push_back( entry.text );
        }
        all_parts.push_back( entry.text );
    }
    if( !meaningful_parts.empty() ) {
        current.last_message = format_semantic_sequence_of_views( meaningful_parts );
    }
    const std::string speech = format_semantic_sequence_of_views( all_parts );
    current.gameplay_queue.clear();
    return speak( speech, false );
}

bool announce_game_message( const std::string &message )
{
    service_state &current = state();
    if( message.empty() || !current.game_messages_enabled ||
        current.game_message_suppressions > 0 ) {
        return false;
    }
    if( current.game_message_capture ) {
        auto &messages = *current.game_message_capture;
        if( std::find( messages.begin(), messages.end(), message ) == messages.end() ) {
            messages.push_back( message );
        }
        return is_active();
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

game_message_speech_suppression::game_message_speech_suppression()
{
    ++state().game_message_suppressions;
}

// The constructor initializes the service state before this destructor runs.
// NOLINTNEXTLINE(bugprone-exception-escape)
game_message_speech_suppression::~game_message_speech_suppression()
{
    --state().game_message_suppressions;
}

game_message_speech_capture::game_message_speech_capture( const bool enabled ) : enabled( enabled )
{
    if( enabled ) {
        previous = state().game_message_capture;
        state().game_message_capture = &collected;
    }
}

// The constructor initializes the service state before this destructor runs.
// NOLINTNEXTLINE(bugprone-exception-escape)
game_message_speech_capture::~game_message_speech_capture()
{
    if( enabled ) {
        state().game_message_capture = previous;
    }
}

progress_reporter::progress_reporter() : started( now() )
{
    state().progress_sources.push_back( this );
}

// The constructor initializes the service state before this destructor runs.
// NOLINTNEXTLINE(bugprone-exception-escape)
progress_reporter::~progress_reporter()
{
    auto &sources = state().progress_sources;
    sources.erase( std::remove( sources.begin(), sources.end(), this ), sources.end() );
}

void progress_reporter::update( const std::string &text )
{
    if( text.empty() || !is_active() ) {
        return;
    }
    message = text;
    if( message == state().last_spoken_message ) {
        last_spoken = message;
    }
    auto &sources = state().progress_sources;
    sources.erase( std::remove( sources.begin(), sources.end(), this ), sources.end() );
    sources.push_back( this );
    set_last_announcement( text );
    poll_progress();
}

bool poll_progress()
{
    service_state &current = state();
    if( current.progress_interval <= 0 || current.progress_sources.empty() || !is_active() ) {
        return false;
    }
    progress_reporter &source = *current.progress_sources.back();
    const auto since = current.last_speech ? std::max( source.started, *current.last_speech ) :
                       source.started;
    if( source.message.empty() || source.message == source.last_spoken ||
        now() - since < std::chrono::seconds( current.progress_interval ) ) {
        return false;
    }
    // Pending warnings take precedence over automatic progress.
    if( flush_gameplay_announcements() ) {
        return false;
    }
    current.last_message = source.message;
    if( speak( source.message, false ) ) {
        source.last_spoken = source.message;
        return true;
    }
    return false;
}

void set_progress_announcement_interval( const int seconds )
{
    state().progress_interval = std::max( 0, seconds );
}

void set_clock_for_testing( std::function<std::chrono::steady_clock::time_point()> clock )
{
    state().clock = std::move( clock );
    state().last_speech.reset();
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

void note_travel_started()
{
    state().travel_in_progress = true;
}

bool take_travel_in_progress()
{
    const bool in_progress = state().travel_in_progress;
    state().travel_in_progress = false;
    return in_progress;
}

bool travel_in_progress()
{
    return state().travel_in_progress;
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

std::optional<std::string> evaluate_threat_transition( std::vector<threat_identity> threats,
        const std::function<std::string()> &describe )
{
    service_state &current = state();
    if( !current.game_messages_enabled ) {
        return std::nullopt;
    }

    std::sort( threats.begin(), threats.end() );
    threats.erase( std::unique( threats.begin(), threats.end() ), threats.end() );
    if( !current.threats_initialized ) {
        current.threats_initialized = true;
        current.previous_threats = std::move( threats );
        return current.previous_threats.empty() ? std::nullopt :
               std::optional<std::string>( describe() );
    }
    if( threats == current.previous_threats ) {
        return std::nullopt;
    }

    current.previous_threats = std::move( threats );
    return describe();
}

bool announce_threats( std::vector<threat_identity> threats, const std::string &message )
{
    const std::optional<std::string> transition = evaluate_threat_transition( std::move( threats ),
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

    for( progress_reporter *source : state().progress_sources ) {
        if( source->message == message ) {
            source->last_spoken = message;
        }
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

namespace
{

/** Forget the speech state of the current turn and of movement and threats. */
void reset_turn_speech( service_state &current )
{
    current.previous_threats.clear();
    current.threats_initialized = false;
    current.previous_movement_direction.clear();
    current.gameplay_queue.clear();
    current.last_game_message.clear();
    current.movement_suppressed = false;
    current.travel_in_progress = false;
}

/** Use `replacement`, or the null backend, with fresh speech state. */
void install_backend( std::unique_ptr<backend> replacement )
{
    service_state &current = state();
    current.active_backend = replacement ? std::move( replacement ) :
                             std::make_unique<null_backend>();
    current.mode = screen_reader_mode::automatic;
    reset_detection();
    current.input_generation = 0;
    current.speech_generation = 0;
    current.last_message.clear();
    current.last_speech.reset();
    current.last_spoken_message.clear();
    current.progress_interval = 5;
    reset_game_state();
}

} // namespace

void enable_game_messages( const bool enabled )
{
    service_state &current = state();
    if( current.game_messages_enabled != enabled ) {
        reset_turn_speech( current );
    }
    current.game_messages_enabled = enabled;
}

bool game_messages_enabled()
{
    return state().game_messages_enabled;
}

std::vector<std::string> evaluate_condition_transitions( const condition_tracker &current )
{
    state().condition_inputs.clear();
    return evaluate_condition_transitions( state().conditions, current );
}

std::vector<std::string> evaluate_condition_transitions( const std::vector<int64_t> &inputs,
        const std::function<condition_tracker()> &make_current )
{
    service_state &current = state();
    if( current.conditions.initialized && !inputs.empty() && inputs == current.condition_inputs ) {
        // The same inputs give the same state, which has no transitions.
        return {};
    }
    std::vector<std::string> alerts = evaluate_condition_transitions( current.conditions,
                                      make_current() );
    current.condition_inputs = inputs;
    return alerts;
}

void reset_condition_tracker()
{
    state().conditions = condition_tracker{};
    state().condition_inputs.clear();
}

bool check_and_announce_condition_transitions( const condition_tracker &current )
{
    return announce_semantic_sequence( evaluate_condition_transitions( current ) );
}

void reset_game_state()
{
    service_state &current = state();
    reset_turn_speech( current );
    for( progress_reporter *source : current.progress_sources ) {
        source->message.clear();
        source->last_spoken.clear();
    }
    reset_condition_tracker();
}

void set_backend_for_testing( std::unique_ptr<backend> replacement )
{
    install_backend( std::move( replacement ) );
}

void reset_backend_after_testing()
{
    service_state &current = state();
    current.game_messages_enabled = false;
    current.clock = {};
    install_backend( make_platform_backend() );
}

} // namespace accessibility
