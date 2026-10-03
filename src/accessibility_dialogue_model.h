#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_DIALOGUE_MODEL_H
#define CATA_SRC_ACCESSIBILITY_DIALOGUE_MODEL_H

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace accessibility
{

/**
 * Screen reader model of the dialogue window.  The conversation level is a
 * history of utterances ending in the response prompt; Enter on the prompt
 * opens the response list, and Escape returns to the prompt.
 */
class dialogue_speech_model
{
    public:
        struct option {
            /** Row spoken on focus. */
            std::string spoken;
            /** Input action of an extra screen action; empty for a response. */
            std::string action;
        };
        struct result {
            /** The window must ignore the action. */
            bool consumed = false;
            /** Action the window handles instead of the input, when not consumed. */
            std::string action;
            std::string speech;
            /** Response the visible cursor moves to; negative to leave it. */
            int response_cursor = -1;
            /** Text to open in the document view. */
            std::string document;
        };

        /** Window name for the summary, such as "Dialogue: Rubik". */
        void set_title( const std::string &title );
        /** Prompt label, such as "Your response". */
        void set_prompt_label( const std::string &label );
        /** Add a history element; the player's own lines are not spoken automatically. */
        void add_utterance( std::string_view text, bool own );
        /**
         * Replace the response list.  The first `response_count` options are
         * responses; the rest are extra actions.  `cursor` is the visible response.
         */
        void set_options( const std::vector<option> &new_options, size_t responses, int cursor );
        /**
         * Speech for a newly presented topic: the utterances not yet spoken,
         * then the prompt, which receives focus on the conversation level.
         */
        std::string begin_topic();
        result handle( const std::string &action );
        /** The focused element, spoken again after a declined confirmation. */
        std::string focus() const;
        /** The row of a response that cannot be chosen. */
        std::string option_row( size_t index ) const;
        bool in_response_list() const {
            return responses_open;
        }

    private:
        std::string prompt() const;
        std::string summary() const;
        std::string title;
        std::string prompt_label;
        std::vector<std::pair<std::string, bool>> history;
        size_t first_unspoken = 0;
        std::vector<option> options;
        size_t response_count = 0;
        /** History index; history.size() is the prompt. */
        size_t history_focus = 0;
        int option_focus = 0;
        bool responses_open = false;
};

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_DIALOGUE_MODEL_H
