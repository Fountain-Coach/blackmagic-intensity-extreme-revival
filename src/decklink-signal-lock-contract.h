// Fountain Coach: platform-independent DeckLink receiver lock classifier.
// This represents only the bmdDeckLinkStatusVideoInputSignalLocked flag.
// Automatic mode detection and valid input-frame pixels are separate signals.
// No vendor APIs, device registers, graphics APIs, display modes or callbacks.
// Copyright (c) 2026 Fountain Coach contributors. MIT.
#ifndef FC_DECKLINK_SIGNAL_LOCK_CONTRACT_H
#define FC_DECKLINK_SIGNAL_LOCK_CONTRACT_H

namespace fc_decklink {
enum class SignalLockResult {
    UnknownStatus = -1,
    NoLock = 0,
    LockCandidate = 1
};

inline SignalLockResult classify_signal_lock(
    unsigned observations,
    unsigned status_queries_succeeded,
    unsigned locked_observations)
{
    if (observations == 0 || status_queries_succeeded != observations ||
        locked_observations > status_queries_succeeded) {
        return SignalLockResult::UnknownStatus;
    }
    if (locked_observations >= 2)
        return SignalLockResult::LockCandidate;
    return SignalLockResult::NoLock;
}
}
#endif
