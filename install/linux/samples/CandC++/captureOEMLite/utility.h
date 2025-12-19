/*
 * utility.h
 *
 *  Created on: Apr 28, 2017
 *      Author: pcarroll
 */

#ifndef PIXELINK_UTILITY_H_
#define PIXELINK_UTILITY_H_

// Declare one of these on the stack to temporarily change
// to a wait cursor
class PxLWaitCursor
{
public:
    PxLWaitCursor()
    {
        m_topGdkWindow = gdk_get_default_root_window ();
        m_topGtkWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

        GdkDisplay* myDisplay = gdk_window_get_display(m_topGdkWindow);

        m_waitCustor = gdk_cursor_new_for_display (myDisplay, GDK_WATCH);
        m_arrowCustor = gdk_cursor_new_for_display (myDisplay, GDK_ARROW);

        gdk_window_set_cursor (m_topGdkWindow, m_waitCustor);
        gtk_widget_show_all(m_topGtkWindow);
    }
    ~PxLWaitCursor()
    {
        gdk_window_set_cursor (m_topGdkWindow, m_arrowCustor);
        gtk_widget_show_all(m_topGtkWindow);

        gtk_widget_destroy (m_topGtkWindow);
    }
private:
    GdkWindow*   m_topGdkWindow;
    GtkWidget*   m_topGtkWindow;
    GdkCursor*   m_waitCustor;
    GdkCursor*   m_arrowCustor;
};

#endif /* PIXELINK_UTILITY_H_ */
