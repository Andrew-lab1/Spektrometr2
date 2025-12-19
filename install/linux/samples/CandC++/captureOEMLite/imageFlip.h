
/***************************************************************************
 *
 *     File: imageFlip.h
 *
 *     Description: Simple wrapper class for all of the image flip controls
 *
 */

#if !defined(PIXELINK_IMAGE_FLIP_H)
#define PIXELINK_IMAGE_FLIP_H

#include "PixeLINKApi.h"
#include <gtk/gtk.h>
#include <vector>

class PxLImageFlip
{
public:
    // Constructor
    PxLImageFlip (GtkWidget *horizontalCheck, GtkWidget *verticalCheck);

    void  greyAll ();
    void  initialize ();

    GtkWidget    *m_horizontalCheck;
    GtkWidget    *m_verticalCheck;
};

#endif // !defined(PIXELINK_IMAGE_FLIP_H)
