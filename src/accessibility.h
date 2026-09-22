#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_H
#define CATA_SRC_ACCESSIBILITY_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace accessibility
{

class backend
{
    public:
        virtual ~backend() = default;

        virtual bool is_available() const = 0;
        virtual bool announce( const std::string &message ) = 0;
};

struct semantic_list_entry {
    int distance;
    int z;
    int y;
    int x;
    int kind;
    std::string label;
    std::string context;
};

struct connected_semantic_list_group {
    semantic_list_entry nearest;
    size_t count;
};

/** Renderer-independent semantic row for the nearby item and creature lists. */
struct nearby_list_entry {
    std::string name;
    int count = 0;
    int position_index = 0;
    int position_count = 0;
    std::string location;
    std::vector<std::string> details;
    int result_index = 0;
    int result_count = 0;
};

void sort_semantic_list_entries( std::vector<semantic_list_entry> &entries );
std::vector<connected_semantic_list_group> group_connected_semantic_list_entries(
    std::vector<semantic_list_entry> entries );

bool is_available();
bool announce( const std::string &message );
bool announce_focus( const std::string &label, const std::string &context = {} );
bool announce_status( const std::string &message );
bool announce_game_message( const std::string &message );
bool announce_movement( const std::string &direction, const std::string &tile );
/** Update visible-threat state and return an announcement only when it changed. */
std::optional<std::string> evaluate_threat_transition(
    const std::vector<std::string> &threat_ids, const std::string &message );
bool announce_threats( const std::vector<std::string> &threat_ids, const std::string &message );
std::string format_semantic_sequence( const std::vector<std::string> &parts );
bool announce_semantic_sequence( const std::vector<std::string> &parts );
/** Describe a main-menu world using its save count rather than visual parentheses. */
std::string format_main_menu_world( const std::string &name, size_t save_count );
std::string format_main_menu_focus( const std::string &menu_name, const std::string &tab,
                                    const std::string &focused, const std::string &title = {},
                                    const std::string &version = {}, const std::string &tip = {} );
struct semantic_document_line {
    std::string text;
    size_t source_index;
};
/** Keep meaningful source lines in reading order without splitting long lines. */
std::vector<semantic_document_line> select_document_lines(
    const std::vector<std::string> &source_lines );
int move_document_cursor( int current, int line_count, int delta );
/** Retain independent reading positions when switching between documents. */
class document_cursor_memory
{
    public:
        explicit document_cursor_memory( size_t document_count );
        int position( size_t document ) const;
        int move( size_t document, int line_count, int delta );
        void reset();

    private:
        std::vector<int> positions_;
};
std::string format_creature_attitude( const std::string &attitude );
std::string format_creature_health( int current, int maximum );
std::string format_nearby_list_entry( const nearby_list_entry &entry );
std::string format_nearby_list_context( const std::string &mode, int result_count,
                                        const std::string &change = {} );
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
/** Strip progress counters and percentages for deduplication. */
std::string normalize_status_announcement( const std::string &message, bool progress );

/** Discrete condition state tracked between turns to detect urgent transitions. */
struct condition_tracker {
    std::string hunger_tier;
    std::string thirst_tier;
    std::string sleepiness_tier;
    std::string pain_tier;
    std::string weariness_tier;
    bool stamina_critical = false;
    std::vector<std::string> bleeding_limbs;
    std::vector<std::string> broken_limbs;
    bool initialized = false;
};

enum class safe_mode_state {
    off,
    on,
    stopped
};

/** Semantic safe-mode state, including information otherwise conveyed only by color. */
std::string format_safe_mode_status( safe_mode_state state, bool auto_safe_mode,
                                     int turns_until_auto_safe );

struct status_snapshot {
    std::string health;
    std::string pain;
    bool bleeding = false;
    bool broken_limb = false;
    int stamina_percent = 0;
    int focus = 0;
    std::string weariness;
    std::string hunger;
    std::string thirst;
    std::string sleepiness;
    std::string body_temperature;
    std::string environment_temperature;
    size_t active_effect_count = 0;
    std::string wielded_weapon;
    std::string movement_mode;
    int speed = 0;
    std::string safe_mode;
    std::string time;
    std::string date;
    std::string weather;
};

struct status_summary_entry {
    std::string label;
    std::string value;
};

/** Format the seven concise status-review categories from a fixed semantic snapshot. */
std::vector<status_summary_entry> format_status_summary( const status_snapshot &snapshot );

/** Evaluate condition transitions between previous and current states.
 *  Returns transition announcements (if any) and updates `tracker`. */
std::vector<std::string> evaluate_condition_transitions( condition_tracker &tracker,
        const condition_tracker &current );
/** Evaluate transitions against the process-wide gameplay tracker. */
std::vector<std::string> evaluate_condition_transitions( const condition_tracker &current );
/** Announce urgent condition transitions via the active backend if any occurred. */
bool announce_condition_transitions( condition_tracker &tracker,
                                     const condition_tracker &current );
/** Reset the global condition tracker (e.g. at game start or between tests). */
void reset_condition_tracker();
/** Check against the global condition tracker and announce any urgent transitions. */
bool check_and_announce_condition_transitions( const condition_tracker &current );
/** Reset transient accessibility state when starting or loading a game. */
void reset_game_state();

void enable_game_messages( bool enabled );

void set_backend_for_testing( std::unique_ptr<backend> replacement );
void reset_backend_after_testing();

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_H
