// Pure, cross-platform classification tests: do not link DeckLink or IOKit.
// Run: c++ -std=c++11 -Wall -Wextra -Werror tests/decklink_signal_lock_contract_test.cpp -o /tmp/fc-lock-tests && /tmp/fc-lock-tests
#include "../src/decklink-signal-lock-contract.h"
#include <assert.h>
#include <stdio.h>

using fc_decklink::classify_signal_lock;
using fc_decklink::SignalLockResult;

int main() {
    // Exact real machine result: eight successful flag reads, zero locked.
    assert(classify_signal_lock(8,8,0) == SignalLockResult::NoLock);

    // A hardware candidate requires only the actual 'visl' status flag.
    // It does NOT need 'dvim' automatic detected-mode data.
    assert(classify_signal_lock(8,8,2) == SignalLockResult::LockCandidate);
    assert(classify_signal_lock(8,8,8) == SignalLockResult::LockCandidate);
    assert(classify_signal_lock(8,8,1) == SignalLockResult::NoLock);

    // Errors/partial status queries cannot masquerade as missing lock.
    assert(classify_signal_lock(8,7,0) == SignalLockResult::UnknownStatus);
    assert(classify_signal_lock(8,7,2) == SignalLockResult::UnknownStatus);
    assert(classify_signal_lock(0,0,0) == SignalLockResult::UnknownStatus);
    assert(classify_signal_lock(8,8,9) == SignalLockResult::UnknownStatus);

    puts("stage=decklink_lock_contract result=PASS tests=8"
         " format_detection_required=0 video_streams=0");
    return 0;
}
