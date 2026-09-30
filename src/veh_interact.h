#pragma once
#ifndef CATA_SRC_VEH_INTERACT_H
#define CATA_SRC_VEH_INTERACT_H

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "color.h"
#include "coordinates.h"
#include "cursesdef.h"
#include "input_context.h"
#include "input_enums.h"
#include "item_location.h"
#include "mapdata.h"
#include "memory_fast.h"
#include "player_activity.h"
#include "point.h"
#include "type_id.h"
#include "units.h"
#include "vpart_position.h"

class Character;
class inventory;
class map;
class time_duration;
class vpart_info;
struct requirement_data;

/** Represents possible return values from the cant_do function. */
enum class task_reason : int {
    UNKNOWN_TASK = -1, //No such task
    CAN_DO, //Task can be done
    INVALID_TARGET, //No valid target i.e. can't "change tire" if no tire present
    LACK_TOOLS, //Player doesn't have all the tools they need
    NOT_FREE, //Part is attached to something else and can't be unmounted
    LACK_SKILL, //Player doesn't have high enough mechanics skill
    MOVING_VEHICLE, // vehicle is moving, no modifications allowed
    LOW_MORALE, // Player has too low morale (for operations that require it)
    LOW_LIGHT // Player cannot see enough to work (for operations that require it)
};

class ui_adaptor;
class vehicle;
struct vehicle_part;

// For marking 'leaking' tanks/reactors/batteries
const std::string leak_marker = "<color_red>*</color>";

class veh_interact
{
        using part_selector = std::function<bool( const map &here, const vehicle_part &pt )>;

    public:
        static player_activity run( map &here,  vehicle &veh, const point_rel_ms &p );

        /** Prompt for a part matching the selector function */
        static std::optional<vpart_reference> select_part( map &here, const vehicle &veh,
                const part_selector &sel,
                const std::string &title = std::string() );

        static void complete_vehicle( map &here, Character &you );

        /**
         * Screen reader task menu of @p part for tests: each row as spoken, with
         * the lines of its requirement document.
         */
        static std::vector<std::pair<std::string, std::vector<std::string>>> accessible_tasks_for_testing(
            map &here, vehicle &veh, int part );
        /** Grid cursor speech on the tile of @p part, for tests. */
        static std::string accessible_tile_for_testing( map &here, vehicle &veh, int part );
        /** Vehicle statistics speech, for tests. */
        static std::string accessible_stats_for_testing( map &here, vehicle &veh );

    private:
        explicit veh_interact( map &here, vehicle &veh, const point_rel_ms &p = point_rel_ms::zero );
        ~veh_interact();

        point_rel_ms dd = point_rel_ms::zero;
        /* starting offset for vehicle parts description display and max offset for scrolling */
        int start_at = 0;
        int start_limit = 0;
        /* starting offset for the overview and the max offset for scrolling */
        int overview_offset = 0;
        int overview_limit = 0;
        /* starting offset for installation scrolling */
        int w_msg_scroll_offset = 0;
        /* starting offset for fuels scrolling */
        int fuel_index = 0;

        // target vehicle tank for refill with liquids
        item_location refill_target;

        const vehicle_part *sel_vehicle_part = nullptr;
        const vpart_info *sel_vpart_info = nullptr;

        // Command currently being run by the player
        char sel_cmd = ' ';

        int cpart = -1;
        int page_size = 0;
        // height of the stats window
        const int stats_h = 8;
        // element width defaults for 80 column display
        int disp_w = 26; // width of the left column
        int pane_w = 25; // width of the center and right columns
        catacurses::window w_border;
        catacurses::window w_mode;
        catacurses::window w_msg;
        catacurses::window w_disp;
        catacurses::window w_parts;
        catacurses::window w_stats;
        catacurses::window w_stats_1;
        catacurses::window w_stats_2;
        catacurses::window w_stats_3;
        catacurses::window w_list;
        catacurses::window w_details;
        catacurses::window w_name;

        weak_ptr_fast<ui_adaptor> ui;

        std::optional<std::string> title;
        std::optional<std::string> msg;

        bool ui_hidden = false;

        int highlight_part = -1;

        struct install_info_t;

        std::unique_ptr<install_info_t> install_info;

        struct remove_info_t;

        std::unique_ptr<remove_info_t> remove_info;

        vehicle *veh;
        const inventory *crafting_inv;
        input_context main_context;

        // maximum weight capacity of available lifting equipment (if any)
        units::mass max_lift;
        // maximum weight_capacity of available jacking equipment (if any)
        units::mass max_jack;

        shared_ptr_fast<ui_adaptor> create_or_get_ui_adaptor( map &here );
        void hide_ui( map &here, bool hide );

        player_activity serialize_activity( map &here );

        /** Requirement speech of a task for the screen reader interface. */
        struct task_speech {
            /** Requirement document lines with the state of each requirement in words. */
            std::vector<std::string> requirements;
            /** Number of unmet requirements. */
            size_t missing = 0;
            /** Why the task is impossible regardless of tools, e.g. "This part cannot be uninstalled". */
            std::string blocked;
        };

        /** Format list of requirements returning true if all are met */
        bool format_reqs( std::string &msg, const requirement_data &reqs,
                          const std::map<skill_id, int> &skills, time_duration time,
                          task_speech *speech = nullptr ) const;

        int part_at( const point_rel_ms &d );
        void move_cursor( map &here, const point_rel_ms &d, int dstart_at = 0 );
        task_reason cant_do( const map &here,  char mode );
        bool can_potentially_install( const vpart_info &vpart );
        /** Move index (parameter pos) according to input action:
         * (up or down, single step or whole page).
         * @param pos index to change.
         * @param action input action (taken from input_context::handle_input)
         * @param size size of the list to scroll, used to wrap the cursor around.
         * @param header number of lines reserved for list header.
         * @return false if the action is not a move action, the index is not changed in this case.
         */
        bool move_in_list( int &pos, const std::string &action, int size,
                           int header = 0 ) const;
        void move_fuel_cursor( map &here, int delta );

        /**
         * @name Task handlers
         *
         * One function for each specific task
         * @warning presently functions may mutate local state
         * @param msg failure message to display (if any)
         */
        /*@{*/
        void do_install( map &here );
        void do_repair( map &here );
        void do_mend( map &here );
        void do_refill( map &here );
        void do_remove( map &here );
        void do_rename();
        void do_siphon( map &here );
        // Returns true if exiting the screen
        bool do_unload( map &here );
        void do_assign_crew( map &here );
        void do_relabel( const map &here );
        /*@}*/

        /**
        * Calculates the lift requirements for a given vehicle_part
        * @return bool true if lift requirements are fulfilled
        * @return string msg for the ui to show the lift requirements
        */
        std::pair<bool, std::string> calc_lift_requirements( map &here, const vpart_info &sel_vpart_info,
                task_speech *speech = nullptr );
        /** Repair requirements of @p pt written to @p nmsg; true when they are met. */
        bool repair_requirements( map &here, vehicle_part &pt, std::string &nmsg,
                                  task_speech *speech = nullptr );

        /**
         * @name Screen reader interface
         *
         * A flat part list opens with the screen; each part has a task menu.
         * The task handlers use accessible lists in place of their grid loops.
         */
        /*@{*/
        enum class access_task : int {
            repair,
            mend,
            remove,
            install,
            refill,
            siphon,
            crew,
            shape,
            label
        };
        /** A task on one part, offered in an accessible task list. */
        struct access_option {
            access_task task;
            int part;
            /** Spoken and shown row name. */
            std::string label;
        };
        bool owned_by_player = true;
        /** Whether terrain or another vehicle occupies the cursor tile. */
        bool cursor_obstructed = false;
        /** Part that the overview selects without asking, or -1. */
        int preselected_part = -1;
        /** Open the part list; returns true when a task closes the screen. */
        bool accessible_part_list( map &here );
        /** Open the task menu of @p part; returns true when a task closes the screen. */
        bool accessible_part_tasks( map &here, int part );
        /** Choose one of @p parts on the cursor tile for @p task and start it. */
        void accessible_choose_part( map &here, access_task task, const std::vector<int> &parts );
        /**
         * Choose one of @p options; unavailable ones say why and stay open.
         * @return the chosen index, or -1 when cancelled.
         */
        int accessible_task_menu( map &here, const std::string &title,
                                  const std::vector<access_option> &options, bool reopen );
        /** Availability and requirements of each of @p options. */
        void evaluate_tasks( map &here, const std::vector<access_option> &options,
                             std::vector<std::string> &reasons, std::vector<task_speech> &speech );
        /** Tasks offered for @p part. */
        std::vector<access_option> part_tasks( map &here, int part );
        /** Choose a part to install on the cursor tile. */
        void accessible_install( map &here );
        /** Choose among the parts the current overview selector enables. */
        void accessible_overview( map &here );
        /** Handle an accessibility action on the grid; returns false for other actions. */
        bool accessible_grid_action( map &here, const std::string &action, bool &finish );
        /** Speak the result of a grid action: its message or the cursor tile. */
        void accessible_after_action( map &here, const std::string &action );
        /** Move the grid cursor to the tile of @p part. */
        void focus_part( map &here, int part );
        /** Why @p task cannot be done on @p part now, or empty; fills @p speech. */
        std::string task_unavailable( map &here, access_task task, int part, task_speech &speech );
        /** Start @p task on @p part; returns true when the screen should close. */
        bool run_task( map &here, access_task task, int part );
        std::string cursor_description() const;
        void speak_grid();
        std::string accessible_stats( map &here ) const;
        void show_requirements( const std::string &title, const std::string &reason,
                                const task_speech &speech ) const;
        /*@}*/

        void display_grid();
        void display_veh( map &here );
        void display_stats( map &here ) const;
        void display_name();
        void display_mode( const map &here );
        void display_list( size_t pos, const std::vector<const vpart_info *> &list, int header = 0 );
        void display_details( const vpart_info *part );

        struct part_option {
            part_option( const std::string &key, vehicle_part *part, bool selectable, const input_event &hotkey,
                         std::function<void( const vehicle_part &pt, const catacurses::window &w, int y )> details ) :
                key( key ), part( part ), selectable( selectable ), hotkey( hotkey ),
                details( std::move( details ) ) {}

            part_option( const std::string &key, vehicle_part *part, bool selectable, const input_event &hotkey,
                         std::function<void( const vehicle_part &pt, const catacurses::window &w, int y )> details,
                         std::function<void( const vehicle_part &pt )> message ) :
                key( key ), part( part ), selectable( selectable ), hotkey( hotkey ),
                details( std::move( details ) ),
                message( std::move( message ) ) {}

            std::string key;
            vehicle_part *part;

            /** Can the part be selected and used */
            bool selectable;

            /** Can @param action be run for this entry? */
            input_event hotkey;

            /** Writes any extra details for this entry */
            std::function<void( const vehicle_part &pt, const catacurses::window &w, int y )> details;

            /** Writes to message window when part is selected */
            std::function<void( const vehicle_part &pt )> message;
        };
        std::vector<part_option> overview_opts;
        std::map<std::string, std::function<void( const catacurses::window &, int )>> overview_headers;
        using overview_enable_t = std::function<bool( const map &here, const vehicle_part &pt )>;
        using overview_action_t = std::function<void( map &here, vehicle_part &pt )>;
        overview_enable_t overview_enable;
        overview_action_t overview_action;
        int overview_pos = -1;

        void calc_overview( map &here );
        void display_overview( const map &here );
        /**
         * Display overview of parts, optionally with interactive selection of one part
         *
         * @param enable used to determine parts of interest. If \p action also present, these
                         parts are the ones that can be selected. Otherwise, these are the parts
                         that will be highlighted
         * @param action callback when part is selected.
         */
        void overview( map &here, const overview_enable_t &enable = {},
                       const overview_action_t &action = {} );
        void move_overview_line( int );

        void count_durability();

        nc_color total_durability_color;
        std::string total_durability_text;

        /** Returns the most damaged part's index, or -1 if they're all healthy. */
        vehicle_part *get_most_damaged_part() const;

        /** Returns the index of the part that needs repair the most.
         * This may not be mostDamagedPart since not all parts can be repaired
         * If there are no damaged parts this returns -1 */
        vehicle_part *get_most_repairable_part() const;

        //do_remove supporting operation, writes requirements to ui
        bool can_remove_part( map &here, int idx, const Character &you, task_speech *speech = nullptr );
        //do install support, writes requirements to ui
        bool update_part_requirements( map &here, task_speech *speech = nullptr );

        /* Vector of all vpart TYPES that can be mounted in the current square.
         * Can be converted to a vector<vpart_info>.
         * Updated whenever the cursor moves. */
        std::vector<const vpart_info *> can_mount;

        /* Vector of vparts in the current square that can be repaired. Strictly a
         * subset of parts_here.
         * Can probably be removed entirely, otherwise is a vector<vehicle_part>.
         * Updated whenever parts_here is updated.
         */
        std::vector<int> need_repair;

        /* Vector of all vparts that exist on the vehicle in the current square.
         * Can be converted to a vector<vehicle_part>.
         * Updated whenever the cursor moves. */
        std::vector<int> parts_here;

        /* Terrain at current square.
         * Updated whenever the cursor moves. */
        ter_t terrain_here;

        void cache_tool_availability();
        void allocate_windows();
        void do_main_loop( map &here );

        void cache_tool_availability_update_lifting( const tripoint_bub_ms &world_cursor_pos );

        /** Returns true if the vehicle has a jack powerful enough to lift itself installed */
        bool can_self_jack( map &here );
};

void act_vehicle_siphon( map &here, vehicle *veh );

void orient_part( map &here, vehicle *veh, const vpart_info &vpinfo, int partnum,
                  const std::optional<point_rel_ms> &part_placement = std::nullopt );

#endif // CATA_SRC_VEH_INTERACT_H
