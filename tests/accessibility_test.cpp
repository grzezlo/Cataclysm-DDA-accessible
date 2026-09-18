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
    CHECK( accessibility::last_announcement() == "New game. Custom character" );
    REQUIRE( accessibility::repeat_last_announcement() );

    REQUIRE( capture_ptr->messages.size() == 3 );
    CHECK( capture_ptr->messages[0] == "Main menu" );
    CHECK( capture_ptr->messages[1] == "New game. Custom character" );
    CHECK( capture_ptr->messages[2] == "New game. Custom character" );

    accessibility::reset_backend_after_testing();
}
