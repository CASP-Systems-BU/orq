#pragma once
// macOS <sys/termios.h> #defines B0 as 0. That macro collides with C++
// identifiers named B0. Leaving B0 undefined breaks Boost.Asio, which uses
// the baud-rate macro in serial_port_base.ipp (case B0 / baud = B0).
// Replace the macro with an enumerator whose value is still 0, so both
// uses compile: declarations named B0 stay declarations, and Asio's
// constant expressions still evaluate to 0.
#include <termios.h>
#undef B0
namespace {
enum { orq_mac_termios_baud_B0 = 0 };
}
#define B0 orq_mac_termios_baud_B0
