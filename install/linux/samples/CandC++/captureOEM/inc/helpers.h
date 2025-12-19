/***************************************************************************
 *
 *     File: helpers.h
 *
 *     Description: Common helper functions used throughout captureOEM
 *
 */

#ifndef PIXELINK_HELPERS_H_
#define PIXELINK_HELPERS_H_

#include <gtk/gtk.h>
#include <vector>
#include "pxlport.h"
#include "LinuxTypes.h"
#include "camera.h"


void IncrementFileName(GtkEntry *entry, const char* format);
void ReplaceFileExtension(GtkEntry *entry, const char* newExtension);
bool WriteConfigFile(PxLCamera* camera,const gchar*fileName);
bool ReadConfigFile(PxLCamera* camera,const gchar*fileName);

// scope guard for critical sections
class AutoCS
{
public:
    explicit AutoCS(pxlmutex_t& cs, BOOL bLock=TRUE)
        :m_cs(cs)
        ,m_bLocked(bLock)
    {
       if(m_bLocked) PxLMutexLock(m_cs);
    }
    ~AutoCS()
    {
        if(m_bLocked) PxLMutexUnlock(m_cs);
    }
private:
    pxlmutex_t& m_cs;
    BOOL m_bLocked;
};

// Returns TRUE if the suppled pixel format is an 8-bit format, FALSE otherwise
inline BOOL eightBitFormat (float pixelFormat)
{
   switch ((int)pixelFormat)
   {
    case PIXEL_FORMAT_MONO8:
    case PIXEL_FORMAT_BAYER8_GRBG:
    case PIXEL_FORMAT_BAYER8_RGGB:
    case PIXEL_FORMAT_BAYER8_GBRG:
    case PIXEL_FORMAT_BAYER8_BGGR:
        return TRUE;
    default:
        return FALSE;

   }
}

//
// The GtkComboBoxs are dumb.  They simply a list of itmes that can be selected, but have no way of associating a 'value'
// to each of the entries in the list.  The user needs to keep track of the order in which items were added to the list, and
// then select the item based on this order.  For instance, you cannot have  list like this...
//   [0, 1, 2, 4, 6, 7, 10]
// That would be a list of 7 items, wit the last one being selected as item 6, not 10.
// This class provides a convinet 'wrapper' around the GtkComboBox to all a more intelligent data structure.
class PxLComboBox
{
    public:
        PxLComboBox(GtkWidget* gtkComboBox);

        void setSensitive (bool bSensitive);
        void addItem (int itemValue, const char* itemText);
        void removeItem (int itemValue);
        void removeAll ();
        void makeActive (int itemValue);
        int  getSelectedItem();   // Returns the itemValue of the currently sleleced item

    private:
        GtkWidget*       m_gtkComboBox;
        std::vector<int> m_values;
        
};



#endif /* PIXELINK_HELPERS_H_ */
