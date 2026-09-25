#include "help.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "accessibility.h"
#include "accessibility_document_view.h"
#include "action.h"
#include "cata_path.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "color.h"
#include "cursesdef.h"
#include "debug.h"
#include "flexbuffer_json.h"
#include "input_context.h"
#include "input_enums.h"
#include "output.h"
#include "path_info.h"
#include "point.h"
#include "string_formatter.h"
#include "text_snippets.h"
#include "translations.h"
#include "ui_manager.h"

help &get_help()
{
    static help single_instance;
    return single_instance;
}

void help::load( const JsonObject &jo, const std::string &src )
{
    get_help().load_object( jo, src );
}

void help::reset()
{
    get_help().reset_instance();
}

void help::reset_instance()
{
    current_order_start = 0;
    current_src = "";
    help_texts.clear();
}

void help::load_object( const JsonObject &jo, const std::string &src )
{
    if( src == "dda" ) {
        jo.throw_error( string_format( "Vanilla help must be located in %s",
                                       PATH_INFO::jsondir().generic_u8string() ) );
    }
    if( src != current_src ) {
        current_order_start = help_texts.empty() ? 0 : help_texts.crbegin()->first + 1;
        current_src = src;
    }
    std::vector<translation> messages;
    jo.read( "messages", messages );

    translation name;
    jo.read( "name", name );
    const int modified_order = jo.get_int( "order" ) + current_order_start;
    if( !help_texts.try_emplace( modified_order, std::make_pair( name, messages ) ).second ) {
        jo.throw_error_at( "order", "\"order\" must be unique (per src)" );
    }
}

std::string help::get_dir_grid()
{
    static const std::array<action_id, 9> movearray = {{
            ACTION_MOVE_FORTH_LEFT, ACTION_MOVE_FORTH, ACTION_MOVE_FORTH_RIGHT,
            ACTION_MOVE_LEFT,  ACTION_PAUSE,  ACTION_MOVE_RIGHT,
            ACTION_MOVE_BACK_LEFT, ACTION_MOVE_BACK, ACTION_MOVE_BACK_RIGHT
        }
    };

    std::string movement = "<LEFTUP_0>  <UP_0>  <RIGHTUP_0>   <LEFTUP_1>  <UP_1>  <RIGHTUP_1>\n"
                           " \\ | /     \\ | /\n"
                           "  \\|/       \\|/\n"
                           "<LEFT_0>--<pause_0>--<RIGHT_0>   <LEFT_1>--<pause_1>--<RIGHT_1>\n"
                           "  /|\\       /|\\\n"
                           " / | \\     / | \\\n"
                           "<LEFTDOWN_0>  <DOWN_0>  <RIGHTDOWN_0>   <LEFTDOWN_1>  <DOWN_1>  <RIGHTDOWN_1>";

    for( action_id dir : movearray ) {
        std::vector<input_event> keys = keys_bound_to( dir, /*maximum_modifier_count=*/0 );
        for( size_t i = 0; i < 2; i++ ) {
            movement = string_replace( movement, "<" + action_ident( dir ) + string_format( "_%d>", i ),
                                       i < keys.size()
                                       ? string_format( "<color_light_blue>%s</color>",
                                               keys[i].short_description() )
                                       : "<color_red>?</color>" );
        }
    }

    return movement;
}

std::string help::get_dir_grid_speech()
{
    static const std::array<std::pair<std::string, action_id>, 8> dir_actions = {{
            { translate_marker( "Northwest" ), ACTION_MOVE_FORTH_LEFT },
            { translate_marker( "North" ), ACTION_MOVE_FORTH },
            { translate_marker( "Northeast" ), ACTION_MOVE_FORTH_RIGHT },
            { translate_marker( "West" ), ACTION_MOVE_LEFT },
            { translate_marker( "East" ), ACTION_MOVE_RIGHT },
            { translate_marker( "Southwest" ), ACTION_MOVE_BACK_LEFT },
            { translate_marker( "South" ), ACTION_MOVE_BACK },
            { translate_marker( "Southeast" ), ACTION_MOVE_BACK_RIGHT }
        }
    };

    std::vector<std::pair<std::string, std::vector<std::string>>> bindings;
    bindings.reserve( 9 );

    const auto collect_keys = []( action_id act ) {
        std::vector<std::string> keys_desc;
        for( const input_event &key : keys_bound_to( act, /*maximum_modifier_count=*/0,
                /*restrict_to_printable=*/false ) ) {
            const std::string desc = key.short_description();
            if( !desc.empty() && std::find( keys_desc.begin(), keys_desc.end(), desc ) == keys_desc.end() ) {
                keys_desc.push_back( desc );
            }
        }
        return keys_desc;
    };

    for( size_t i = 0; i < 4; ++i ) {
        const auto &entry = dir_actions[i];
        bindings.emplace_back( _( entry.first ), collect_keys( entry.second ) );
    }

    bindings.emplace_back( _( "Pause" ), collect_keys( ACTION_PAUSE ) );

    for( size_t i = 4; i < 8; ++i ) {
        const auto &entry = dir_actions[i];
        bindings.emplace_back( _( entry.first ), collect_keys( entry.second ) );
    }

    return accessibility::format_help_directions( bindings );
}

std::map<int, inclusive_rectangle<point>> help::draw_menu( const catacurses::window &win,
                                       int selected, std::map<int, input_event> &hotkeys ) const
{
    std::map<int, inclusive_rectangle<point>> opt_map;

    werase( win );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    int y = fold_and_print( win, point( 1, 0 ), getmaxx( win ) - 2, c_white,
                            _( "Please press one of the following for help on that topic:\n"
                               "Press ESC to return to the game." ) ) + 1;

    size_t half_size = help_texts.size() / 2 + 1;
    int second_column = divide_round_up( getmaxx( win ), 2 );
    size_t i = 0;
    for( const auto &text : help_texts ) {
        std::string cat_name;
        auto hotkey_it = hotkeys.find( text.first );
        if( hotkey_it != hotkeys.end() ) {
            cat_name = colorize( hotkey_it->second.short_description(),
                                 selected == text.first ? hilite( c_light_blue ) : c_light_blue );
            cat_name += ": ";
        }
        cat_name += text.second.first.translated();
        const int cat_width = utf8_width( remove_color_tags( cat_name ) );
        if( i < half_size ) {
            second_column = std::max( second_column, cat_width + 4 );
        }

        const point sc_start( i < half_size ? 1 : second_column, y + i % half_size );
        fold_and_print( win, sc_start, getmaxx( win ) - 2,
                        selected == text.first ? hilite( c_white ) : c_white, cat_name );
        ++i;

        opt_map.emplace( text.first,
                         inclusive_rectangle<point>( sc_start, sc_start + point( cat_width - 1, 0 ) ) );
    }

    wnoutrefresh( win );

    return opt_map;
}

std::string help::get_note_colors()
{
    std::string text = _( "Note colors: " );
    for( const auto &color_pair : get_note_color_names() ) {
        // The color index is not translatable, but the name is.
        //~ %1$s: note color abbreviation, %2$s: note color name
        text += string_format( pgettext( "note color", "%1$s:%2$s, " ),
                               colorize( color_pair.first, color_pair.second.color ),
                               color_pair.second.name );
    }

    return text;
}

void help::display_topic( const std::string &topic_name,
                          const std::vector<std::string> &visual_paragraphs,
                          const std::vector<std::string> &spoken_paragraphs ) const
{
    accessibility_ui::show_document( string_format( _( "Help: %s" ), topic_name ), topic_name,
                                     visual_paragraphs, spoken_paragraphs );
}

void help::display_help() const
{
    catacurses::window w_help_border;
    catacurses::window w_help;

    ui_adaptor ui;
    const auto init_windows = [&]( ui_adaptor & ui ) {
        w_help_border = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                            point( TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0,
                                                    TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0 ) );
        w_help = catacurses::newwin( FULL_SCREEN_HEIGHT - 2, FULL_SCREEN_WIDTH - 2,
                                     point( 1 + static_cast<int>( TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0 ),
                                            1 + static_cast<int>( TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0 ) ) );
        ui.position_from_window( w_help_border );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    input_context ctxt( "DISPLAY_HELP", keyboard_mode::keychar );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    // for mouse selection
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "MOUSE_MOVE" );
    // for the menu shortcuts
    ctxt.register_action( "ANY_INPUT" );

    std::string action;
    std::map<int, inclusive_rectangle<point>> opt_map;

    const hotkey_queue &hkq = hotkey_queue::alphabets();
    input_event next_hotkey = ctxt.first_unassigned_hotkey( hkq );
    std::map<int, input_event> hotkeys;
    std::vector<int> topic_orders;
    topic_orders.reserve( help_texts.size() );
    for( const auto &text : help_texts ) {
        topic_orders.push_back( text.first );
        hotkeys.emplace( text.first, next_hotkey );
        next_hotkey = ctxt.next_unassigned_hotkey( hkq, next_hotkey );
    }

    int sel_index = topic_orders.empty() ? -1 : 0;
    int sel = sel_index >= 0 ? topic_orders[sel_index] : -1;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        draw_border( w_help_border, BORDER_COLOR, _( "Help" ) );
        wnoutrefresh( w_help_border );
        opt_map = draw_menu( w_help, sel, hotkeys );
    } );

    if( !topic_orders.empty() ) {
        const auto first_it = help_texts.find( topic_orders.front() );
        const std::string first_hotkey = hotkeys[topic_orders.front()].short_description();
        const std::string first_title = first_it != help_texts.end() ?
                                        first_it->second.first.translated() : std::string();
        accessibility::announce( accessibility::format_help_menu_open(
                                     topic_orders.size(), first_hotkey, first_title ) );
    }

    do {
        sel = sel_index >= 0 && sel_index < static_cast<int>( topic_orders.size() ) ?
              topic_orders[sel_index] : -1;
        ui_manager::redraw();

        action = ctxt.handle_input();
        input_event input = ctxt.get_raw_input();

        // Mouse selection
        if( action == "MOUSE_MOVE" || action == "SELECT" ) {
            std::optional<point> coord = ctxt.get_coordinates_text( w_help );
            if( !!coord ) {
                int mouse_sel = -1;
                int cnt = run_for_point_in<int, point>( opt_map, *coord,
                [&mouse_sel]( const std::pair<int, inclusive_rectangle<point>> &p ) {
                    mouse_sel = p.first;
                } );
                if( cnt > 0 ) {
                    for( size_t i = 0; i < topic_orders.size(); ++i ) {
                        if( topic_orders[i] == mouse_sel ) {
                            sel_index = static_cast<int>( i );
                            sel = mouse_sel;
                            break;
                        }
                    }
                    if( action == "SELECT" ) {
                        action = "CONFIRM";
                    }
                }
            }
        }

        const auto announce_current_topic = [&]() {
            if( sel_index >= 0 && sel_index < static_cast<int>( topic_orders.size() ) ) {
                const int order = topic_orders[sel_index];
                const auto it = help_texts.find( order );
                if( it != help_texts.end() ) {
                    accessibility::announce( accessibility::format_help_topic_focus(
                                                 hotkeys[order].short_description(),
                                                 it->second.first.translated() ) );
                }
            }
        };

        if( action == "UP" ) {
            if( !topic_orders.empty() ) {
                const int prev = sel_index;
                sel_index = std::max( 0, sel_index - 1 );
                if( sel_index != prev ) {
                    announce_current_topic();
                }
            }
        } else if( action == "DOWN" ) {
            if( !topic_orders.empty() ) {
                const int prev = sel_index;
                sel_index = std::min( static_cast<int>( topic_orders.size() ) - 1, sel_index + 1 );
                if( sel_index != prev ) {
                    announce_current_topic();
                }
            }
        } else if( action == "PAGE_UP" ) {
            if( !topic_orders.empty() ) {
                const int prev = sel_index;
                sel_index = std::max( 0, sel_index - 5 );
                if( sel_index != prev ) {
                    announce_current_topic();
                }
            }
        } else if( action == "PAGE_DOWN" ) {
            if( !topic_orders.empty() ) {
                const int prev = sel_index;
                sel_index = std::min( static_cast<int>( topic_orders.size() ) - 1, sel_index + 5 );
                if( sel_index != prev ) {
                    announce_current_topic();
                }
            }
        } else if( action == "ACCESSIBILITY_REPEAT_LAST" ) {
            accessibility::repeat_last_announcement();
        } else if( action != "QUIT" ) {
            int target_topic_order = -1;
            for( size_t i = 0; i < topic_orders.size(); ++i ) {
                const int order = topic_orders[i];
                if( input == hotkeys[order] ) {
                    sel_index = static_cast<int>( i );
                    target_topic_order = order;
                    break;
                }
            }
            if( target_topic_order == -1 && action == "CONFIRM" &&
                sel_index >= 0 && sel_index < static_cast<int>( topic_orders.size() ) ) {
                target_topic_order = topic_orders[sel_index];
            }

            if( target_topic_order != -1 ) {
                auto help_text_it = help_texts.find( target_topic_order );
                if( help_text_it != help_texts.end() ) {
                    std::vector<std::string> visual_paragraphs;
                    std::vector<std::string> spoken_paragraphs;
                    visual_paragraphs.reserve( help_text_it->second.second.size() );
                    spoken_paragraphs.reserve( help_text_it->second.second.size() );

                    for( const translation &line : help_text_it->second.second ) {
                        std::string line_proc = line.translated();
                        std::string line_spoken = line_proc;

                        if( line_proc == "<DRAW_NOTE_COLORS>" ) {
                            line_proc = get_note_colors();
                            line_spoken = remove_color_tags( line_proc );
                        } else if( line_proc == "<HELP_DRAW_DIRECTIONS>" ) {
                            line_proc = get_dir_grid();
                            line_spoken = get_dir_grid_speech();
                        } else {
                            size_t pos = line_proc.find( "<press_", 0, 7 );
                            while( pos != std::string::npos ) {
                                size_t pos2 = line_proc.find( ">", pos, 1 );
                                std::string action_str = line_proc.substr( pos + 7, pos2 - pos - 7 );
                                std::string replace = "<color_light_blue>" +
                                                      press_x( look_up_action( action_str ), "", "" ) + "</color>";
                                if( replace.empty() ) {
                                    debugmsg( "Help json: Unknown action: %s", action_str );
                                } else {
                                    line_proc = string_replace(
                                                    line_proc, "<press_" + action_str + ">", replace );
                                }
                                pos = line_proc.find( "<press_", pos2, 7 );
                            }
                            line_spoken = remove_color_tags( line_proc );
                        }

                        visual_paragraphs.push_back( std::move( line_proc ) );
                        spoken_paragraphs.push_back( std::move( line_spoken ) );
                    }

                    if( !visual_paragraphs.empty() ) {
                        ui.on_screen_resize( nullptr );
                        display_topic( help_text_it->second.first.translated(),
                                       visual_paragraphs, spoken_paragraphs );
                        ui.on_screen_resize( init_windows );

                        // Announce return to topic menu
                        accessibility::announce( accessibility::format_document_focus(
                                                     _( "Help" ),
                                                     accessibility::format_help_topic_focus(
                                                         hotkeys[target_topic_order].short_description(),
                                                         help_text_it->second.first.translated() ) ) );
                        action.clear();
                    }
                }
            }
        }
    } while( action != "QUIT" );
}

std::string get_hint()
{
    return SNIPPET.random_from_category( "hint" ).value_or( translation() ).translated();
}
