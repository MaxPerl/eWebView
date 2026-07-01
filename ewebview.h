/**
 * ewebview.h  –  WPEWebKit Evas Smart Object  (libewebview)
 *
 * Verwendung:
 *   Evas_Object *wv = ewebview_add(evas);
 *   evas_object_move(wv, 0, 0);
 *   evas_object_resize(wv, 1280, 720);
 *   evas_object_show(wv);
 *   ewebview_url_set(wv, "https://www.webkit.org");
 *
 * Compile:
 *   pkg-config --cflags --libs ewebview
 */
#ifndef EWEBVIEW_H
#define EWEBVIEW_H

#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbole die aus der shared library exportiert werden */
#ifndef EWEBVIEW_API
#  define EWEBVIEW_API __attribute__((visibility("default")))
#endif

typedef struct {
    const char *uri;
    const char *suggested_filename;
} eWebView_DownloadRequest;


/* ### Konstruktor ########################################################### */
EWEBVIEW_API Evas_Object *ewebview_add(Evas *evas);

/* ### Smart Callbacks ####################################################### */
/*
 * evas_object_smart_callback_add(wv, "url,changed",   cb, data)
 * evas_object_smart_callback_add(wv, "title,changed", cb, data)
 * evas_object_smart_callback_add(wv, "load,started",  cb, data)
 * evas_object_smart_callback_add(wv, "load,finished", cb, data)
 * evas_object_smart_callback_add(wv, "load,progress", cb, data)  // event_info = double*
 * evas_object_smart_callback_add(wv, "download,requested", cb, data) 
 *   → event_info = eWebView_DownloadRequest
 * Important: Registered on the WebKitWeb context, not on the WebView; that is, if there are multiple
 *        instances, one would receive downloads from all of them; but in the typical case
 *        where there is only one instance, this shouldn't be a problem! One just have to be aware of it...  
 * Wichtig: am WebKitWeb Kontext registriert, nicht am WebView, d.h. bei mehreren
 *		Instanzen würde man Download von allen bekommen; für den Normalfall
 *		mit einer Instanz dürfte das aber kein Problem sein! Man muss es nur wissen...
 * evas_object_smart_callback_add(wv, "download,finished", cb, data)  
 * 	→ event_info = const char* (destination path / Zielpfad)
 * evas_object_smart_callback_add(wv,"download,failed", cb, data)
 * 	→ event_info = const char* (error message / Fehlermeldung)
 ############################################################################### */

/* ### Navigation ############################################################## */
EWEBVIEW_API void        ewebview_url_set(Evas_Object *obj, const char *url);
EWEBVIEW_API const char *ewebview_url_get(Evas_Object *obj);
EWEBVIEW_API const char *ewebview_title_get(Evas_Object *obj);
EWEBVIEW_API void        ewebview_back(Evas_Object *obj);
EWEBVIEW_API void        ewebview_forward(Evas_Object *obj);
EWEBVIEW_API void        ewebview_reload(Evas_Object *obj);
EWEBVIEW_API void        ewebview_stop(Evas_Object *obj);
EWEBVIEW_API double      ewebview_load_progress_get(Evas_Object *obj);

/* ### Focus ################################################################### */
EWEBVIEW_API void        ewebview_focus_set(Evas_Object *obj, Eina_Bool focused);

/* ### Download ################################################################ */
EWEBVIEW_API const char *ewebview_download_uri_get(Evas_Object *obj);
EWEBVIEW_API void        ewebview_download_set_destination(Evas_Object *obj, const char *path);
EWEBVIEW_API void        ewebview_download_cancel(Evas_Object *obj);

/* ### Copy & Paste ############################################################ */
EWEBVIEW_API void ewebview_selected_text_request(Evas_Object *obj);
EWEBVIEW_API void ewebview_text_to_clipboard(Evas_Object *obj, const char *text);
EWEBVIEW_API void ewebview_clipboard_paste(Evas_Object *obj);

#ifdef __cplusplus
}
#endif
#endif /* EWEBVIEW_H */
