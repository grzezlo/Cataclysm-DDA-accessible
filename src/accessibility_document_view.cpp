#include "accessibility_document_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "accessibility.h"
#include "accessibility_format.h"
#include "cata_assert.h"
#include "color.h"
#include "cursesdef.h"
#include "input_context.h"
#include "output.h"
#include "point.h"
#include "ui_manager.h"

namespace accessibility_ui
{

void show_text_document( const std::string &title, const std::string &text )
{
    const std::vector<std::string> paragraphs = accessibility::format_document_paragraphs( text );
    show_document( title, title, paragraphs, paragraphs );
}

void show_document( const std::string &window_title, const std::string &spoken_title,
                    const std::vector<std::string> &visual_paragraphs,
                    const std::vector<std::string> &spoken_paragraphs )
{
    cata_assert( visual_paragraphs.size() == spoken_paragraphs.size() );

    catacurses::window w;
    int width = 0;
    int height = 0;
    int text_w = 0;
    int text_h = 0;
    std::vector<std::string> folded_lines;
    std::vector<int> para_line_starts;
    int beg_line = 0;
    int max_beg_line = 0;
    int current_para = 0;
    const int num_paragraphs = static_cast<int>( spoken_paragraphs.size() );

    ui_adaptor ui;
    const auto screen_resize_cb = [&]( ui_adaptor & ui ) {
        w = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                point( TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0,
                                       TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0 ) );
        width = getmaxx( w );
        height = getmaxy( w );
        text_w = std::max( 0, width - 2 );
        text_h = std::max( 0, height - 2 );

        folded_lines.clear();
        para_line_starts.clear();
        para_line_starts.reserve( visual_paragraphs.size() );

        for( size_t i = 0; i < visual_paragraphs.size(); ++i ) {
            para_line_starts.push_back( static_cast<int>( folded_lines.size() ) );
            std::vector<std::string> folded = foldstring( visual_paragraphs[i], text_w );
            folded_lines.insert( folded_lines.end(), folded.begin(), folded.end() );
            if( i + 1 < visual_paragraphs.size() ) {
                folded_lines.emplace_back();
            }
        }
        max_beg_line = std::max( 0, static_cast<int>( folded_lines.size() ) - text_h );
        if( current_para >= 0 && current_para < static_cast<int>( para_line_starts.size() ) ) {
            beg_line = std::min( para_line_starts[current_para], max_beg_line );
        }

        ui.position_from_window( w );
    };
    screen_resize_cb( ui );
    ui.on_screen_resize( screen_resize_cb );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w );
        draw_border( w, BORDER_COLOR, window_title );
        for( int line = beg_line, pos_y = 1;
             line < std::min<int>( beg_line + text_h, folded_lines.size() );
             ++line, ++pos_y ) {
            nc_color dummy = c_white;
            print_colored_text( w, point( 1, pos_y ), dummy, dummy, folded_lines[line] );
        }
        scrollbar().offset_x( width - 1 ).offset_y( 1 ).content_size( folded_lines.size() )
        .viewport_pos( std::min( beg_line, max_beg_line ) ).viewport_size( text_h ).apply( w );
        wnoutrefresh( w );
    } );

    input_context ctxt( "HELP_DOCUMENT" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "HOME" );
    ctxt.register_action( "END" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "ACCESSIBILITY_REPEAT_LAST" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "SCROLL_UP" );
    ctxt.register_action( "SCROLL_DOWN" );

    if( num_paragraphs > 0 ) {
        accessibility::announce( accessibility::format_document_focus(
                                     spoken_title, spoken_paragraphs[0] ) );
    }

    std::string action;
    do {
        ui_manager::redraw();
        action = ctxt.handle_input();

        const auto update_view_to_para = [&]() {
            if( current_para >= 0 && current_para < static_cast<int>( para_line_starts.size() ) ) {
                beg_line = std::min( para_line_starts[current_para], max_beg_line );
            }
        };
        const auto move_by = [&]( const int delta ) {
            const int next = accessibility::move_document_cursor( current_para, num_paragraphs, delta );
            if( next != current_para && num_paragraphs > 0 ) {
                current_para = next;
                update_view_to_para();
                accessibility::announce( spoken_paragraphs[current_para] );
            }
        };

        if( action == "DOWN" || action == "SCROLL_DOWN" ) {
            move_by( 1 );
        } else if( action == "UP" || action == "SCROLL_UP" ) {
            move_by( -1 );
        } else if( action == "PAGE_DOWN" ) {
            move_by( 3 );
        } else if( action == "PAGE_UP" ) {
            move_by( -3 );
        } else if( action == "HOME" ) {
            move_by( -current_para );
        } else if( action == "END" ) {
            move_by( num_paragraphs - 1 - current_para );
        } else if( action == "ACCESSIBILITY_REPEAT_LAST" ) {
            accessibility::repeat_last_announcement();
        } else if( action == "HELP_KEYBINDINGS" && num_paragraphs > 0 ) {
            accessibility::announce( spoken_paragraphs[current_para] );
        }
    } while( action != "CONFIRM" && action != "QUIT" );
}

} // namespace accessibility_ui
