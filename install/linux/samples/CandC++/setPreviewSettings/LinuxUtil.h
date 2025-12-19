
/***************************************************************************
 * *
 *     File: LinuxUtil.h
 *
 *     Description: Utility routines useful for Linux
 *
 *     Revisions:
 *          2014-09-25  PEC     Created
 */

#if !defined(PIXELINK_LINUX_UTIL_H)
#define PIXELINK_LINUX_UTIL_H

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1


// Prototypes
int kbhit();
time_t timeInMilliseconds();

//
// Declaring a variable of this type, will put he keyboard in 'unbuffered' mode, for the scope
// of the variable.  While in unbuffered mode, keyboard input will be passed to the application
// without the user pressing the enter key.
class DontWaitForEnter
{
public:
    DontWaitForEnter()
    {
        struct termios newTs;

        ioctl (0, TCGETS, &m_oldTs);
        newTs = m_oldTs;
        newTs.c_lflag  &= !ICANON;
        newTs.c_lflag  &= !ECHO;
        ioctl (0, TCSETS, &newTs);
    }
    ~DontWaitForEnter()
    {
        ioctl (0, TCSETS, &m_oldTs);
    }
private:
    struct termios m_oldTs;
};

#endif // !defined(PIXELINK_LINUX_UTIL_H)
