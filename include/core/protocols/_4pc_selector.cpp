// Print a warning based on 4PC protocol

#include "../../debug/orq_debug.h"

#ifdef MPC_PROTOCOL_FANTASTIC_FOUR
#ifdef USE_DALSKOV_FANTASTIC_FOUR
#warning "Note: using Dalskov 4PC protocol."
#else
#warning "Note: using custom 4PC protocol."
#endif
#endif