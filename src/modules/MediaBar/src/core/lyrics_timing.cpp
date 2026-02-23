#include "lyrics_timing.hpp"

// MediaBar: lyrics timing manages core logic and state.

namespace mediabar {

int findCurrentLyricLine(const LyricsList& lyrics, qint64 progressMs) {
    if (lyrics.isEmpty()) {
        return -1;
    }

    if (progressMs < lyrics.first().timeMs) {
        return -1;
    }

    int left = 0;
    int right = lyrics.size() - 1;
    int result = -1;

    while (left <= right) {
        const int mid = (left + right) / 2;
        if (lyrics[mid].timeMs <= progressMs) {
            result = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (result >= 0 && (result + 1) < lyrics.size()) {
        const auto& current = lyrics[result];
        const auto& next = lyrics[result + 1];
        constexpr qint64 kImmediateInstrumentalAdvanceMs = 150;

        if (!current.isInstrumental && next.isInstrumental && progressMs >= (current.timeMs + kImmediateInstrumentalAdvanceMs)) {
            return result + 1;
        }
    }

    return result;
}

}
