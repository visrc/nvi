/* ICCCM Cut and paste Utilities: */

#include	<stdio.h>
#include	<X11/X.h>
#include	<X11/Intrinsic.h>
#include	<X11/Xatom.h>

typedef	int	(*PFI)();

static	PFI	icccm_paste,
		icccm_copy,
		icccm_clear,
		icccm_error;

#if defined(__STDC__)
void	InitCopyPaste(  PFI f_copy,
			PFI f_paste,
			PFI f_clear,
			PFI f_error
			)
#else
void	InitCopyPaste( f_copy, f_paste, f_clear, f_error )
PFI	f_copy;
PFI	f_paste;
PFI	f_clear;
PFI	f_error;
#endif
{
    icccm_paste	= f_paste;
    icccm_clear	= f_clear;
    icccm_copy	= f_copy;
    icccm_error	= f_error;
}


#if defined(__STDC__)
static	void	peekProc( Widget widget,
			  void *data,
			  Atom *selection,
			  Atom *type,
			  void *value,
			  unsigned long *length,
			  int *format
			  )
#else
static	void	peekProc( widget, data, selection, type, value, length, format )
	Widget	widget;
	void	*data;
	Atom	*selection, *type;
	void	*value;
	unsigned long *length;
	int	*format;
#endif
{
    if ( *type == 0 )
	(*icccm_error)( stderr, "Nothing in the primary selection buffer");
    else if ( *type != XA_STRING )
	(*icccm_error)( stderr, "Unknown type return from selection");
    else
	XtFree( value );
}


#if defined(__STDC__)
void 	AcquireClipboard( Widget wid )
#else
void 	AcquireClipboard( wid )
Widget	wid;
#endif
{
    XtGetSelectionValue( wid,
			 XA_PRIMARY,
			 XA_STRING,
			 (XtSelectionCallbackProc) peekProc,
			 NULL,
			 XtLastTimestampProcessed( XtDisplay(wid) )
			 );
}


#if defined(__STDC__)
static	void	loseProc( Widget widget )
#else
static	void	loseProc( widget )
	Widget	widget;
#endif
{
    /* we have lost ownership of the selection.  clear it */
    (*icccm_clear)( widget );

    /* also participate in the protocols */
    XtDisownSelection(	widget,
			XA_PRIMARY,
			XtLastTimestampProcessed( XtDisplay(widget) )
			);
}


#if defined(__STDC__)
static	convertProc( Widget widget,
		     Atom *selection,
		     Atom *target,
		     Atom *type,
		     void **value,
		     int *length,
		     int *format
		     )
#else
static	convertProc( widget, selection, target, type, value, length, format )
Widget	widget;
Atom	*selection, *target, *type;
void	**value;
int	*length;
int	*format;
#endif
{
    String	buffer;
    int		len;

    /* someone wants a copy of the selection.  is there one? */
    (*icccm_copy)( &buffer, &len );
    if ( len == 0 ) return False;

    /* do they want the string? */
    if ( *target == XA_STRING ) {
	*length = len;
	*value  = (void *) XtMalloc( len );
	*type   = XA_STRING;
	*format = 8;
	memcpy( (char *) *value, buffer, *length );
	return True;
	}

    /* do they want the length? */
    if ( *target == XInternAtom( XtDisplay(widget), "LENGTH", FALSE) ) {
	*length = 1;
	*value  = (void *) XtMalloc( sizeof(int) );
	*type   = *target;
	*format = 32;
	* ((int *) *value) = len;
	return True;
	}

    /* we lose */
    return False;
}


#if defined(__STDC__)
void 	AcquirePrimary( Widget widget )
#else
void 	AcquirePrimary( widget )
Widget	widget;
#endif
{
    /* assert we own the primary selection */
    XtOwnSelection( widget,
		    XA_PRIMARY,
		    XtLastTimestampProcessed( XtDisplay(widget) ),
		    (XtConvertSelectionProc) convertProc,
		    (XtLoseSelectionProc) loseProc,
		    NULL
		    );

#if defined(OPENLOOK)
    /* assert we also own the clipboard */
    XtOwnSelection( widget,
		    XA_CLIPBOARD( XtDisplay(widget) ),
		    XtLastTimestampProcessed( XtDisplay(widget) ),
		    convertProc,
		    loseProc,
		    NULL
		    );
#endif
}


#if defined(__STDC__)
static	void	gotProc( Widget widget,
			 void *data,
			 Atom *selection,
			 Atom *type,
			 void *value,
			 unsigned long *length,
			 int *format
			 )
#else
static	void	gotProc( widget, data, selection, type, value, length, format )
	Widget	widget;
	void	*data;
	Atom	*selection, *type;
	void	*value;
	unsigned long *length;
	int	*format;
#endif
{
    if ( *type == 0 )
	(*icccm_error)( stderr, "Nothing in the primary selection buffer");
    else if ( *type != XA_STRING )
	(*icccm_error)( stderr, "Unknown type return from selection");
    else {
	(*icccm_paste)( widget, value, *length );
	XtFree( value );
    }
}


#if defined(__STDC__)
void	PasteFromClipboard( Widget widget )
#else
void	PasteFromClipboard( widget )
Widget	widget;
#endif
{
    XtGetSelectionValue( widget,
			 XA_PRIMARY,
			 XA_STRING,
			 (XtSelectionCallbackProc) gotProc,
			 NULL,
			 XtLastTimestampProcessed( XtDisplay(widget) )
			 );
}
