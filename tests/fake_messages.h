#pragma once
#ifndef CATA_TESTS_FAKE_MESSAGES_H
#define CATA_TESTS_FAKE_MESSAGES_H

#include <functional>
#include <string>

/** Opt-in observation of non-debug handler messages in integration tests. */
void set_test_message_observer( std::function<void( const std::string & )> observer );

#endif // CATA_TESTS_FAKE_MESSAGES_H
