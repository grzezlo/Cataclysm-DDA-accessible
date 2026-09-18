#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_H
#define CATA_SRC_ACCESSIBILITY_H

#include <memory>
#include <string>

namespace accessibility
{

class backend
{
    public:
        virtual ~backend() = default;

        virtual bool is_available() const = 0;
        virtual bool announce( const std::string &message ) = 0;
};

bool is_available();
bool announce( const std::string &message );
bool announce_focus( const std::string &label, const std::string &context = {} );
bool announce_status( const std::string &message );
bool repeat_last_announcement();
std::string last_announcement();

void set_backend_for_testing( std::unique_ptr<backend> replacement );
void reset_backend_after_testing();

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_H
