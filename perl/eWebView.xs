#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <Evas.h>
#include <ewebview.h>

typedef Evas_Object EvasObject;
typedef Evas_Object eWebView;
typedef Evas_Canvas EvasCanvas;

MODULE = eWebView		PACKAGE = eWebView	

eWebView *
ewebview_add(e)
	EvasCanvas *e	

MODULE = eWebView		PACKAGE = eWebViewPtr	PREFIX = ewebview_

void
ewebview_url_set(obj, url);
	eWebView *obj
	const char *url

const char *
ewebview_url_get(obj);
	eWebView *obj

const char *
ewebview_title_get(obj);
	eWebView *obj
	
void
ewebview_back(obj);
	eWebView *obj
	
void
ewebview_forward(obj);
	eWebView *obj
	
void
ewebview_reload(obj);
	eWebView *obj
	
void
ewebview_stop(obj);
	eWebView *obj
	
double
ewebview_load_progress_get(obj);
	eWebView *obj
	
void
ewebview_focus_set(obj, focused);
	eWebView *obj
	Eina_Bool focused