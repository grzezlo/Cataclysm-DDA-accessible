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
    CHECK( accessibility::last_announcement() == "northwest. wooden floor" );
    REQUIRE( accessibility::repeat_last_announcement() );

    REQUIRE( capture_ptr->messages.size() == 6 );
    CHECK( capture_ptr->messages[0] == "Main menu" );
    CHECK( capture_ptr->messages[1] == "New game. Custom character" );
    CHECK( capture_ptr->messages[2] == "Loading" );
    CHECK( capture_ptr->messages[3] == "You hear a noise." );
    CHECK( capture_ptr->messages[4] == "northwest. wooden floor" );
    CHECK( capture_ptr->messages[5] == "northwest. wooden floor" );

    accessibility::reset_backend_after_testing();
}
