/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de/
 * Copyright (c) 2009-2013, The OpenClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

/* A wrapper class to OS dependent event and window interfaces, X11 version */

#include <C4Include.h>
#include <C4App.h>

#include <C4Window.h>
#include <C4DrawGL.h>
#include <C4Draw.h>
#include <StdFile.h>
#include <StdBuf.h>

#include <glib.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <gdk/gdkx.h>
#endif

#include "C4AppGTKImpl.h"

C4AbstractApp::C4AbstractApp(): Active(false), fQuitMsgReceived(false),
		// main thread
#ifdef HAVE_PTHREAD
		MainThread (pthread_self()),
#endif
		Priv(new C4X11AppImpl(this)), fDspModeSet(false)
{
	Add(&Priv->GLibProc);
}

C4AbstractApp::~C4AbstractApp()
{
	Remove(&Priv->GLibProc);
	delete Priv;
}

bool C4AbstractApp::Init(int argc, char * argv[])
{
	// Set locale
	setlocale(LC_ALL,"");
	gtk_init(&argc, &argv);

	GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/org/openclonk/engine/oc.ico", NULL);
	gtk_window_set_default_icon(icon);
	g_object_unref(icon);
	// Try to figure out the location of the executable
	Priv->argc=argc; Priv->argv=argv;

#ifdef GDK_WINDOWING_X11
	Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
	int xrandr_error_base;
	if (!XRRQueryExtension(dpy, &Priv->xrandr_event, &xrandr_error_base)
	    || !XRRQueryVersion(dpy, &Priv->xrandr_major_version, &Priv->xrandr_minor_version))
	{
		Priv->xrandr_major_version = -1;
		Priv->xrandr_minor_version = 0;
	}
	if (Priv->xrandr_major_version >= 0)
	{
		XRRSelectInput(dpy, DefaultRootWindow(dpy), RRScreenChangeNotifyMask);
	}
	else
		Log("The Xrandr extension is missing. Resolution switching will not work.");
#endif

	// Custom initialization
	return DoInit (argc, argv);
}

static void
gtk_clipboard_store_all (void)
{
  GtkClipboard *clipboard;
  GSList *displays, *list;
  
  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());

  list = displays;
  while (list)
    {
      GdkDisplay *display = static_cast<GdkDisplay *>(list->data);

      clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);

      if (clipboard)
	gtk_clipboard_store (clipboard);
      
      list = list->next;
    }
  g_slist_free (displays);
  
}

void C4AbstractApp::Clear()
{
	gtk_clipboard_store_all();
}

void C4AbstractApp::Quit()
{
	fQuitMsgReceived = true;
}

bool C4AbstractApp::FlushMessages()
{
	// Always fail after quit message
	if (fQuitMsgReceived)
		return false;

	Priv->GLibProc.IteratePendingEvents();
	return true;
}

bool C4AbstractApp::SetVideoMode(int iXRes, int iYRes, unsigned int iColorDepth, unsigned int iRefreshRate, unsigned int iMonitor, bool fFullScreen)
{
#ifdef GDK_WINDOWING_X11
	Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
	if (!fFullScreen)
	{
		RestoreVideoMode();
		if (iXRes != -1)
			pWindow->SetSize(iXRes, iYRes);
		return true;
	}

	if (Priv->xrandr_major_version >= 0 && !(iXRes == -1 && iYRes == -1))
	{
		// randr spec says to always get fresh info, so don't cache.
		XRRScreenConfiguration * conf = XRRGetScreenInfo (dpy, pWindow->renderwnd);
		if (Priv->xrandr_oldmode == -1)
			Priv->xrandr_oldmode = XRRConfigCurrentConfiguration (conf, &Priv->xrandr_rot);
		int n;
		XRRScreenSize * sizes = XRRConfigSizes(conf, &n);
		for (int i = 0; i < n; ++i)
		{
			if (int(sizes[i].width) == iXRes && int(sizes[i].height) == iYRes)
			{
#ifdef _DEBUG
				LogF("XRRSetScreenConfig %d", i);
#endif
				fDspModeSet = XRRSetScreenConfig(dpy, conf, pWindow->renderwnd, i, Priv->xrandr_rot, CurrentTime) == RRSetConfigSuccess;
				break;
			}
		}
		XRRFreeScreenConfigInfo(conf);
	}
	gtk_window_fullscreen(GTK_WINDOW(pWindow->window));
	return fDspModeSet || (iXRes == -1 && iYRes == -1);
#endif
}

void C4AbstractApp::RestoreVideoMode()
{
#ifdef GDK_WINDOWING_X11
	// Restore resolution
	Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
	if (fDspModeSet && Priv->xrandr_major_version >= 0 && Priv->xrandr_oldmode != -1)
	{
		XRRScreenConfiguration * conf = XRRGetScreenInfo (dpy, pWindow->renderwnd);
#ifdef _DEBUG
		LogF("XRRSetScreenConfig %d (back)", Priv->xrandr_oldmode);
#endif
		XRRSetScreenConfig (dpy, conf, pWindow->renderwnd, Priv->xrandr_oldmode, Priv->xrandr_rot, CurrentTime);
		Priv->xrandr_oldmode = -1;
		XRRFreeScreenConfigInfo(conf);
		fDspModeSet = false;
	}
	// pWindow may be unset when C4AbstractApp gets destroyed during the
	// initialization code, before a window has been created
	if (pWindow)
		gtk_window_unfullscreen(GTK_WINDOW(pWindow->window));
#endif
}

bool C4AbstractApp::GetIndexedDisplayMode(int32_t iIndex, int32_t *piXRes, int32_t *piYRes, int32_t *piBitDepth, int32_t *piRefreshRate, uint32_t iMonitor)
{
#ifdef GDK_WINDOWING_X11
	Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
	int n;
	XRRScreenSize * sizes = XRRSizes(dpy, XDefaultScreen(dpy), &n);
	if (iIndex < n && iIndex >= 0)
	{
		*piXRes = sizes[iIndex].width;
		*piYRes = sizes[iIndex].height;
		*piBitDepth = 32;
		return true;
	}
	return false;
#endif
}

// Copy the text to the clipboard or the primary selection
bool C4AbstractApp::Copy(const StdStrBuf & text, bool fClipboard)
{
	gtk_clipboard_set_text(gtk_clipboard_get(fClipboard ? GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY),
	                       text.getData(), text.getLength());
	return true;
}

// Paste the text from the clipboard or the primary selection
StdStrBuf C4AbstractApp::Paste(bool fClipboard)
{
	char * r = gtk_clipboard_wait_for_text(gtk_clipboard_get(fClipboard ? GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY));
//	gtk_clipboard_request_text(gtk_clipboard_get(fClipboard ? GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY),
//	                           GtkClipboardTextReceivedFunc callback, gpointer user_data);
	StdStrBuf rbuf;
	rbuf.Copy(r);
	g_free(r);
	return rbuf;
}

// Is there something in the clipboard?
bool C4AbstractApp::IsClipboardFull(bool fClipboard)
{
	return gtk_clipboard_wait_is_text_available(gtk_clipboard_get(fClipboard ? GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY));
}

void C4AbstractApp::MessageDialog(const char * message)
{
	GtkWidget * dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), "OpenClonk Error");
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
