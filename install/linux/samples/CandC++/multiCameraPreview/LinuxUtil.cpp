/***************************************************************************
 * *
 *     File: LinuxUtil.h
 *
 *     Description: Utility routines useful for Linux
 *
 *     Revisions:
 *          2014-09-25  PEC     Created
 */
#include "LinuxUtil.h"

//----------------------------------------------------------------
// Name:
//    kbhit
//
// Description:
//    Checks the keyboard buffer to determine if a key has been
//    pressed since the last getchar.
//
//    Note that the keyboard should be in unbuffered mode in order
//    for this to work) See class DontWaitForEnter
//
// Returns:
//    1 - a key has been pressed since the last getchar
//    0 - otherwise.
//
//----------------------------------------------------------------
int kbhit ()
{
    struct timeval tv;
    fd_set         fds;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO (&fds);
    FD_SET  (0, &fds);

    if (-1 == select (01, &fds, 0, 0, &tv)) return 0;
    return (FD_ISSET(0, &fds));
}

//----------------------------------------------------------------
// Name:
//   timeInMilliseconds
//
// Description:
//   Determines the number of milliseconds since Epoch (Jan 1, 1970).  See
//   notes on accuracy.
//
// Returns:
//   The number of milliseconds since epoch
//
// Notes:
//    - On a 32 bit system, (sizeof(time_t) == 4), it's likely that the return
//      value will have overflowed.  This is, there have been more milliseconds
//      since epoch, to fit in a 32 bit quantity.
//    - Even though this routine uses gettimeofday, which has a uSecond component,
//      it is still at the mercy of the system clock.  In other words, do not expect
//      this counter will increment by 1 every millisecond.  It is more likely to
//      Increment by a value >1, at less frequent intervals.
//----------------------------------------------------------------
time_t timeInMilliseconds()
{
    struct timeval currTime;

    gettimeofday (&currTime, 0);
    return (currTime.tv_sec*1000 + currTime.tv_usec/1000);
}
