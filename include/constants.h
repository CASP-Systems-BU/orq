/**
 * @file constants.h
 *
 * Various constants to be used throughout the codebase.
 */

#pragma once

// Base port from which other correlation generator's ports are calculated
const int GILBOA_OLE_BASE_PORT = 8877;

// In the current implementation, each thread (and often each type) has its own port. These fields
// help us calculate starting port ranges for various generators.
const int MAX_POSSIBLE_THREADS = 256;
const int NUM_POSSIBLE_TYPES = 16;

// Which OT extension protocol we should use.
#ifdef USE_LIBOTE

#include "libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtSender.h"
#include "libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h"

namespace orq {
//// Asymptotically efficient silent OT, concretely slower on smaller inputs
// using OTRecvT = oc::SilentOtExtReceiver;
// using OTSendT = oc::SilentOtExtSender;

//// Concretely efficient OT
using OTRecvT = oc::SoftSpokenShOtReceiver<>;
using OTSendT = oc::SoftSpokenShOtSender<>;

}  // namespace orq
#endif