#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_DOCUMENT_VIEW_H
#define CATA_SRC_ACCESSIBILITY_DOCUMENT_VIEW_H

#include <string>
#include <vector>

namespace accessibility_ui
{

/** Display and speak source paragraphs without splitting speech at visual wraps. */
void show_document( const std::string &window_title, const std::string &spoken_title,
                    const std::vector<std::string> &visual_paragraphs,
                    const std::vector<std::string> &spoken_paragraphs );

/** Read a formatted message by source line and sentence, without retaining it after closure. */
void show_text_document( const std::string &title, const std::string &text );

} // namespace accessibility_ui

#endif // CATA_SRC_ACCESSIBILITY_DOCUMENT_VIEW_H
