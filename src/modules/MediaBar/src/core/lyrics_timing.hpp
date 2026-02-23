#pragma once

// MediaBar: lyrics timing manages core logic and state.

#include "types.hpp"

namespace mediabar {

int findCurrentLyricLine(const LyricsList& lyrics, qint64 progressMs);

}
