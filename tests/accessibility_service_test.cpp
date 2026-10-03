#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "accessibility.h"
#include "accessibility_format.h"
#include "accessibility_test_helpers.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "popup.h"

// Expected speech strings mirror the spoken output, which uses single spaces after sentences.
// NOLINTBEGIN(cata-text-style)

TEST_CASE( "accessibility_announcements_can_be_captured_and_repeated", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    CHECK( accessibility::is_active() );
    CHECK_FALSE( accessibility::announce( "" ) );
    CHECK( capture_ptr->messages.empty() );

    CHECK( accessibility::announce( "Main menu" ) );
    CHECK( accessibility::announce_focus( "Custom character", "New game" ) );
    CHECK( accessibility::announce_status( "Loading" ) );
    CHECK( accessibility::last_announcement() == "Loading" );
    CHECK_FALSE( accessibility::announce_game_message( "Hidden during setup" ) );
    accessibility::enable_game_messages( true );
    CHECK( accessibility::announce_game_message( "You hear a noise." ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    CHECK( accessibility::announce_movement( "northwest", "wooden floor" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( accessibility::announce_movement( "northwest", "grass" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( accessibility::announce_movement( "east", "grass" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    // Routine movement speech never replaces the repeat-last message.
    CHECK( accessibility::last_announcement() == "You hear a noise." );
    CHECK_FALSE( accessibility::announce_threats( {}, "No nearby threats." ) );
    CHECK( accessibility::announce_threats( { "monster-1" },
                                            "Nearby threats: zombie, 5 tiles north." ) );
    CHECK_FALSE( accessibility::announce_threats( { "monster-1" }, "This must be deduplicated." ) );
    CHECK( accessibility::announce_threats( { "monster-2" }, "A new nearby threat." ) );
    CHECK_FALSE( accessibility::announce_threats( { "monster-2" }, "This must be deduplicated too." ) );
    CHECK( accessibility::announce_threats( {}, "No nearby threats." ) );
    CHECK( accessibility::last_announcement() == "No nearby threats." );
    REQUIRE( accessibility::repeat_last_announcement() );

    REQUIRE( capture_ptr->messages.size() == 11 );
    CHECK( capture_ptr->messages[0] == "Main menu" );
    CHECK( capture_ptr->messages[1] == "New game. Custom character" );
    CHECK( capture_ptr->messages[2] == "Loading" );
    CHECK( capture_ptr->messages[3] == "You hear a noise." );
    CHECK( capture_ptr->messages[4] == "wooden floor. northwest." );
    CHECK( capture_ptr->messages[5] == "grass." );
    CHECK( capture_ptr->messages[6] == "grass. east." );
    CHECK( capture_ptr->messages[7] == "Nearby threats: zombie, 5 tiles north." );
    CHECK( capture_ptr->messages[8] == "A new nearby threat." );
    CHECK( capture_ptr->messages[9] == "No nearby threats." );
    CHECK( capture_ptr->messages[10] == "No nearby threats." );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "accessibility_last_announcement_can_be_set_quietly_and_repeated",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    accessibility::announce_status( "Loading the save…" );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0] == "Loading the save…" );

    // Quietly update progress during loading - no speech should be emitted
    accessibility::set_last_announcement( "Loading the save… Core game data" );
    CHECK( capture_ptr->messages.size() == 1 );
    CHECK( accessibility::last_announcement() == "Loading the save… Core game data" );

    accessibility::set_last_announcement( "Loading the save… Overmap" );
    CHECK( capture_ptr->messages.size() == 1 );
    CHECK( accessibility::last_announcement() == "Loading the save… Overmap" );

    // Repeating speaks the latest quiet progress update on demand
    REQUIRE( accessibility::repeat_last_announcement() );
    REQUIRE( capture_ptr->messages.size() == 2 );
    CHECK( capture_ptr->messages[1] == "Loading the save… Overmap" );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "real_time_progress_obeys_silence_interval_deduplication_and_repeat",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    std::chrono::steady_clock::time_point now{};
    accessibility::set_clock_for_testing( [&now]() {
        return now;
    } );
    accessibility::announce_status( "Loading" );
    accessibility::progress_reporter progress;
    progress.update( "Loading 10%" );
    now += std::chrono::milliseconds( 4999 );
    CHECK_FALSE( accessibility::poll_progress() );
    now += std::chrono::milliseconds( 1 );
    CHECK( accessibility::poll_progress() );
    REQUIRE( speech->messages.size() == 2 );
    CHECK( speech->messages.back() == "Loading 10%" );
    CHECK( speech->cancels == 0 );
    now += std::chrono::seconds( 20 );
    progress.update( "Loading 10%" );
    CHECK( speech->messages.size() == 2 );
    progress.update( "Loading 20%" );
    REQUIRE( speech->messages.size() == 3 );
    progress.update( "Loading 30%" );
    now += std::chrono::seconds( 4 );
    accessibility::note_player_input();
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( speech->messages.back() == "Loading 30%" );
    const int cancels_after_repeat = speech->cancels;
    // Silencing NVDA externally does not make the same text eligible again.
    now += std::chrono::seconds( 30 );
    CHECK_FALSE( accessibility::poll_progress() );
    CHECK( speech->messages.size() == 4 );
    progress.update( "Loading 40%" );
    CHECK( speech->messages.size() == 5 );
    CHECK( speech->cancels == cancels_after_repeat );

    accessibility::announce_focus( "Stop loading? Yes" );
    now += std::chrono::seconds( 30 );
    CHECK_FALSE( accessibility::poll_progress() );
    // A prompt suspends progress until the operation actually resumes.
    progress.update( "Loading 50%" );
    CHECK( speech->messages.back() == "Loading 50%" );
    accessibility::set_progress_announcement_interval( 0 );
    progress.update( "Loading 60%" );
    now += std::chrono::hours( 1 );
    CHECK_FALSE( accessibility::poll_progress() );
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( speech->messages.back() == "Loading 60%" );
}

TEST_CASE( "progress_sources_end_with_their_owner_and_prioritize_warnings",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    std::chrono::steady_clock::time_point now{};
    accessibility::set_clock_for_testing( [&now]() {
        return now;
    } );
    {
        accessibility::progress_reporter progress;
        progress.update( "Reading 10%" );
        now += std::chrono::seconds( 5 );
        accessibility::queue_gameplay_announcement( "Danger!",
                accessibility::gameplay_speech::meaningful );
        CHECK_FALSE( accessibility::poll_progress() );
        REQUIRE( speech->messages.size() == 1 );
        CHECK( speech->messages.back() == "Danger!" );
        now += std::chrono::seconds( 5 );
        CHECK( accessibility::poll_progress() );
        accessibility::set_progress_announcement_interval( 10 );
        progress.update( "Reading 20%" );
        now += std::chrono::seconds( 9 );
        CHECK_FALSE( accessibility::poll_progress() );
        now += std::chrono::seconds( 1 );
        CHECK( accessibility::poll_progress() );
        accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::off );
        progress.update( "Reading 30%" );
        now += std::chrono::seconds( 10 );
        CHECK_FALSE( accessibility::poll_progress() );
    }
    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::on );
    CHECK_FALSE( accessibility::poll_progress() );
    CHECK( speech->messages.size() == 3 );
}

TEST_CASE( "static_progress_popups_speak_one_start_and_rate_limited_updates",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    std::chrono::steady_clock::time_point now{};
    accessibility::set_clock_for_testing( [&now]() {
        return now;
    } );
    {
        static_popup saving;
        saving.progress_message( "%s", "Saving 10%" );
        saving.progress_message( "%s", "Saving 20%" );
        REQUIRE( speech->messages.size() == 1 );
        CHECK( speech->messages.back() == "Saving" );
        CHECK( accessibility::last_announcement() == "Saving 20%" );
        now += std::chrono::seconds( 5 );
        CHECK( accessibility::poll_progress() );
        CHECK( speech->messages.back() == "Saving 20%" );
        saving.progress_message( "%s", "Saving 20%" );
        CHECK( speech->messages.size() == 2 );
    }
    now += std::chrono::seconds( 5 );
    CHECK_FALSE( accessibility::poll_progress() );
}

TEST_CASE( "activity_message_capture_keeps_results_without_turn_speech",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *speech = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    on_out_of_scope restore( []() {
        accessibility::reset_backend_after_testing();
    } );
    {
        accessibility::game_message_speech_capture result( true );
        CHECK( accessibility::announce_game_message( "Finished waiting" ) );
        CHECK( accessibility::announce_game_message( "Finished waiting" ) );
        {
            accessibility::game_message_speech_suppression quiet;
            CHECK_FALSE( accessibility::announce_game_message( "Suppressed" ) );
        }
        CHECK_FALSE( accessibility::flush_gameplay_announcements() );
        CHECK( speech->messages.empty() );
        CHECK( result.messages() == std::vector<std::string> { "Finished waiting" } );
    }
    CHECK( accessibility::announce_game_message( "Normal gameplay" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( speech->messages == std::vector<std::string> { "Normal gameplay." } );
}

TEST_CASE( "accessibility_urgent_events_are_combined_and_reset_between_games",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );
    accessibility::reset_game_state();

    accessibility::condition_tracker normal;
    normal.hunger_tier = "Satisfied";
    normal.thirst_tier = "Hydrated";
    normal.sleepiness_tier = "Rested";
    normal.pain_tier = "No pain";
    normal.weariness_tier = "Fresh";

    int descriptions_built = 0;
    const auto describe = [&descriptions_built]( const std::string & text ) {
        // The test intentionally returns a string copy from this callback.
        // NOLINTNEXTLINE(bugprone-exception-escape)
        return [&descriptions_built, text]() {
            ++descriptions_built;
            return text;
        };
    };
    CHECK_FALSE( accessibility::evaluate_threat_transition( {}, describe( "No threats in view" ) ) );
    CHECK( descriptions_built == 0 );
    CHECK( accessibility::evaluate_condition_transitions( normal ).empty() );

    const std::optional<std::string> threat = accessibility::evaluate_threat_transition(
    { "monster-1" }, describe( "Zombie spotted!" ) );
    REQUIRE( threat );
    CHECK( descriptions_built == 1 );
    CHECK_FALSE( accessibility::evaluate_threat_transition( { "monster-1" },
                 describe( "Unchanged threats are not described" ) ) );
    CHECK( descriptions_built == 1 );

    accessibility::condition_tracker hungry = normal;
    hungry.hunger_tier = "Hungry";
    std::vector<std::string> alerts = { *threat };
    const std::vector<std::string> condition_alerts =
        accessibility::evaluate_condition_transitions( hungry );
    alerts.insert( alerts.end(), condition_alerts.begin(), condition_alerts.end() );

    REQUIRE( accessibility::announce_semantic_sequence( alerts ) );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0] == "Zombie spotted! Hunger: Hungry." );
    CHECK( accessibility::last_announcement() == capture_ptr->messages[0] );

    accessibility::reset_game_state();
    CHECK( accessibility::evaluate_condition_transitions( hungry ).empty() );
    CHECK( accessibility::evaluate_threat_transition( { "monster-1" },
            describe( "Zombie spotted!" ) ) );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "screen_reader_mode_controls_speech_and_latches_detection", "[accessibility][nogame]" )
{
    class toggled_backend final : public accessibility::backend
    {
        public:
            bool is_available() const override {
                return available;
            }

            bool announce( const std::string &message ) override {
                messages.push_back( message );
                return true;
            }

            bool available = false;
            std::vector<std::string> messages;
    };

    auto backend = std::make_unique<toggled_backend>();
    toggled_backend *backend_ptr = backend.get();
    accessibility::set_backend_for_testing( std::move( backend ) );

    // Automatic mode stays inactive while no screen reader is detected.
    CHECK_FALSE( accessibility::is_active() );
    CHECK_FALSE( accessibility::announce( "Silent" ) );
    CHECK( backend_ptr->messages.empty() );

    // Forced on works without a detected screen reader, e.g. for testing.
    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::on );
    CHECK( accessibility::is_active() );
    CHECK( accessibility::announce( "Forced" ) );

    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::off );
    backend_ptr->available = true;
    CHECK_FALSE( accessibility::is_active() );
    CHECK_FALSE( accessibility::announce( "Disabled" ) );
    CHECK_FALSE( accessibility::repeat_last_announcement() );

    // Selecting automatic mode detects immediately and keeps the interface
    // active if the screen reader later becomes unavailable.
    accessibility::set_screen_reader_mode( accessibility::screen_reader_mode::automatic );
    CHECK( accessibility::is_active() );
    backend_ptr->available = false;
    CHECK( accessibility::is_active() );

    REQUIRE( backend_ptr->messages.size() == 1 );
    CHECK( backend_ptr->messages[0] == "Forced" );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "focus_speech_interrupts_once_per_key_and_gameplay_speech_queues",
           "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    // Speech without a new key press never interrupts.
    CHECK( accessibility::announce( "Inventory" ) );
    CHECK( capture_ptr->cancels == 0 );

    // The first focus speech after a key replaces unfinished speech; context
    // spoken for the same key queues behind it.
    accessibility::note_player_input();
    CHECK( accessibility::announce( "Hammer" ) );
    CHECK( accessibility::announce( "Details available" ) );
    CHECK( capture_ptr->cancels == 1 );
    accessibility::note_player_input();
    CHECK( accessibility::announce( "Rock" ) );
    CHECK( capture_ptr->cancels == 2 );

    // Gameplay speech of one turn is one utterance and never interrupts.
    accessibility::enable_game_messages( true );
    accessibility::note_player_input();
    CHECK( accessibility::announce_movement( "north", "grass" ) );
    CHECK( accessibility::announce_game_message( "You hear a whack!" ) );
    accessibility::queue_gameplay_announcement( "Zombie spotted!",
            accessibility::gameplay_speech::meaningful );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK( capture_ptr->cancels == 2 );
    CHECK( capture_ptr->messages.back() == "grass. north. You hear a whack! Zombie spotted!" );
    // Repeat-last keeps the meaningful parts without the routine tile name.
    CHECK( accessibility::last_announcement() == "You hear a whack! Zombie spotted!" );

    // A prompt caused by the same turn speaks pending alerts first and does
    // not cut them off.
    accessibility::note_player_input();
    accessibility::queue_gameplay_announcement( "Bleeding: Torso!",
            accessibility::gameplay_speech::meaningful );
    CHECK( accessibility::announce( "Really continue?" ) );
    CHECK( capture_ptr->cancels == 2 );
    REQUIRE( capture_ptr->messages.size() >= 2 );
    CHECK( capture_ptr->messages[capture_ptr->messages.size() - 2] == "Bleeding: Torso!" );
    CHECK( capture_ptr->messages.back() == "Really continue?" );

    // Repeat-last is a key press and replaces unfinished speech.
    accessibility::note_player_input();
    CHECK( accessibility::repeat_last_announcement() );
    CHECK( capture_ptr->cancels == 3 );
    CHECK( capture_ptr->messages.back() == "Really continue?" );

    // Skipped auto-move steps are reported once.
    accessibility::note_suppressed_movement();
    CHECK( accessibility::take_suppressed_movement() );
    CHECK_FALSE( accessibility::take_suppressed_movement() );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "repeated_game_messages_need_a_new_key_press", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    // An activity such as smashing repeats the same message every turn
    // without input; only the first one is spoken.
    accessibility::note_player_input();
    CHECK( accessibility::announce_game_message( "You hear whack!" ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    CHECK_FALSE( accessibility::announce_game_message( "You hear whack!" ) );
    CHECK_FALSE( accessibility::flush_gameplay_announcements() );
    CHECK_FALSE( accessibility::announce_game_message( "You hear whack!" ) );
    CHECK( accessibility::announce_game_message( "The chair is destroyed." ) );
    CHECK( accessibility::flush_gameplay_announcements() );

    // A repeat caused by a new key press, such as another attack, is spoken.
    accessibility::note_player_input();
    CHECK( accessibility::announce_game_message( "You miss the zombie." ) );
    accessibility::note_player_input();
    CHECK( accessibility::announce_game_message( "You miss the zombie." ) );
    CHECK( accessibility::flush_gameplay_announcements() );

    REQUIRE( capture_ptr->messages.size() == 3 );
    CHECK( capture_ptr->messages[0] == "You hear whack!" );
    CHECK( capture_ptr->messages[1] == "The chair is destroyed." );
    CHECK( capture_ptr->messages[2] == "You miss the zombie. You miss the zombie." );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "suppressed_game_messages_are_not_spoken", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );
    accessibility::enable_game_messages( true );

    accessibility::note_player_input();
    {
        accessibility::game_message_speech_suppression quiet;
        CHECK_FALSE( accessibility::announce_game_message( "You pick up: 1 rock" ) );
        {
            accessibility::game_message_speech_suppression nested;
            CHECK_FALSE( accessibility::announce_game_message( "You pick up: 1 hammer" ) );
        }
        CHECK_FALSE( accessibility::announce_game_message( "You pick up: 1 pipe" ) );
    }
    CHECK( accessibility::announce_game_message( "Picked up: rock, hammer, pipe." ) );
    CHECK( accessibility::flush_gameplay_announcements() );
    REQUIRE( capture_ptr->messages.size() == 1 );
    CHECK( capture_ptr->messages[0] == "Picked up: rock, hammer, pipe." );

    accessibility::reset_backend_after_testing();
}

// NOLINTEND(cata-text-style)
