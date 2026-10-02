#pragma once

// Shine is portable C but its upstream source uses GCC's attribute spelling.
// MSVC does not need this unused-variable annotation for the static encoder.
#ifndef __attribute__
#define __attribute__(ignored)
#endif
