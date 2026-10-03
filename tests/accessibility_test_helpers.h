#pragma once
#ifndef CATA_TESTS_ACCESSIBILITY_TEST_HELPERS_H
#define CATA_TESTS_ACCESSIBILITY_TEST_HELPERS_H

#include <string>
#include <vector>

#include "accessibility.h"

/** Accessibility backend that records announcements and cancellations for assertions. */
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

        void cancel() override {
            ++cancels;
        }

        std::vector<std::string> messages;
        int cancels = 0;
};

#endif // CATA_TESTS_ACCESSIBILITY_TEST_HELPERS_H
