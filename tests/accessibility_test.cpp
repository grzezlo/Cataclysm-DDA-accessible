#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "accessibility.h"
#include "cata_catch.h"

namespace
{

class capture_backend final : public accessibility::backend
{
    public:
        bool is_available() const override {
            return true;
        }

        bool announce( const std::string &message ) override {
            messages.push_back( message );
            return true;
        }

        std::vector<std::string> messages;
};

} // namespace

TEST_CASE( "accessibility announcements can be captured and repeated", "[accessibility][nogame]" )
{
    auto capture = std::make_unique<capture_backend>();
    capture_backend *capture_ptr = capture.get();
    accessibility::set_backend_for_testing( std::move( capture ) );

    CHECK( accessibility::is_available() );
    CHECK_FALSE( accessibility::announce( "" ) );
    CHECK( capture_ptr->messages.empty() );

    CHECK( accessibility::announce( "Main menu" ) );
    CHECK( accessibility::announce_focus( "Custom character", "New game" ) );
    CHECK( accessibility::announce_status( "Loading" ) );
    CHECK( accessibility::last_announcement() == "Loading" );
    CHECK_FALSE( accessibility::announce_game_message( "Hidden during setup" ) );
    accessibility::enable_game_messages( true );
    CHECK( accessibility::announce_game_message( "You hear a noise." ) );
    CHECK( accessibility::announce_movement( "northwest", "wooden floor" ) );
    CHECK( accessibility::announce_movement( "northwest", "grass" ) );
    CHECK( accessibility::announce_movement( "east", "grass" ) );
    CHECK_FALSE( accessibility::announce_threats( {}, "No nearby threats." ) );
    CHECK( accessibility::announce_threats( { "monster-1" }, "Nearby threats: zombie, 5 tiles north." ) );
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
    CHECK( capture_ptr->messages[4] == "wooden floor. northwest" );
    CHECK( capture_ptr->messages[5] == "grass" );
    CHECK( capture_ptr->messages[6] == "grass. east" );
    CHECK( capture_ptr->messages[7] == "Nearby threats: zombie, 5 tiles north." );
    CHECK( capture_ptr->messages[8] == "A new nearby threat." );
    CHECK( capture_ptr->messages[9] == "No nearby threats." );
    CHECK( capture_ptr->messages[10] == "No nearby threats." );

    accessibility::reset_backend_after_testing();
}

TEST_CASE( "semantic accessibility lists have stable distance ordering",
           "[accessibility][nogame]" )
{
    std::vector<accessibility::semantic_list_entry> entries = {
        { 2, 0, 0, 2, 0, "far east", "2 tiles east" },
        { 1, 0, 1, 0, 1, "south item", "1 tile south" },
        { 1, 0, -1, 0, 2, "north obstacle", "1 tile north" },
        { 0, 0, 0, 0, 0, "current position", "here" },
        { 1, 0, -1, 0, 1, "north hazard", "1 tile north" },
    };

    accessibility::sort_semantic_list_entries( entries );

    REQUIRE( entries.size() == 5 );
    CHECK( entries[0].label == "current position" );
    CHECK( entries[1].label == "north hazard" );
    CHECK( entries[2].label == "north obstacle" );
    CHECK( entries[3].label == "south item" );
    CHECK( entries[4].label == "far east" );
}

TEST_CASE( "semantic accessibility lists group connected matching regions",
           "[accessibility][nogame]" )
{
    std::vector<accessibility::semantic_list_entry> entries = {
        { 3, 0, 0, 3, 1, "wall", "3 tiles east" },
        { 1, 0, 0, 1, 1, "wall", "1 tile east" },
        { 2, 0, 1, 2, 1, "wall", "2 tiles southeast" },
        { 5, 0, 0, -5, 1, "wall", "5 tiles west" },
        { 2, 0, 0, 2, 2, "deep water", "2 tiles east" },
        { 3, 0, 1, 3, 2, "deep water", "3 tiles southeast" },
        { 4, 1, 1, 3, 1, "wall", "above" },
    };

    const std::vector<accessibility::connected_semantic_list_group> groups =
        accessibility::group_connected_semantic_list_entries( std::move( entries ) );

    REQUIRE( groups.size() == 4 );
    CHECK( groups[0].nearest.label == "wall" );
    CHECK( groups[0].nearest.context == "1 tile east" );
    CHECK( groups[0].count == 3 );
    CHECK( groups[1].nearest.label == "deep water" );
    CHECK( groups[1].count == 2 );
    CHECK( groups[2].nearest.context == "above" );
    CHECK( groups[2].count == 1 );
    CHECK( groups[3].nearest.context == "5 tiles west" );
    CHECK( groups[3].count == 1 );
}
