#include "ChordDetector.h"

#include <algorithm>
#include <bitset>

namespace mc {

std::optional<Chord> ChordDetector::detect(const std::vector<int> &midiNotes) const
{
    if (midiNotes.empty())
        return std::nullopt;

    unsigned mask = 0;
    int lowest = 128;
    for (int n : midiNotes) {
        mask |= 1u << (n % 12);
        lowest = std::min(lowest, n);
    }
    if (std::bitset<12>(mask).count() < 2)
        return std::nullopt;
    const int bassPc = lowest % 12;

    double bestScore = -1e9;
    Chord best;
    for (int q = 0; q < static_cast<int>(Quality::Count); ++q) {
        const QualityInfo &info = qualityInfo(static_cast<Quality>(q));
        for (int root = 0; root < 12; ++root) {
            if (!(mask & (1u << root)))
                continue; // the root itself must be played

            unsigned tmask = 0;
            for (int iv : info.intervals)
                tmask |= 1u << ((root + iv) % 12);

            const int matched = std::bitset<12>(mask & tmask).count();
            const int extra = std::bitset<12>(mask & ~tmask & 0xfffu).count();
            double missingPenalty = 0.0;
            for (int pc = 0; pc < 12; ++pc) {
                if ((tmask & ~mask) & (1u << pc)) {
                    const int iv = (pc - root + 12) % 12;
                    missingPenalty += (iv == 7 && info.intervals.size() >= 3) ? 0.6 : 2.5;
                }
            }

            const double score = 2.0 * matched - 3.0 * extra - missingPenalty
                                 + (bassPc == root ? 1.0 : 0.0) - 0.05 * info.complexity;
            if (score > bestScore + 1e-9) {
                bestScore = score;
                best.root = root;
                best.quality = static_cast<Quality>(q);
            }
        }
    }

    if (!best.isValid())
        return std::nullopt;
    best.bass = bassPc != best.root ? bassPc : -1;
    return best;
}

} // namespace mc
