#include "character.h" // IWYU pragma: associated

#include <algorithm> //std::min
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "accessibility.h"
#include "accessibility_document_view.h"
#include "accessibility_format.h"
#include "avatar.h"
#include "cata_scope_helpers.h"
#include "cata_utility.h"
#include "color.h"
#include "cursesdef.h"
#include "enums.h"
#include "input_context.h"
#include "inventory.h"
#include "magic.h"
#include "mutation.h"
#include "mutation_ui.h"
#include "output.h"
#include "popup.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui_manager.h"
enum class mutation_menu_mode {
    activating,
    examining,
    reassigning,
    hiding
};
enum class mutation_tab_mode {
    active,
    passive,
    none
};
// '!' and '=' are uses as default bindings in the menu
static const invlet_wrapper
mutation_chars( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\"#&()*+./:;@[\\]^_{|}" );

static void draw_exam_window( const catacurses::window &win, const int border_y )
{
    const int width = getmaxx( win );
    wattron( win, BORDER_COLOR );
    mvwaddch( win, point( 0, border_y ), LINE_XXXO );
    mvwhline( win, point( 1, border_y ), LINE_OXOX, width - 2 );
    mvwaddch( win, point( width - 1, border_y ), LINE_XOXX );
    wattroff( win, BORDER_COLOR );
}

static const auto shortcut_desc = []( std::string_view comment, const std::string &keys )
{
    return string_format( comment, string_format( "[<color_yellow>%s</color>]", keys ) );
};

// needs extensive improvement

static std::optional<trait_id> GetTrait( const std::vector<trait_id> &active,
        const std::vector<trait_id> &passive,
        int cursor, mutation_tab_mode tab_mode )
{
    std::optional<trait_id> mut_id;
    if( tab_mode == mutation_tab_mode::active ) {
        mut_id = active[cursor];
    } else if( tab_mode == mutation_tab_mode::passive ) {
        mut_id = passive[cursor];
    }
    return mut_id;
}

static void show_mutations_titlebar( const catacurses::window &window,
                                     const mutation_menu_mode menu_mode, const input_context &ctxt )
{
    werase( window );
    std::string desc;
    if( menu_mode == mutation_menu_mode::reassigning ) {
        desc += std::string( _( "Reassigning." ) ) + "  " +
                _( "Select a mutation to reassign or press [<color_yellow>SPACE</color>] to cancel. " );
    }
    if( menu_mode == mutation_menu_mode::activating ) {
        desc += colorize( _( "Activating" ),
                          c_green ) + "  " + shortcut_desc( _( "%s Examine, " ),
                                  ctxt.get_desc( "TOGGLE_EXAMINE" ) );
    }
    if( menu_mode == mutation_menu_mode::examining ) {
        desc += colorize( _( "Examining" ),
                          c_light_blue ) + "  " + shortcut_desc( _( "%s Activate, " ),
                                  ctxt.get_desc( "TOGGLE_EXAMINE" ) );
    }
    if( menu_mode == mutation_menu_mode::hiding ) {
        desc += colorize( _( "Hiding" ), c_cyan ) + "  " + shortcut_desc( _( "%s Activate, " ),
                ctxt.get_desc( "TOGGLE_EXAMINE" ) );
    }
    if( menu_mode != mutation_menu_mode::reassigning ) {
        desc += shortcut_desc( _( "%s Reassign, " ), ctxt.get_desc( "REASSIGN" ) );
    }
    desc += shortcut_desc( _( "%s Toggle sprite visibility, " ), ctxt.get_desc( "TOGGLE_SPRITE" ) );
    desc += shortcut_desc( _( "%s Change keybindings." ), ctxt.get_desc( "HELP_KEYBINDINGS" ) );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    fold_and_print( window, point( 1, 0 ), getmaxx( window ) - 1, c_white, desc );
    wnoutrefresh( window );
}

// Whether the character has enough of the resources the mutation uses to activate it.
static bool mutation_resources_available( const avatar &you, const mutation_branch &mut_data )
{
    return ( !mut_data.hunger || you.get_kcal_percent() >= 0.8f ) &&
           ( !mut_data.thirst || you.get_thirst() <= 400 ) &&
           ( !mut_data.sleepiness || you.get_sleepiness() <= 400 ) &&
           ( !mut_data.mana || you.magic->available_mana() >= mut_data.cost );
}

// Screen reader speech for the mutations screen.  Rows speak in words what the
// columns show by color: on or off, starting traits, and missing resources.
static std::string mutation_accessibility_row( avatar &you, const trait_id &mut )
{
    const mutation_branch &md = mut.obj();
    const Character::trait_data &td = you.cached_mutations[mut];
    accessibility::mutation_power_row row;
    row.name = you.mutation_name( mut );
    row.activated = md.activated;
    row.powered = td.powered;
    row.starting_trait = you.has_base_trait( mut );
    row.sprite_hidden = !td.show_sprite;
    if( md.activated ) {
        if( md.hunger ) {
            row.resources.emplace_back( _( "kcal" ) );
        }
        if( md.thirst ) {
            row.resources.emplace_back( _( "thirst" ) );
        }
        if( md.sleepiness ) {
            row.resources.emplace_back( _( "sleepiness" ) );
        }
        if( md.mana ) {
            row.resources.emplace_back( _( "mana" ) );
        }
        row.cost = md.cost;
        if( md.cooldown > 0_turns ) {
            row.cooldown = to_string_clipped( md.cooldown );
        }
        row.insufficient = !td.powered && !mutation_resources_available( you, md );
    }
    return accessibility::format_mutation_power_row( row );
}

static std::string mutation_column_name( const mutation_tab_mode tab )
{
    return tab == mutation_tab_mode::active ? _( "Active mutations" ) : _( "Passive mutations" );
}

static accessibility::power_menu_mode mutation_accessibility_mode( const mutation_menu_mode mode )
{
    switch( mode ) {
        case mutation_menu_mode::activating:
            return accessibility::power_menu_mode::activating;
        case mutation_menu_mode::examining:
            return accessibility::power_menu_mode::examining;
        case mutation_menu_mode::reassigning:
            return accessibility::power_menu_mode::reassigning;
        case mutation_menu_mode::hiding:
            return accessibility::power_menu_mode::hiding;
    }
    return accessibility::power_menu_mode::activating;
}

static const std::vector<trait_id> *mutation_column( const std::vector<trait_id> &active,
        const std::vector<trait_id> &passive, const mutation_tab_mode tab )
{
    if( tab == mutation_tab_mode::active ) {
        return &active;
    }
    if( tab == mutation_tab_mode::passive ) {
        return &passive;
    }
    return nullptr;
}

static std::string mutation_accessibility_focus( avatar &you, const std::vector<trait_id> *column,
        const int cursor )
{
    if( column == nullptr || cursor < 0 || static_cast<size_t>( cursor ) >= column->size() ) {
        return std::string();
    }
    return mutation_accessibility_row( you, ( *column )[cursor] );
}

static std::string mutation_accessibility_tab( avatar &you, const std::vector<trait_id> *column,
        const mutation_tab_mode tab, const int cursor )
{
    if( column == nullptr ) {
        return accessibility::format_list_count( 0 );
    }
    return accessibility::format_character_tab( mutation_column_name( tab ), {}, column->size(),
            mutation_accessibility_focus( you, column, cursor ) );
}

static std::string mutation_accessibility_key( avatar &you, const trait_id &mut )
{
    return accessibility::format_assigned_key( std::string( 1, you.cached_mutations[mut].key ) );
}

static std::string mutation_accessibility_summary( avatar &you,
        const std::vector<trait_id> *column, const mutation_tab_mode tab, const int cursor,
        const mutation_menu_mode mode )
{
    std::vector<std::string> parts = {
        accessibility::format_power_menu_mode( mutation_accessibility_mode( mode ) )
    };
    if( column == nullptr ) {
        parts.push_back( accessibility::format_list_count( 0 ) );
    } else {
        parts.push_back( mutation_column_name( tab ) );
        if( cursor >= 0 && static_cast<size_t>( cursor ) < column->size() ) {
            parts.push_back( accessibility::format_list_position( cursor + 1, column->size() ) );
            parts.push_back( mutation_accessibility_key( you, ( *column )[cursor] ) );
        }
    }
    return accessibility::format_semantic_sequence( parts );
}

// The row with its column position, then the examine pane text as paragraphs.
static std::vector<std::string> mutation_accessibility_details( avatar &you,
        const std::vector<trait_id> &active, const std::vector<trait_id> &passive,
        const trait_id &mut )
{
    std::string position;
    for( const std::vector<trait_id> *column : {
             &active, &passive
         } ) {
        const auto found = std::find( column->begin(), column->end(), mut );
        if( found != column->end() ) {
            position = accessibility::format_semantic_sequence( {
                mutation_column_name( column == &active ? mutation_tab_mode::active :
                                      mutation_tab_mode::passive ),
                accessibility::format_list_position( found - column->begin() + 1, column->size() )
            } );
        }
    }
    std::vector<std::string> paragraphs = { accessibility::format_semantic_sequence( {
            mutation_accessibility_row( you, mut ), position, mutation_accessibility_key( you, mut )
        } )
    };
    std::string description = you.mutation_desc( mut );
    if( !you.purifiable( mut ) ) {
        description +=
            _( "\n<color_yellow>This trait is an intrinsic part of you now, purifier won't be able to remove it.</color>" );
    }
    for( const std::string &paragraph : string_split( remove_color_tags( description ), '\n' ) ) {
        const std::string spoken = accessibility::format_semantic_sequence( { paragraph } );
        if( !spoken.empty() ) {
            paragraphs.push_back( spoken );
        }
    }
    return paragraphs;
}

// The columns as power_mutations() builds them, without assigning letters.
static void mutation_accessibility_columns( avatar &you, std::vector<trait_id> &active,
        std::vector<trait_id> &passive )
{
    for( const std::pair<const trait_id, Character::trait_data> &mut : you.cached_mutations ) {
        if( mut.second.corrupted > 0 || !mut.first->player_display ) {
            continue;
        }
        ( mut.first->activated ? active : passive ).push_back( mut.first );
    }
}

std::string mutations_accessibility_tab( avatar &you, const bool active_column )
{
    std::vector<trait_id> active;
    std::vector<trait_id> passive;
    mutation_accessibility_columns( you, active, passive );
    const mutation_tab_mode tab = active_column ? mutation_tab_mode::active :
                                  mutation_tab_mode::passive;
    return mutation_accessibility_tab( you, mutation_column( active, passive, tab ), tab, 0 );
}

std::vector<std::string> mutations_accessibility_rows( avatar &you, const bool active_column )
{
    std::vector<trait_id> active;
    std::vector<trait_id> passive;
    mutation_accessibility_columns( you, active, passive );
    std::vector<std::string> rows;
    for( const trait_id &mut : active_column ? active : passive ) {
        rows.push_back( mutation_accessibility_row( you, mut ) );
    }
    return rows;
}

std::vector<std::string> mutations_accessibility_details( avatar &you, const trait_id &mut )
{
    std::vector<trait_id> active;
    std::vector<trait_id> passive;
    mutation_accessibility_columns( you, active, passive );
    return mutation_accessibility_details( you, active, passive, mut );
}

std::string mutations_accessibility_summary( avatar &you, const bool active_column,
        const int cursor, const bool examining )
{
    std::vector<trait_id> active;
    std::vector<trait_id> passive;
    mutation_accessibility_columns( you, active, passive );
    const mutation_tab_mode tab = active_column ? mutation_tab_mode::active :
                                  mutation_tab_mode::passive;
    return mutation_accessibility_summary( you, mutation_column( active, passive, tab ), tab, cursor,
                                           examining ? mutation_menu_mode::examining : mutation_menu_mode::activating );
}

void avatar::power_mutations()
{
    std::vector<trait_id> passive;
    std::vector<trait_id> active;
    for( std::pair<const trait_id, trait_data> &mut : cached_mutations ) {
        if( mut.second.corrupted > 0 ) {
            continue;
        }
        if( !mut.first->player_display ) {
            continue;
        }
        if( !mut.first->activated ) {
            passive.push_back( mut.first );
        } else {
            active.push_back( mut.first );
        }
        // New mutations are initialized with no key at all, so we have to do this here.
        if( mut.second.key == ' ' ) {
            for( const char &letter : mutation_chars ) {
                if( trait_by_invlet( letter ).is_null() && mut.first->activated ) {
                    mut.second.key = letter;
                    break;
                }
            }
        }
    }

    // maximal number of rows in both columns
    const int mutations_count = std::max( passive.size(), active.size() );
    const int TITLE_HEIGHT = 2;

    const int DESCRIPTION_HEIGHT = 5;
    // + lines with text in titlebar, local
    const int HEADER_LINE_Y = TITLE_HEIGHT + 1;
    const int list_start_y = HEADER_LINE_Y + 2;

    // Main window
    /** Total required height is:
    * top frame line:                                         + 1
    * height of title window:                                 + TITLE_HEIGHT
    * line after the title:                                   + 1
    * line with active/passive mutation captions:               + 1
    * height of the biggest list of active/passive mutations:   + mutations_count
    * line before mutation description:                         + 1
    * height of description window:                           + DESCRIPTION_HEIGHT
    * bottom frame line:                                      + 1
    * TOTAL: TITLE_HEIGHT + mutations_count + DESCRIPTION_HEIGHT + 5
    */

    int HEIGHT = 0;
    int WIDTH = 0;
    catacurses::window wBio;

    int DESCRIPTION_LINE_Y = 0;
    catacurses::window w_description;

    catacurses::window w_title;

    int second_column = 0;

    int scroll_position = 0;
    int examine_pos = 0;
    int cursor = 0;
    int max_scroll_position = 0;
    int list_height = 0;
    int half_list_view_location = 0;
    mutation_menu_mode menu_mode = mutation_menu_mode::activating;
    mutation_tab_mode tab_mode;
    if( !active.empty() ) {
        tab_mode = mutation_tab_mode::active;
    } else if( !passive.empty() ) {
        tab_mode = mutation_tab_mode::passive;
    } else {
        tab_mode = mutation_tab_mode::none;
    }

    const auto recalc_max_scroll_position = [&]() {
        list_height = ( menu_mode == mutation_menu_mode::examining ?
                        DESCRIPTION_LINE_Y : HEIGHT - 1 ) - list_start_y;
        max_scroll_position = mutations_count - list_height;
        half_list_view_location = list_height / 2;
        if( max_scroll_position < 0 ) {
            scroll_position = 0;
        } else if( scroll_position > max_scroll_position ) {
            scroll_position = max_scroll_position;
        }
    };

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        HEIGHT = std::min( TERMY, std::max( FULL_SCREEN_HEIGHT,
                                            TITLE_HEIGHT + mutations_count + DESCRIPTION_HEIGHT + 5 ) );
        WIDTH = FULL_SCREEN_WIDTH + ( TERMX - FULL_SCREEN_WIDTH ) / 2;
        const point START( ( TERMX - WIDTH ) / 2, ( TERMY - HEIGHT ) / 2 );
        wBio = catacurses::newwin( HEIGHT, WIDTH, START );

        // Description window @ the bottom of the bionic window
        const int DESCRIPTION_START_Y = START.y + HEIGHT - DESCRIPTION_HEIGHT - 1;
        DESCRIPTION_LINE_Y = DESCRIPTION_START_Y - START.y - 1;
        w_description = catacurses::newwin( DESCRIPTION_HEIGHT, WIDTH - 2,
                                            point( START.x + 1, DESCRIPTION_START_Y ) );

        // Title window
        const int TITLE_START_Y = START.y + 1;
        w_title = catacurses::newwin( TITLE_HEIGHT, WIDTH - 2,
                                      point( START.x + 1, TITLE_START_Y ) );

        recalc_max_scroll_position();
        examine_pos = 0;

        // X-coordinate of the list of active mutations
        second_column = 32 + ( TERMX - FULL_SCREEN_WIDTH ) / 4;

        ui.position_from_window( wBio );
    } );
    ui.mark_resize();

    input_context ctxt( "MUTATIONS", keyboard_mode::keychar );
    ctxt.register_updown();
    ctxt.register_action( "ANY_INPUT" );
    ctxt.register_action( "TOGGLE_EXAMINE" );
    ctxt.register_action( "TOGGLE_SPRITE" );
    ctxt.register_action( "REASSIGN" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "SCROLL_TRAIT_INFO_UP" );
    ctxt.register_action( "SCROLL_TRAIT_INFO_DOWN" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "ACCESSIBILITY_DETAILS" );
    ctxt.register_action( "ACCESSIBILITY_STATUS" );
    ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
#if defined(__ANDROID__)
    for( const auto &p : passive ) {
        ctxt.register_manual_key( cached_mutations[p].key, p.obj().name() );
    }
    for( const auto &a : active ) {
        ctxt.register_manual_key( cached_mutations[a].key, a.obj().name() );
    }
#endif

    std::optional<trait_id> examine_id;

    const auto accessible_column = [&]() {
        return mutation_column( active, passive, tab_mode );
    };
    const auto accessible_focus = [&]() {
        return mutation_accessibility_focus( *this, accessible_column(), cursor );
    };
    const auto accessible_tab = [&]() {
        return mutation_accessibility_tab( *this, accessible_column(), tab_mode, cursor );
    };
    const auto accessible_mode = [&]() {
        return accessibility::format_power_menu_mode( mutation_accessibility_mode( menu_mode ) );
    };
    const auto accessible_reassigned = [&]( const trait_id & mut_id ) {
        return accessibility::format_semantic_sequence( { accessible_mode(),
                mutation_accessibility_row( *this, mut_id ), mutation_accessibility_key( *this, mut_id )
                                                        } );
    };
    const auto accessible_sprite = [&]( const trait_id & mut_id ) {
        return accessibility::format_semantic_sequence( {
            cached_mutations[mut_id].show_sprite ? _( "Sprite shown" ) : _( "Sprite hidden" ),
            mutation_accessibility_row( *this, mut_id )
        } );
    };
    accessibility::announce( accessibility::format_semantic_sequence( { _( "Mutations" ),
                             accessible_tab() } ) );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( wBio );
        draw_border( wBio, BORDER_COLOR, _( "Mutations" ) );
        wattron( wBio, BORDER_COLOR );
        // Draw line under title
        mvwhline( wBio, point( 1, HEADER_LINE_Y ), LINE_OXOX, WIDTH - 2 );
        // Draw symbols to connect additional lines to border
        mvwaddch( wBio, point( 0, HEADER_LINE_Y ), LINE_XXXO ); // |-
        mvwaddch( wBio, point( WIDTH - 1, HEADER_LINE_Y ), LINE_XOXX ); // -|
        wattroff( wBio, BORDER_COLOR );

        // Captions
        mvwprintz( wBio, point( 2, HEADER_LINE_Y + 1 ), c_light_blue, _( "Passive:" ) );
        mvwprintz( wBio, point( second_column, HEADER_LINE_Y + 1 ), c_light_blue, _( "Active:" ) );

        if( menu_mode == mutation_menu_mode::examining ) {
            draw_exam_window( wBio, DESCRIPTION_LINE_Y );
        }
        nc_color type;
        if( passive.empty() ) {
            mvwprintz( wBio, point( 2, list_start_y ), c_light_gray, _( "None" ) );
        } else {
            for( int i = scroll_position; static_cast<size_t>( i ) < passive.size(); i++ ) {
                const mutation_branch &md = passive[i].obj();
                const trait_data &td = cached_mutations[passive[i]];
                const bool is_highlighted = cursor == static_cast<int>( i );
                if( i - scroll_position == list_height ) {
                    break;
                }
                if( is_highlighted && tab_mode == mutation_tab_mode::passive ) {
                    type = has_base_trait( passive[i] ) ? c_cyan_yellow : c_light_cyan_yellow;
                } else {
                    type = has_base_trait( passive[i] ) ? c_cyan : c_light_cyan;
                }
                mvwprintz( wBio, point( 2, list_start_y + i - scroll_position ),
                           type, "%c %s", td.key, mutation_name( md.id ) );
                if( !td.show_sprite ) {
                    //~ Hint: Letter to show which mutation is Hidden in the mutation menu
                    wprintz( wBio, c_cyan, _( " H" ) );
                }
            }
        }

        if( active.empty() ) {
            mvwprintz( wBio, point( second_column, list_start_y ), c_light_gray, _( "None" ) );
        } else {
            for( int i = scroll_position; static_cast<size_t>( i ) < active.size(); i++ ) {
                const mutation_branch &md = active[i].obj();
                const trait_data &td = cached_mutations[active[i]];
                const bool is_highlighted = cursor == static_cast<int>( i );
                if( i - scroll_position == list_height ) {
                    break;
                }
                if( td.powered ) {
                    if( is_highlighted && tab_mode == mutation_tab_mode::active ) {
                        type = has_base_trait( active[i] ) ? c_green_yellow : c_light_green_yellow;
                    } else {
                        type = has_base_trait( active[i] ) ? c_green : c_light_green;
                    }
                } else {
                    if( is_highlighted && tab_mode == mutation_tab_mode::active ) {
                        type = has_base_trait( active[i] ) ? c_red_yellow : c_light_red_yellow;
                    } else {
                        type = has_base_trait( active[i] ) ? c_red : c_light_red;
                    }
                }
                // TODO: track resource(s) used and specify
                mvwputch( wBio, point( second_column, list_start_y + i - scroll_position ),
                          type, td.key );
                std::string mut_desc;
                std::string resource_unit;
                int number_of_resource = 0;
                if( md.hunger ) {
                    resource_unit += _( " kcal" );
                    number_of_resource++;
                }
                if( md.thirst ) {
                    if( number_of_resource > 0 ) {
                        //~ Resources consumed by a mutation: "kcal & thirst & sleepiness & mana"
                        resource_unit += _( " &" );
                    }
                    resource_unit += _( " thirst" );
                    number_of_resource++;
                }
                if( md.sleepiness ) {
                    if( number_of_resource > 0 ) {
                        //~ Resources consumed by a mutation: "kcal & thirst & sleepiness & mana"
                        resource_unit += _( " &" );
                    }
                    resource_unit += _( " sleepiness" );
                }
                if( md.mana ) {
                    resource_unit += _( " mana" );
                    number_of_resource++;
                }
                mut_desc += mutation_name( md.id );
                if( md.cost > 0 && md.cooldown > 0_turns ) {
                    mut_desc += string_format( _( " - %d%s / %s" ),
                                               md.cost, resource_unit, to_string_clipped( md.cooldown ) );
                } else if( md.cost > 0 ) {
                    mut_desc += string_format( _( " - %d%s" ), md.cost, resource_unit );
                } else if( md.cooldown > 0_turns ) {
                    mut_desc += string_format( _( " - %s" ), to_string_clipped( md.cooldown ) );
                }
                if( td.powered ) {
                    mut_desc += _( " - Active" );
                }
                mvwprintz( wBio, point( second_column + 2, list_start_y + i - scroll_position ),
                           type, mut_desc );
            }
        }

        draw_scrollbar( wBio, scroll_position, list_height, mutations_count,
                        point( 0, list_start_y ), c_white, true );

        if( menu_mode == mutation_menu_mode::examining && examine_id.has_value() ) {
            werase( w_description );
            std::string description = mutation_desc( examine_id.value() );
            if( !purifiable( examine_id.value() ) ) {
                description +=
                    _( "\n<color_yellow>This trait is an intrinsic part of you now, purifier won't be able to remove it.</color>" );
            }
            std::vector<std::string> desc = foldstring( description, WIDTH - 2 );
            const int winh = catacurses::getmaxy( w_description );
            const bool do_scroll = desc.size() > static_cast<unsigned>( std::abs( winh ) );
            const int fline = do_scroll ? examine_pos % ( desc.size() + 1 - winh ) : 0;
            const int lline = do_scroll ? fline + winh : desc.size();
            for( int i = fline; i < lline; i++ ) {
                trim_and_print( w_description, point( 0, i - fline ), WIDTH - 2, c_light_blue, desc[i] );
            }
            draw_scrollbar( wBio, fline, winh, desc.size(), point( 0, catacurses::getmaxy( wBio ) - winh - 1 ),
                            c_white, true );
        }
        wnoutrefresh( wBio );
        show_mutations_titlebar( w_title, menu_mode, ctxt );
        if( menu_mode == mutation_menu_mode::examining && examine_id.has_value() ) {
            wnoutrefresh( w_description );
        }
    } );

    bool exit = false;
    while( !exit ) {
        recalc_max_scroll_position();
        ui_manager::redraw();
        bool handled = false;
        const std::string action = ctxt.handle_input();
        const input_event evt = ctxt.get_raw_input();
        // Screen reader speech for this key, spoken however the iteration ends.
        std::string speech;
        const on_out_of_scope speak_result( [&speech]() {
            accessibility::announce( speech );
        } );
        const int previous_cursor = cursor;
        if( action == "ACCESSIBILITY_REPEAT_LAST" ) {
            accessibility::repeat_last_announcement();
            continue;
        } else if( action == "ACCESSIBILITY_STATUS" ) {
            speech = mutation_accessibility_summary( *this, accessible_column(), tab_mode, cursor,
                     menu_mode );
            continue;
        } else if( action == "ACCESSIBILITY_DETAILS" ) {
            if( accessibility::is_active() ) {
                std::optional<trait_id> detail_id = examine_id;
                if( menu_mode != mutation_menu_mode::examining || !detail_id.has_value() ) {
                    detail_id = std::nullopt;
                    const std::vector<trait_id> *column = accessible_column();
                    if( column != nullptr && cursor >= 0 && static_cast<size_t>( cursor ) < column->size() ) {
                        detail_id = ( *column )[cursor];
                    }
                }
                if( detail_id.has_value() ) {
                    const std::vector<std::string> paragraphs = mutation_accessibility_details( *this, active,
                            passive, detail_id.value() );
                    const std::string title = mutation_name( detail_id.value() );
                    accessibility_ui::show_document( title, title, paragraphs, paragraphs );
                    speech = mutation_accessibility_row( *this, detail_id.value() );
                } else {
                    speech = accessible_tab();
                }
            }
            continue;
        }
        if( evt.type == input_event_t::keyboard_char && !evt.sequence.empty() ) {
            const int ch = evt.get_first_input();
            if( ch == ' ' ) { //skip if space is pressed (space is used as an empty hotkey)
                continue;
            }
            const trait_id mut_id = trait_by_invlet( ch );
            if( !mut_id.is_null() ) {
                const mutation_branch &mut_data = mut_id.obj();
                switch( menu_mode ) {
                    case mutation_menu_mode::reassigning: {
                        query_popup pop;
                        pop.message( _( "%s; enter new letter." ), mutation_name( mut_id ) )
                        .preferred_keyboard_mode( keyboard_mode::keychar )
                        .context( "POPUP_WAIT" )
                        .allow_cancel( true )
                        .allow_anykey( true );

                        bool pop_exit = false;
                        while( !pop_exit ) {
                            const query_popup::result ret = pop.query();
                            bool pop_handled = false;
                            if( ret.evt.type == input_event_t::keyboard_char && !ret.evt.sequence.empty() ) {
                                const int newch = ret.evt.get_first_input();
                                if( mutation_chars.valid( newch ) ) {
                                    const trait_id other_mut_id = trait_by_invlet( newch );
                                    if( !other_mut_id.is_null() ) {
                                        std::swap( cached_mutations[mut_id].key, cached_mutations[other_mut_id].key );
                                    } else {
                                        cached_mutations[mut_id].key = newch;
                                    }
                                    pop_exit = true;
                                    pop_handled = true;
                                }
                            }
                            if( !pop_handled ) {
                                if( ret.action == "QUIT" ) {
                                    pop_exit = true;
                                } else if( ret.action != "HELP_KEYBINDINGS" &&
                                           ret.evt.type == input_event_t::keyboard_char ) {
                                    popup( _( "Invalid mutation letter.  Only those characters are valid:\n\n%s" ),
                                           mutation_chars.get_allowed_chars() );
                                }
                            }
                        }

                        menu_mode = mutation_menu_mode::activating;
                        examine_id = std::nullopt;
                        speech = accessible_reassigned( mut_id );
                        // TODO: show a message like when reassigning a key to an item?
                        break;
                    }
                    case mutation_menu_mode::activating: {
                        if( mut_data.activated ) {
                            if( cached_mutations[mut_id].powered ) {
                                add_msg_if_player( m_neutral, _( "You stop using your %s." ), mutation_name( mut_data.id ) );
                                // Reset menu in advance
                                ui.reset();
                                deactivate_mutation( mut_id );
                                // Action done, leave screen
                                exit = true;
                            } else if( mutation_resources_available( *this, mut_data ) ) {
                                add_msg_if_player( m_neutral,
                                                   string_format( mut_data.activation_msg, mutation_name( mut_data.id ) ) );
                                // Reset menu in advance
                                ui.reset();
                                activate_mutation( mut_id );
                                // Action done, leave screen
                                exit = true;
                            } else {
                                popup( _( "You don't have enough in you to activate your %s!" ), mutation_name( mut_data.id ) );
                                speech = accessible_focus();
                            }
                        } else {
                            popup( _( "You cannot activate %1$s!  To read a description of "
                                      "%1$s, press '%2$s', then '%3$c'." ),
                                   mutation_name( mut_data.id ), ctxt.get_desc( "TOGGLE_EXAMINE" ), cached_mutations[mut_id].key );
                            speech = accessible_focus();
                        }
                        break;
                    }
                    case mutation_menu_mode::examining:
                        // Describing mutations, not activating them!
                        examine_id = mut_id;
                        speech = mutation_accessibility_row( *this, mut_id );
                        break;
                    case mutation_menu_mode::hiding:
                        cached_mutations[mut_id].show_sprite = !cached_mutations[mut_id].show_sprite;
                        speech = accessible_sprite( mut_id );
                        break;
                }
                handled = true;
            } else if( mutation_chars.valid( ch ) ) {
                handled = true;
            }
        }
        if( !handled ) {

            // Essentially, up-down navigation adapted from the bionics_ui.cpp, with a bunch of extra workarounds to keep functionality

            if( action == "DOWN" ) {

                int lowerlim;

                if( tab_mode == mutation_tab_mode::passive ) {
                    lowerlim = static_cast<int>( passive.size() ) - 1;
                } else if( tab_mode == mutation_tab_mode::active ) {
                    lowerlim = static_cast<int>( active.size() ) - 1;
                } else {
                    continue;
                }

                if( cursor < lowerlim ) {
                    cursor++;
                } else {
                    cursor = 0;
                }
                if( scroll_position < max_scroll_position &&
                    cursor - scroll_position > list_height - half_list_view_location ) {
                    scroll_position++;
                }
                if( scroll_position > 0 && cursor - scroll_position < half_list_view_location ) {
                    scroll_position = std::max( cursor - half_list_view_location, 0 );
                }
                examine_pos = 0;

                // Draw the description, shabby workaround
                examine_id = GetTrait( active, passive, cursor, tab_mode );
                speech = accessibility::format_semantic_sequence( {
                    accessibility::format_list_wrap( previous_cursor, cursor, 1 ), accessible_focus()
                } );

            } else if( action == "UP" ) {

                int lim;
                if( tab_mode == mutation_tab_mode::passive ) {
                    lim = passive.size() - 1;
                } else if( tab_mode == mutation_tab_mode::active ) {
                    lim = active.size() - 1;
                } else {
                    continue;
                }
                if( cursor > 0 ) {
                    cursor--;
                } else {
                    cursor = lim;
                }
                if( scroll_position > 0 && cursor - scroll_position < half_list_view_location ) {
                    scroll_position--;
                }
                if( scroll_position < max_scroll_position &&
                    cursor - scroll_position > list_height - half_list_view_location ) {
                    scroll_position =
                        std::max( std::min<int>( lim + 1 - list_height,
                                                 cursor - half_list_view_location ), 0 );
                }
                examine_pos = 0;

                examine_id = GetTrait( active, passive, cursor, tab_mode );
                speech = accessibility::format_semantic_sequence( {
                    accessibility::format_list_wrap( previous_cursor, cursor, -1 ), accessible_focus()
                } );
            } else if( ( action == "SCROLL_TRAIT_INFO_UP" || action == "SCROLL_TRAIT_INFO_DOWN" ) &&
                       menu_mode == mutation_menu_mode::examining ) {
                examine_pos += action == "SCROLL_TRAIT_INFO_UP" ? -1 : 1;
            } else if( action == "NEXT_TAB" || action == "PREV_TAB" ) {
                if( tab_mode == mutation_tab_mode::active && !passive.empty() ) {
                    tab_mode = mutation_tab_mode::passive;
                } else if( tab_mode == mutation_tab_mode::passive && !active.empty() ) {
                    tab_mode = mutation_tab_mode::active;
                } else {
                    // Nothing to switch to; the column stays.
                    speech = accessible_tab();
                    continue;
                }
                scroll_position = 0;
                examine_pos = 0;
                cursor = 0;
                examine_id = GetTrait( active, passive, cursor, tab_mode );
                speech = accessible_tab();
            } else if( action == "CONFIRM" ) {
                trait_id mut_id;
                if( tab_mode == mutation_tab_mode::active ) {
                    mut_id = active[cursor];
                } else if( tab_mode == mutation_tab_mode::passive ) {
                    mut_id = passive[cursor];
                } else {
                    continue;
                }
                if( !mut_id.is_null() ) {
                    const mutation_branch &mut_data = mut_id.obj();
                    switch( menu_mode ) {
                        case mutation_menu_mode::reassigning: {
                            query_popup pop;
                            pop.message( _( "%s; enter new letter." ), mutation_name( mut_id ) )
                            .preferred_keyboard_mode( keyboard_mode::keychar )
                            .context( "POPUP_WAIT" )
                            .allow_cancel( true )
                            .allow_anykey( true );

                            bool pop_exit = false;
                            while( !pop_exit ) {
                                const query_popup::result ret = pop.query();
                                bool pop_handled = false;
                                if( ret.evt.type == input_event_t::keyboard_char && !ret.evt.sequence.empty() ) {
                                    const int newch = ret.evt.get_first_input();
                                    if( mutation_chars.valid( newch ) ) {
                                        const trait_id other_mut_id = trait_by_invlet( newch );
                                        if( !other_mut_id.is_null() ) {
                                            std::swap( cached_mutations[mut_id].key, cached_mutations[other_mut_id].key );
                                        } else {
                                            cached_mutations[mut_id].key = newch;
                                        }
                                        pop_exit = true;
                                        pop_handled = true;
                                    } else if( newch == ' ' ) {
                                        cached_mutations[mut_id].key = newch;
                                        pop_exit = true;
                                        pop_handled = true;
                                    }
                                }
                                if( !pop_handled ) {
                                    if( ret.action == "QUIT" ) {
                                        pop_exit = true;
                                    } else if( ret.action != "HELP_KEYBINDINGS" &&
                                               ret.evt.type == input_event_t::keyboard_char ) {
                                        popup( _( "Invalid mutation letter.  Only those characters are valid:\n\n%s" ),
                                               mutation_chars.get_allowed_chars() );
                                    }
                                }
                            }

                            menu_mode = mutation_menu_mode::activating;
                            examine_id = std::nullopt;
                            speech = accessible_reassigned( mut_id );
                            // TODO: show a message like when reassigning a key to an item?
                            break;
                        }
                        case mutation_menu_mode::activating: {
                            if( mut_data.activated ) {
                                if( cached_mutations[mut_id].powered ) {
                                    add_msg_if_player( m_neutral, _( "You stop using your %s." ), mutation_name( mut_data.id ) );
                                    // Reset menu in advance
                                    ui.reset();
                                    deactivate_mutation( mut_id );
                                    // Action done, leave screen
                                    exit = true;
                                } else if( mutation_resources_available( *this, mut_data ) ) {
                                    add_msg_if_player( m_neutral,
                                                       string_format( mut_data.activation_msg, mutation_name( mut_data.id ) ) );
                                    // Reset menu in advance
                                    ui.reset();
                                    activate_mutation( mut_id );
                                    // Action done, leave screen
                                    exit = true;
                                } else {
                                    popup( _( "You don't have enough in you to activate your %s!" ), mutation_name( mut_data.id ) );
                                    speech = accessible_focus();
                                }
                            } else {
                                popup( _( "You cannot activate %1$s!  To read a description of "
                                          "%1$s, press '%2$s'." ),
                                       mutation_name( mut_data.id ), ctxt.get_desc( "TOGGLE_EXAMINE" ) );
                                speech = accessible_focus();
                            }
                            break;
                        }
                        case mutation_menu_mode::examining:
                            // Describing mutations, not activating them!
                            examine_id = mut_id;
                            speech = mutation_accessibility_row( *this, mut_id );
                            break;
                        case mutation_menu_mode::hiding:
                            cached_mutations[mut_id].show_sprite = !cached_mutations[mut_id].show_sprite;
                            speech = accessible_sprite( mut_id );
                            break;
                    }
                }
            } else if( action == "REASSIGN" ) {
                menu_mode = mutation_menu_mode::reassigning;
                examine_id = std::nullopt;
                speech = accessible_mode();
            } else if( action == "TOGGLE_EXAMINE" ) {
                // switches between activation and examination
                menu_mode = menu_mode == mutation_menu_mode::activating ?
                            mutation_menu_mode::examining : mutation_menu_mode::activating;
                if( menu_mode == mutation_menu_mode::examining ) {
                    examine_id = GetTrait( active, passive, cursor, tab_mode );
                } else {
                    examine_id = std::nullopt;
                }
                examine_pos = 0;
                speech = accessible_mode();
            } else if( action == "TOGGLE_SPRITE" ) {
                menu_mode = mutation_menu_mode::hiding;
                examine_id = std::nullopt;
                speech = accessible_mode();
            } else if( action == "QUIT" ) {
                exit = true;
            }
        }
    }
}
