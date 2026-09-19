#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_H
#define CATA_SRC_ACCESSIBILITY_H

#include <memory>
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

void sort_semantic_list_entries( std::vector<semantic_list_entry> &entries );
std::vector<connected_semantic_list_group> group_connected_semantic_list_entries(
    std::vector<semantic_list_entry> entries );

bool is_available();
bool announce( const std::string &message );
bool announce_focus( const std::string &label, const std::string &context = {} );
bool announce_status( const std::string &message );
bool announce_game_message( const std::string &message );
bool announce_movement( const std::string &direction, const std::string &tile );
bool announce_threats( const std::vector<std::string> &threat_ids, const std::string &message );
bool repeat_last_announcement();
std::string last_announcement();

void enable_game_messages( bool enabled );

void set_backend_for_testing( std::unique_ptr<backend> replacement );
void reset_backend_after_testing();

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_H
