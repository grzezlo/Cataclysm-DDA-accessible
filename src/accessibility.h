#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_H
#define CATA_SRC_ACCESSIBILITY_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Screen-reader-independent accessibility service.  Semantic formatters live in
// accessibility_format.h so that adding a formatter does not rebuild every caller.
namespace accessibility
{

class backend
{
    public:
        virtual ~backend() = default;

        virtual bool is_available() const = 0;
        virtual bool announce( const std::string &message ) = 0;
        /** Stop speech that is still being spoken or queued. */
        virtual void cancel() {}
};

/** Player-selected screen reader mode; see the SCREEN_READER_SPEECH option. */
enum class screen_reader_mode {
    automatic,
    on,
    off
};

/** Apply the configured mode.  Automatic mode probes the backend immediately. */
void set_screen_reader_mode( screen_reader_mode mode );
screen_reader_mode get_screen_reader_mode();
/**
 * Whether the accessible interface is active.  Automatic mode activates once a
 * screen reader is detected and then stays active for the session, so the
 * interface does not change when the screen reader restarts or pauses.
 */
bool is_active();

/**
 * Record that the player pressed a key.  The first focus announcement after a
 * key press replaces unfinished earlier speech; later announcements caused by
 * the same key, and all gameplay speech, are queued behind it.
 */
void note_player_input();

/** Speak a focus or status announcement immediately; it becomes repeat-last. */
bool announce( const std::string &message );
bool announce_focus( const std::string &label, const std::string &context = {} );
bool announce_status( const std::string &message );

enum class gameplay_speech {
    /** Messages and alerts; they become repeat-last when spoken. */
    meaningful,
    /** Movement tile names and similar chatter; never repeat-last. */
    routine
};
/**
 * Queue gameplay speech for the current turn.  flush_gameplay_announcements()
 * speaks the queue as one utterance that never interrupts earlier speech.
 */
void queue_gameplay_announcement( const std::string &message, gameplay_speech kind );
/** Speak and clear the queued gameplay speech.  Returns whether anything was spoken. */
bool flush_gameplay_announcements();
/** Queue a game log message for this turn's utterance. */
bool announce_game_message( const std::string &message );
/** Queue the destination tile, adding the direction only when it changes. */
bool announce_movement( const std::string &direction, const std::string &tile );
/** Remember that per-step movement speech was skipped during auto-move. */
void note_suppressed_movement();
/** Return and clear whether movement speech was skipped since the last call. */
bool take_suppressed_movement();
/** Remember that announced auto-travel started; cleared with the game state. */
void note_travel_started();
/** Return and clear whether announced auto-travel is in progress. */
bool take_travel_in_progress();
bool travel_in_progress();
/**
 * Update visible-threat state and return an announcement only when the set of
 * threats changed.  `describe` is called only when an announcement is needed.
 */
std::optional<std::string> evaluate_threat_transition(
    const std::vector<std::string> &threat_ids, const std::function<std::string()> &describe );
bool announce_threats( const std::vector<std::string> &threat_ids, const std::string &message );
bool announce_semantic_sequence( const std::vector<std::string> &parts );
struct nearby_list_entry;
bool announce_nearby_list_focus( const nearby_list_entry &entry,
                                 const std::string &context = {} );
/** Speak the most recent announcement again via the active backend. */
bool repeat_last_announcement();
/** Return the most recent announcement text (spoken or quietly set). */
std::string last_announcement();
/** Update the stored last announcement without triggering speech.
 *  Use this for progress updates that should only be spoken on demand
 *  (e.g. via Alt+R during blocking operations). Empty strings are ignored. */
void set_last_announcement( const std::string &message );

struct condition_tracker;
/** Evaluate transitions against the process-wide gameplay tracker. */
std::vector<std::string> evaluate_condition_transitions( const condition_tracker &current );
/** Check against the global condition tracker and announce any urgent transitions. */
bool check_and_announce_condition_transitions( const condition_tracker &current );
/** Reset the global condition tracker (e.g. at game start or between tests). */
void reset_condition_tracker();
/** Reset transient accessibility state when starting or loading a game. */
void reset_game_state();

void enable_game_messages( bool enabled );
bool game_messages_enabled();

void set_backend_for_testing( std::unique_ptr<backend> replacement );
void reset_backend_after_testing();

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_H
