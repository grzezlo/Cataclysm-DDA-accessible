#include "ui_iteminfo.h"

#include <imgui/imgui.h>
#include <string>
#include <vector>

#include "accessibility.h"
#include "accessibility_format.h"
#include "accessibility_document_view.h"
#include "color.h"
#include "input_enums.h"
#include "translations.h"
#include "ui_manager.h"


iteminfo_window::iteminfo_window( item_info_data &info, point pos, int width, int height,
                                  ImGuiWindowFlags flags ) :
    cataimgui::window( _( "Item info" ), flags ),
    data( info ), pos( pos ), width( width ), height( height )
{
    ctxt = input_context( "default" );
    if( info.handle_scrolling ) {
        ctxt.register_action( "PAGE_UP" );
        ctxt.register_action( "PAGE_DOWN" );
        ctxt.register_action( "HOME" );
        ctxt.register_action( "END" );
        if( info.arrow_scrolling ) {
            ctxt.register_action( "UP" );
            ctxt.register_action( "DOWN" );
        }
    }
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    if( info.any_input ) {
        ctxt.register_action( "ANY_INPUT" );
    }

    ctxt.set_timeout( 10 );
}

cataimgui::bounds iteminfo_window::get_bounds()
{
    return { pos.x < 0 ? -1.f : static_cast<float>( str_width_to_pixels( pos.x ) ),
             pos.y < 0 ? -1.f : static_cast<float>( str_height_to_pixels( pos.y ) ),
             static_cast<float>( str_width_to_pixels( width ) ),
             static_cast<float>( str_height_to_pixels( height ) ) };
}

void iteminfo_window::draw_controls()
{
    cataimgui::set_scroll( s );

    nc_color base_color = c_light_gray;
    if( !data.get_item_name().empty() ) {
        cataimgui::draw_colored_text( data.get_item_name(), base_color );
        ImGui::Spacing();
    }
    if( !data.get_type_name().empty() &&
        data.get_item_name().find( data.get_type_name() ) == std::string::npos ) {
        cataimgui::draw_colored_text( data.get_type_name(), base_color );
    }

    display_item_info( data.get_item_display(), data.get_item_compare() );
}

void iteminfo_window::execute()
{
    if( data.without_getch ) {
        ui_manager::redraw();
        return;
    }

    if( accessibility::is_active() ) {
        const std::string raw_info = format_item_info( data.get_item_display(), data.get_item_compare() );
        const std::string clean_item_name = remove_color_tags( data.get_item_name() );
        const std::string clean_type_name = remove_color_tags( data.get_type_name() );
        const std::string type_name =
            ( !clean_type_name.empty() &&
              clean_item_name.find( clean_type_name ) == std::string::npos )
            ? clean_type_name : std::string();
        std::vector<std::string> paragraphs =
            accessibility::format_item_info_paragraphs( type_name, raw_info );
        if( paragraphs.empty() ) {
            paragraphs.emplace_back( _( "No details available." ) );
        }
        accessibility_ui::show_document( string_format( _( "Item info: %s" ), clean_item_name ),
                                         clean_item_name, paragraphs, paragraphs );
        return;
    }

    while( true ) {
        ui_manager::redraw();
        std::string action = ctxt.handle_input();

        if( data.handle_scrolling && data.arrow_scrolling && action == "UP" ) {
            s = cataimgui::scroll::line_up;
        } else if( data.handle_scrolling && data.arrow_scrolling && action == "DOWN" ) {
            s = cataimgui::scroll::line_down;
        } else if( data.handle_scrolling && action == "PAGE_UP" ) {
            s = cataimgui::scroll::page_up;
        } else if( data.handle_scrolling && action == "PAGE_DOWN" ) {
            s = cataimgui::scroll::page_down;
        } else if( data.handle_scrolling && action == "HOME" ) {
            s = cataimgui::scroll::begin;
        } else if( data.handle_scrolling && action == "END" ) {
            s = cataimgui::scroll::end;
        } else if( action == "CONFIRM" || action == "QUIT" ||
                   ( data.any_input && action == "ANY_INPUT" && !ctxt.get_raw_input().sequence.empty() ) ) {
            break;
        }

        // mouse click on x to close leads here
        if( !get_is_open() ) {
            break;
        }
    }
}
