#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <ewebview.h>

typedef eWebView_DownloadRequest DownloadRequest;


MODULE = eWebView::DownloadRequest		PACKAGE = DownloadRequestPtr

const char*
uri(dr)
    DownloadRequest *dr
CODE:
    RETVAL = dr->uri;
OUTPUT:
    RETVAL
 
 
const char*
suggested_filename(dr)
    DownloadRequest *dr
CODE:
    RETVAL = dr->suggested_filename;
OUTPUT:
    RETVAL