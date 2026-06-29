/**
 * ewebview.c  –  WPEWebKit als Evas Smart Object (libewebview)
 */

#define _GNU_SOURCE
#include "ewebview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <Evas.h>

// Am Anfang der Datei (oder über CMake) prüfen wir, ob Elementary verfügbar ist.
// Falls wie elementary.h inkludieren können, fügen wir es hier hinzu:
#if HAS_ELEMENTARY
#include <Elementary.h>
#endif

#include <wpe/wpe.h>
#include <wpe/fdo.h>
#include <wpe/unstable/fdo-shm.h>
#include <wpe/webkit.h>

#include <wayland-server-protocol.h>
#include <xkbcommon/xkbcommon.h>



/* ── Private Instanzdaten ─────────────────────────────────────────────────── */
typedef struct {
    // Pflichtfeld bei EVAS_SMART_CLIPPED: muss ERSTES Feld in der Struct sein!
    // EFL legt hier intern das Clipper-Rectangle ab. Wir nutzen es zusätzlich
    // als Fokus-Träger für Key-Events (Evas_Object_Image kann keinen Fokus halten).
    Evas_Object_Smart_Clipped_Data clip_data;

    // Evas-Objekte
    Evas_Object  *clip;          // Zeigt auf den EFL-internen Clipper (via evas_object_smart_clipped_clipper_get) */
    Evas_Object  *img;           // Evas_Object_Image – WebKit-Frame        */

    // WPE
    struct wpe_view_backend_exportable_fdo *exportable;
    struct wpe_view_backend                *wpe_backend;
    WebKitWebView                          *web_view;

    // Gecachte Metadaten 
    char         *current_url;
    char         *current_title;
    double        load_progress;

    // Geometrie
    int           x, y, w, h;
    int           wv_focused;

    // Selbst verwalteter Pixelbuffer — verhindert GL-Modus in Evas.
    // Evas bekommt immer denselben Pointer, wir kopieren WPE-Frames hinein.
    // Bei Resize neu allozieren und alten freigeben.
    void         *pixel_buf;
    int           pixel_buf_w, pixel_buf_h;

    
    // das zuletzt fokussierte Widget (brauchen wir für webkit_focus_set, um den Fokus wieder an Elm abzugeben)
    Evas_Object   *focus_saved;

    /* Laufender Download — gesetzt in _on_download_started,
     * damit ewebview_download_* API darauf zugreifen kann */
    WebKitDownload *current_download;
} Wv_Data;

/* ── Smart-Object Typ ─────────────────────────────────────────────────────── */
static Evas_Smart *_smart = NULL;

#define WV_DATA_GET(obj, sd) \
    Wv_Data *sd = evas_object_smart_data_get(obj)

/* ── GLib-Pump (einmalig pro Prozess) ────────────────────────────────────── */
static Ecore_Timer *_glib_pump = NULL;
static int          _instance_count = 0;

static Eina_Bool _glib_pump_cb(void *d)
{
    (void)d;
    while (g_main_context_iteration(NULL, FALSE))
        ;
    return ECORE_CALLBACK_RENEW;
}

/* ── SHM Frame-Callback ───────────────────────────────────────────────────── */
static void _on_export_shm_buffer(void *userdata,
                                   struct wpe_fdo_shm_exported_buffer *buffer)
{
    Evas_Object *obj = (Evas_Object *)userdata;
    WV_DATA_GET(obj, sd);
    if (!sd) goto done;
 
    struct wl_shm_buffer *shm =
        wpe_fdo_shm_exported_buffer_get_shm_buffer(buffer);
    if (!shm) goto done;
 
    int sw     = wl_shm_buffer_get_width(shm);
    int sh     = wl_shm_buffer_get_height(shm);
    int stride = wl_shm_buffer_get_stride(shm);
 
    // Pixel-Buffer neu allozieren wenn sich die Größe geändert hat.
    // Wir verwalten den Buffer selbst — kein evas_object_image_data_get nötig,
    // kein GL-Konflikt. Evas bekommt immer denselben Pointer via data_set.
    if (sw != sd->pixel_buf_w || sh != sd->pixel_buf_h) {
        free(sd->pixel_buf);
        sd->pixel_buf   = calloc((size_t)(sw * sh), 4);
        sd->pixel_buf_w = sw;
        sd->pixel_buf_h = sh;
        evas_object_image_size_set(sd->img, sw, sh);
        evas_object_image_fill_set(sd->img, 0, 0, sw, sh);
    }
 
    if (!sd->pixel_buf) goto done;
 
    // Direkt in unseren Buffer kopieren — kein data_get nötig
    wl_shm_buffer_begin_access(shm);
    void *src = wl_shm_buffer_get_data(shm);
    if (src) {
        int row = sw * 4;
        if (stride == row) {
            memcpy(sd->pixel_buf, src, (size_t)(sh * row));
        } else {
            for (int y = 0; y < sh; y++)
                memcpy((uint8_t*)sd->pixel_buf + y*row,
                       (uint8_t*)src + y*stride, (size_t)row);
        }
        // Buffer an Evas übergeben und Neuzeichnen anfordern
        evas_object_image_data_set(sd->img, sd->pixel_buf);
        evas_object_image_data_update_add(sd->img, 0, 0, sw, sh);
    }
    wl_shm_buffer_end_access(shm);
 
done:
    wpe_view_backend_exportable_fdo_dispatch_frame_complete(
        sd ? sd->exportable : NULL);
    wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(
        sd ? sd->exportable : NULL, buffer);
}



/* ── WebKit Signal-Callbacks ──────────────────────────────────────────────── */
static void _on_load_changed(WebKitWebView *wkv, WebKitLoadEvent ev,
                              gpointer userdata)
{
    Evas_Object *obj = (Evas_Object *)userdata;
    WV_DATA_GET(obj, sd);
    if (!sd) return;

    const char *uri = webkit_web_view_get_uri(wkv);

    free(sd->current_url);
    sd->current_url = uri ? strdup(uri) : NULL;

    switch (ev) {
    case WEBKIT_LOAD_STARTED:
        sd->load_progress = 0.0;
        evas_object_smart_callback_call(obj, "load,started",
                                        (void*)sd->current_url);
        evas_object_smart_callback_call(obj, "url,changed",
                                        (void*)sd->current_url);
        break;
    case WEBKIT_LOAD_FINISHED:
        sd->load_progress = 1.0;
        evas_object_smart_callback_call(obj, "load,finished",
                                        (void*)sd->current_url);
        break;
    default: break;
    }
}

static void _on_progress_changed(WebKitWebView *wkv, GParamSpec *ps,
                                  gpointer userdata)
{
    (void)ps;
    Evas_Object *obj = (Evas_Object *)userdata;
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    sd->load_progress = webkit_web_view_get_estimated_load_progress(wkv);
    evas_object_smart_callback_call(obj, "load,progress", &sd->load_progress);
}

static void _on_title_changed(WebKitWebView *wkv, GParamSpec *ps,
                               gpointer userdata)
{
    (void)ps;
    Evas_Object *obj = (Evas_Object *)userdata;
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    free(sd->current_title);
    const char *t = webkit_web_view_get_title(wkv);
    sd->current_title = t ? strdup(t) : NULL;
    evas_object_smart_callback_call(obj, "title,changed",
                                    (void*)sd->current_title);
}

static void _on_uri_changed(WebKitWebView *wkv, GParamSpec *ps,
                             gpointer userdata)
{
    (void)ps;
    Evas_Object *obj = (Evas_Object *)userdata;
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    free(sd->current_url);
    const char *uri = webkit_web_view_get_uri(wkv);
    sd->current_url = uri ? strdup(uri) : NULL;
    evas_object_smart_callback_call(obj, "url,changed",
                                    (void*)sd->current_url);
}

/* ── Download-Callbacks ───────────────────────────────────────────────────── */

static void _on_download_finished(WebKitDownload *download, gpointer userdata)
{
    Evas_Object *obj = (Evas_Object *)userdata;
    WV_DATA_GET(obj, sd);
    /* event_info = const char* mit dem Zielpfad wo die Datei liegt */
    const char *dest = sd ? webkit_download_get_destination(download) : NULL;
    evas_object_smart_callback_call(obj, "download,finished", (void*)dest);
    /* Alle Signal-Handler dieses Downloads aufräumen */
    g_signal_handlers_disconnect_by_data(download, userdata);
}

static void _on_download_failed(WebKitDownload *download,
                                 GError *error, gpointer userdata)
{
    Evas_Object *obj = (Evas_Object *)userdata;
    /* event_info = const char* mit der Fehlermeldung */
    const char *msg = error ? error->message : "Unbekannter Fehler";
    evas_object_smart_callback_call(obj, "download,failed", (void*)msg);
    /* Alle Signal-Handler dieses Downloads aufräumen */
    g_signal_handlers_disconnect_by_data(download, userdata);
}

/*
 * Wird aufgerufen wenn:
 *   - der User "Speichern" in der PDF-Toolbar drückt
 *   - ein nicht darstellbarer MIME-Type geladen wird (zip, exe, ...)
 *   - webkit_download_set_destination() nicht gesetzt wurde
 *
 * Verfügbare Smart Callbacks:
 *   "download,requested" → event_info = eWebView_DownloadRequest
 *   "download,finished"  → event_info = const char* (Zielpfad)
 *   "download,failed"    → event_info = const char* (Fehlermeldung)
 *
 * "download,progress" kann später ergänzt werden wenn eine Fortschrittsanzeige
 * gewünscht ist (notify::estimated-progress auf dem WebKitDownload-Objekt).
 */
// Forward-Deklaration für das neue Signal
static gboolean _on_decide_destination(WebKitDownload *download, 
                                       gchar *suggested_filename, 
                                       gpointer userdata);

static void _on_download_started(WebKitNetworkSession *session,
                                 WebKitDownload *download,
                                 gpointer userdata)
{
    (void)session;
    Evas_Object *obj = (Evas_Object *)userdata;
    WV_DATA_GET(obj, sd);
    if (!sd) return;

    /* Download-Objekt merken damit ewebview_download_* API darauf zugreifen kann */
    if (sd->current_download)
        g_object_unref(sd->current_download);
    sd->current_download = g_object_ref(download);

    // Standard-Handler für Ende/Fehler
    g_signal_connect(download, "finished", G_CALLBACK(_on_download_finished), obj);
    g_signal_connect(download, "failed", G_CALLBACK(_on_download_failed), obj);

    // HIER klinken wir uns ein, BEVOR WebKit den Pfad festlegt!
    g_signal_connect(download, "decide-destination", 
                     G_CALLBACK(_on_decide_destination), obj);
}

static gboolean _on_decide_destination(WebKitDownload *download, 
                                       gchar *suggested_filename, 
                                       gpointer userdata)
{
    //(void)suggested_filename;
    Evas_Object *obj = (Evas_Object *)userdata;
    
    // Wir holen die URI des Downloads, um sie an Perl zu übergeben
    const char *uri = webkit_uri_request_get_uri(webkit_download_get_request(download));

    eWebView_DownloadRequest event_data;
    event_data.uri = uri;
    event_data.suggested_filename = suggested_filename;
    
    // Wir triggern deinen Perl Smart-Callback "download,started"
    evas_object_smart_callback_call(obj, "download,requested", &event_data);

    // WICHTIG: TRUE zurückgeben signalisiert WebKit, dass wir das Handling 
    // übernehmen und die Destination asynchron via webkit_download_set_destination setzen!
    return TRUE; 
}

/* ── Input ────────────────────────────────────────────────────────────────── */

// Wichtig: WPE erwartet X11 KeySyms, die von Evas gelieferten Keycodes müssen daher 
// mit den X11 Keysm Konstanten aus xkbcommon gemappt werden 
static uint32_t _keyname_to_keysym(const char *k)
{
    if (!k) return XKB_KEY_NoSymbol;
    xkb_keysym_t s = xkb_keysym_from_name(k, XKB_KEYSYM_NO_FLAGS);
    if (s != XKB_KEY_NoSymbol) return s;
    if (!strcmp(k, "space")) return XKB_KEY_space;
    return XKB_KEY_NoSymbol;
}

/*
 * _modifiers: Evas-Modifier → WPE-Modifier-Bitmask
 *
 * Modifier-Tasten (Shift, Ctrl, Alt) werden als BITMASK kodiert —
 * ein klassisches C-Pattern für "mehrere Flags gleichzeitig aktiv".
 *
 * Jeder Modifier belegt genau ein Bit in einem uint32_t:
 *
 *   wpe_input_keyboard_modifier_shift   = 0b001  (= 1)
 *   wpe_input_keyboard_modifier_control = 0b010  (= 2)
 *   wpe_input_keyboard_modifier_alt     = 0b100  (= 4)
 *
 * Mit |= (Bitwise OR Assignment) werden aktive Modifier ins Ergebnis r
 * "eingeschaltet" ohne die anderen zu löschen:
 *
 *   r = 0          →  0b000  (keine Modifier)
 *   r |= shift     →  0b001  (Shift ein)
 *   r |= control   →  0b011  (Shift + Ctrl ein)
 *
 * WPE prüft empfangsseitig mit & (Bitwise AND) welche Modifier aktiv sind:
 *   if (modifiers & wpe_input_keyboard_modifier_shift) ...
 *
 * Vorteil: ein einziges uint32_t kann 32 Flags gleichzeitig halten.
 *
 * Wichtig: Modifier werden für Tastenkombinationen in der Webseite gebraucht:
 *   Shift+Tab    → rückwärts durch fokussierbare Elemente tabben
 *   Ctrl+C/V     → Copy/Paste in Webseiten
 *   Alt+Left     → Browser-Back per Tastatur
 * Ohne Modifier-Übertragung würden diese Kombinationen nicht funktionieren.
 *
 * Evas liefert Modifier als opaken Evas_Modifier* Pointer — der einzige
 * Weg sie abzufragen ist evas_key_modifier_is_set() mit dem Namen als String.
 */
static uint32_t _modifiers(const Evas_Modifier *m)
{
    if (!m) return 0;
    uint32_t r = 0;
    if (evas_key_modifier_is_set(m, "Shift"))   r |= wpe_input_keyboard_modifier_shift;
    if (evas_key_modifier_is_set(m, "Control")) r |= wpe_input_keyboard_modifier_control;
    if (evas_key_modifier_is_set(m, "Alt"))     r |= wpe_input_keyboard_modifier_alt;
    return r;
}

//
void _cb_key_down(void *d, Evas *e, Evas_Object *o, void *ei)
{
    (void)e;(void)o;
    Evas_Object *obj = (Evas_Object *)d;
    WV_DATA_GET(obj, sd); if (!sd) return;
    Evas_Event_Key_Down *ev = ei;
    struct wpe_input_keyboard_event wev = {
        .time     = (uint32_t)(ecore_time_get() * 1000),
        .key_code = _keyname_to_keysym(ev->key),
        .modifiers = _modifiers(ev->modifiers),
        .pressed  = true,
    };
    wpe_view_backend_dispatch_keyboard_event(sd->wpe_backend, &wev);
}

void _cb_key_up(void *d, Evas *e, Evas_Object *o, void *ei)
{
    (void)e;(void)o;
    Evas_Object *obj = (Evas_Object *)d;
    WV_DATA_GET(obj, sd); if (!sd) return;
    Evas_Event_Key_Up *ev = ei;
    struct wpe_input_keyboard_event wev = {
        .time     = (uint32_t)(ecore_time_get() * 1000),
        .key_code = _keyname_to_keysym(ev->key),
        .modifiers = _modifiers(ev->modifiers),
        .pressed  = false,
    };
    wpe_view_backend_dispatch_keyboard_event(sd->wpe_backend, &wev);
}

static void
_canvas_key_down_wrapper_cb(void *data, Evas *e, Evas_Object *o, void *event_info)
{
    (void)o;
    Evas_Object *obj = data;
    WV_DATA_GET(obj, sd); if (!sd) return;
    if (!sd->wv_focused) return;
    _cb_key_down(obj, e, obj, event_info);
}

static void
_canvas_key_up_wrapper_cb(void *data, Evas *e, Evas_Object *o, void *event_info)
{
    (void)o;
    Evas_Object *obj = data;
    WV_DATA_GET(obj, sd); if (!sd) return;
    if (!sd->wv_focused) return;
    _cb_key_up(obj, e, obj, event_info);
}


static void _cb_mouse_down(void *d, Evas *e, Evas_Object *o, void *ei)
{
    (void)e;(void)o;
    Evas_Object *obj = (Evas_Object *)d;
    WV_DATA_GET(obj, sd); if (!sd) return;
    Evas_Event_Mouse_Down *ev = ei;
    /* Koordinaten relativ zum Smart Object */
    struct wpe_input_pointer_event wev = {
        .type   = wpe_input_pointer_event_type_button,
        .time   = (uint32_t)(ecore_time_get() * 1000),
        .x      = ev->canvas.x - sd->x,
        .y      = ev->canvas.y - sd->y,
        .button = 1, .state = 1,
    };
    wpe_view_backend_dispatch_pointer_event(sd->wpe_backend, &wev);

ewebview_focus_set(obj, EINA_TRUE);
}

static void _cb_mouse_up(void *d, Evas *e, Evas_Object *o, void *ei)
{
    (void)e;(void)o;
    Evas_Object *obj = (Evas_Object *)d;
    WV_DATA_GET(obj, sd); if (!sd) return;
    Evas_Event_Mouse_Up *ev = ei;
    struct wpe_input_pointer_event wev = {
        .type   = wpe_input_pointer_event_type_button,
        .time   = (uint32_t)(ecore_time_get() * 1000),
        .x      = ev->canvas.x - sd->x,
        .y      = ev->canvas.y - sd->y,
        .button = 1, .state = 0,
    };
    wpe_view_backend_dispatch_pointer_event(sd->wpe_backend, &wev);
}

static void _cb_mouse_move(void *d, Evas *e, Evas_Object *o, void *ei)
{
    (void)e;(void)o;
    Evas_Object *obj = (Evas_Object *)d;
    WV_DATA_GET(obj, sd); if (!sd) return;
    Evas_Event_Mouse_Move *ev = ei;
    struct wpe_input_pointer_event wev = {
        .type   = wpe_input_pointer_event_type_motion,
        .time   = (uint32_t)(ecore_time_get() * 1000),
        .x      = ev->cur.canvas.x - sd->x,
        .y      = ev->cur.canvas.y - sd->y,
        .button = 0, .state = 0,
    };
    wpe_view_backend_dispatch_pointer_event(sd->wpe_backend, &wev);
}

// Am Anfang der Datei (oder über CMake) prüfen wir, ob Elementary verfügbar ist.
// Falls Sie die elementary.h inkludieren können, fügen Sie sie hier hinzu:
#if HAS_ELEMENTARY
#include <Elementary.h>
#endif

static void _cb_mouse_wheel(void *d, Evas *e, Evas_Object *o, void *ei)
{
    (void)e;(void)o;
    Evas_Object *obj = (Evas_Object *)d;
    WV_DATA_GET(obj, sd); if (!sd) return;
    Evas_Event_Mouse_Wheel *ev = ei;

    // 1. Standard-Fallbacks (Wenn kein Elementary genutzt wird)
    double scroll_factor = 20.0;
    Eina_Bool natural = EINA_TRUE; // Standard: Natural Scrolling ("Finger hoch, Seite runter")

    // 2. Echtes Auslesen der Enlightenment/Elm-Einstellungen (falls verfügbar)
#if HAS_ELEMENTARY
    // Wenn Elementary aktiv ist, überschreiben wir die Fallbacks mit den echten Werten
    //scroll_factor = elm_config_scroll_accel_factor_get() * 20.0;
    natural = elm_config_scroll_thumbscroll_enabled_get();
#endif

    // 3. Mathematische Delta-Berechnung für WebKit
    double wheel_delta = (double)ev->z * scroll_factor;
    
    // Vorzeichen-Korrektur für die WebKit-Richtung:
    if (!natural) {
        wheel_delta = -wheel_delta; 
    }

    // Wir erstellen ein 2D-Achsen-Ereignis (Scrollen/Mausrad).
    // WPE benötigt dies, um Webseiten vertikal oder horizontal zu bewegen.
    struct wpe_input_axis_2d_event wev = {
        .base = {
            // .type bestimmt die Art der Bewegung. 
            // 'motion_smooth' steht für flüssiges, präzises Scrollen (z. B. Touchpad).
            // 'mask_2d' signalisiert WPE, dass wir X- und Y-Werte gleichzeitig senden.
            .type  = wpe_input_axis_event_type_motion_smooth
                   | wpe_input_axis_event_type_mask_2d,
            
            // Der Zeitstempel des Ereignisses in Millisekunden.
            // WPE braucht das für die Berechnung von Scroll-Animationen und Trägheit.
            .time  = (uint32_t)(ecore_time_get() * 1000),
            
            // Die exakte X- und Y-Position des Mauszeigers AUF der Webseite.
            // Wir nehmen die globale Canvas-Position und ziehen den Offset (sd->x / sd->y) 
            // unserer Webview ab, damit die Koordinaten relativ zur linken oberen Ecke der Webseite sind.
            .x     = ev->canvas.x - sd->x,
            .y     = ev->canvas.y - sd->y,
            
            // Diese beiden Felder werden bei der 2D-Variante ignoriert (auf 0 gesetzt), 
            // da die Werte weiter unten separat für X und Y aufgeteilt angegeben werden.
            .axis  = 0, 
            .value = 0,
        },
        
        // .x_axis: Das horizontale Scroll-Delta (Links/Rechts).
        // ev->direction == 1 bedeutet, das Mausrad/die Geste bewegt sich horizontal.
        // Falls ja, übergeben wir unser berechnetes Delta, ansonsten 0.0 (keine Bewegung).
        .x_axis = (ev->direction == 1) ? wheel_delta : 0.0,
        
        // .y_axis: Das vertikale Scroll-Delta (Oben/Unten).
        // ev->direction == 0 bedeutet, das Mausrad/die Geste bewegt sich vertikal.
        // Ein negativer Wert scrollt die Seite nach unten (Inhalt wandert hoch).
        .y_axis = (ev->direction == 0) ? wheel_delta : 0.0,
    };


    wpe_view_backend_dispatch_axis_event(sd->wpe_backend, (struct wpe_input_axis_event *)&wev);
}


static void _canvas_focus_in_cb(void *data, Evas *e, void *event_info) {
    (void)e;
    Evas_Object *obj = (Evas_Object *)data;
    WV_DATA_GET(obj, sd); if (!sd) return;
    Evas_Object *my_ignored_widget = sd->clip;
    // event_info enthält das Evas_Object, das gerade den Fokus erhalten hat
    Evas_Object *focused = (Evas_Object *)event_info;
    
    // Um später den Fokus wieder an Elm zurückgeben zu können, müssen wir
    // das zuletzt fokussierte Elm_Object hier speichern. Andernfalls 
    // könnten wir nie wieder zu Elm und bspw. zur Adressbar zurückkehren
    if (focused != my_ignored_widget) {
        sd->focus_saved = focused;
    }
}

/* ── Evas Smart Object Callbacks ──────────────────────────────────────────── */
static void _smart_add(Evas_Object *obj)
{
    // Wv_Data allozieren — Wichtig bei Clipped Smart Classes: clip_data muss das erste Feld sein
    Wv_Data *sd = calloc(1, sizeof(Wv_Data));
    evas_object_smart_data_set(obj, sd);

    // parent_sc->add initialisiert den Clipper darin (! Immer als erstes aufrufen bei Clipped Smart Class)
    const Evas_Smart_Class *parent_sc = evas_object_smart_clipped_class_get();
    parent_sc->add(obj);

    // Clipper holen — existiert jetzt weil parent->add ihn erstellt hat
    sd->clip = evas_object_smart_clipped_clipper_get(obj);

    Evas *evas = evas_object_evas_get(obj);

     // Image-Objekt = der eigentliche WebView!
    // Wir verwalten den Pixelbuffer selbst (sd->pixel_buf) damit Evas
    // immer im Software-Modus bleibt — auch wenn die GL-Engine aktiv ist.
    // Evas bekommt immer denselben Pointer via data_set; wir kopieren
    // WPE-Frames direkt in diesen Buffer.
    sd->pixel_buf_w = sd->w;
    sd->pixel_buf_h = sd->h;
    sd->pixel_buf   = calloc((size_t)(sd->w * sd->h), 4);
    sd->img = evas_object_image_add(evas);
    evas_object_image_colorspace_set(sd->img, EVAS_COLORSPACE_ARGB8888);
    evas_object_image_alpha_set(sd->img, EINA_TRUE);
    evas_object_image_size_set(sd->img, sd->w, sd->h);
    evas_object_image_data_set(sd->img, sd->pixel_buf);
    evas_object_image_fill_set(sd->img, 0, 0, sd->w, sd->h);
    evas_object_clip_set(sd->img, sd->clip);
    evas_object_smart_member_add(sd->img, obj);
    evas_object_show(sd->img);

    // WICHTIG: Fokus kann nur der Clip haben, nicht das img
    // Deshalb registrieren wir Key-Events am clip, nicht am img oder Canvas.
    evas_object_event_callback_add(sd->clip, EVAS_CALLBACK_KEY_DOWN, _canvas_key_down_wrapper_cb, obj);
    evas_object_event_callback_add(sd->clip, EVAS_CALLBACK_KEY_UP,   _canvas_key_up_wrapper_cb,   obj);

    evas_object_event_callback_add(sd->img, EVAS_CALLBACK_MOUSE_DOWN,  _cb_mouse_down,  obj);
    evas_object_event_callback_add(sd->img, EVAS_CALLBACK_MOUSE_UP,    _cb_mouse_up,    obj);
    evas_object_event_callback_add(sd->img, EVAS_CALLBACK_MOUSE_MOVE,  _cb_mouse_move,  obj);
    evas_object_event_callback_add(sd->img, EVAS_CALLBACK_MOUSE_WHEEL, _cb_mouse_wheel, obj);
    
    // Event-Registrierung für das gesamte Canvas, um das zuletzt fokusierte Widget zu speichern
    evas_event_callback_add(evas_object_evas_get(evas),EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_IN, _canvas_focus_in_cb, obj);


    /// WPE initialisieren (einmalig pro Prozess) 
    if (_instance_count == 0) {
        // Initialisiert das Shared Memory Backend 
        // -> Frames werden über den normalen Arbeitsspeicher mit anderen Prozessen geteilt
        // -> bereitet Datenstrukturen vor, damit der WebKit Prozess die fertigen Pixelbuffer
        //		direkt in den gemeinsamen Speicher schreiben kann, von wo sie die Host Anwendun
        //		abgreift
        //
        // Sidenote: Die Callbacks laufen also im Hauptthread (UI-Thread) Ihrer App
        // wir holen sie mit dem Ecore Timer _glib_pump; so funktioniert also die Integration in
        // unseren Evas/Elm_Mainloop!!!!
        
        // WPE Backend laden — der Soname wird zur Compile-Zeit über
        // WPE_BACKEND_SONAME gesetzt (in meson.build via pkg-config ermittelt).
        // Fallback auf den bekannten Namen wenn nicht definiert.
        // So muss bei einer neuen wpebackend-fdo Version nur meson.build
        // angepasst werden, nicht der Source-Code.
        #ifndef WPE_BACKEND_SONAME
        #  define WPE_BACKEND_SONAME "libWPEBackend-fdo-1.0.so.1"
        #endif
        wpe_loader_init(WPE_BACKEND_SONAME);
        // SHM initialisierung
        wpe_fdo_initialize_shm();
        _glib_pump = ecore_timer_add(0.005, _glib_pump_cb, NULL);
    }
    _instance_count++;

    /* WPE Exportable + WebView */
    sd->w = 1280; sd->h = 720;   /* Default, wird in _smart_resize gesetzt */
	
    // Statische Variable, in der Callback Funktion gespeichert wird, die
    // aufgerufen wird, wenn WPE ein neues Frame im Shared Memory bereitgestellt hat
    static struct wpe_view_backend_exportable_fdo_client shm_client = {
        .export_shm_buffer = _on_export_shm_buffer,
    };

    // erstellt das logische Verbindungs-Objekt (das "Exportable"), das die Brücke 
    // zwischen dem WebKit-Renderer und Evas schlägt, d.h.
    // -> Funktion erstellt das exportable
    // -> sie registriert die gerade im shm_client gespeicherten Callbacks
    // -> sie teilt dem Renderer mit, in welcher Breite und Höhe Webseite gerendert
    //		werden soll
    //	-> obj (= unser Smart Object) wird in der Callback Funktion als data mitübergeben
    sd->exportable = wpe_view_backend_exportable_fdo_create(
        &shm_client, obj, (uint32_t)sd->w, (uint32_t)sd->h);

    // Mit den folgenden drei Funktionen wird nun das so eben erstellte rohe Grafik-Backend
    //  in die offizielle, hochrangige WebKitGTK-API eingepackt und ein WebKitWebView-Objekt erstellt, 
    // mit dem wir den Browser am Ende wie ein normales UI-Element steuern können und die eigentlichen
    // Browser Befehle (z.B. webkit_web_view_load_uri_load()|go_back usw) abfeuern können
    sd->wpe_backend =
        wpe_view_backend_exportable_fdo_get_view_backend(sd->exportable);

    WebKitWebViewBackend *wk_backend = webkit_web_view_backend_new(
        sd->wpe_backend,
        (GDestroyNotify)wpe_view_backend_exportable_fdo_destroy,
        sd->exportable);

    sd->web_view = g_object_new(WEBKIT_TYPE_WEB_VIEW,
                                 "backend", wk_backend, NULL);

    // mit diesen Zeilen werden nun in unseren WebKit-Mainloop (s.o. läuft im UI-Thread und werden 
    // mit dem Timer glib_pump abgeholt) Callbacks für Ereignisse dieses WebKit_View-Objekts erstellt
    g_signal_connect(sd->web_view, "load-changed",
                     G_CALLBACK(_on_load_changed), obj);
    g_signal_connect(sd->web_view, "notify::estimated-load-progress",
                     G_CALLBACK(_on_progress_changed), obj);
    g_signal_connect(sd->web_view, "notify::title",
                     G_CALLBACK(_on_title_changed), obj);
    g_signal_connect(sd->web_view, "notify::uri",
                     G_CALLBACK(_on_uri_changed), obj);

    /* Download: auf WebKitWebContext registrieren (nicht auf WebView) —
     * der Context ist für alle Downloads zuständig unabhängig vom View */
    // Früher: WebKitWebContext *ctx = webkit_web_view_get_context(sd->web_view);
    // Heute:
    WebKitNetworkSession *session = webkit_web_view_get_network_session(sd->web_view);
    g_signal_connect(session, "download-started",
                 G_CALLBACK(_on_download_started), obj);

}

static void _smart_del(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return;

    // WebView freigeben
    if (sd->web_view) {
        g_object_unref(sd->web_view);
        sd->web_view = NULL;
    }

    free(sd->current_url);
    free(sd->current_title);
    free(sd->pixel_buf);
    sd->pixel_buf = NULL;

    if (sd->current_download) {
        g_object_unref(sd->current_download);
        sd->current_download = NULL;
    }


    evas_object_del(sd->img);
    // sd->clip NICHT löschen — gehört dem EFL-Clipper, parent_sc->del räumt auf (s.u.)

    _instance_count--;
    if (_instance_count == 0 && _glib_pump) {
        ecore_timer_del(_glib_pump);
        _glib_pump = NULL;
    }

    // ZULETZT parent aufrufen — der löscht das interne Clipper-Rectangle
    // und danach (!) unsere Instanz-Struct freigeben (parent_sc->del braucht die natürlich noch)
    const Evas_Smart_Class *parent_sc = evas_object_smart_clipped_class_get();
    parent_sc->del(obj);

    free(sd);
    evas_object_smart_data_set(obj, NULL);
}

static void _smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    // parent zuerst: Clipper mitbewegen (falls Hook vorhanden)
    const Evas_Smart_Class *parent_sc = evas_object_smart_clipped_class_get();
    if (parent_sc->move) parent_sc->move(obj, x, y);
    // sd->x/y merken damit Maus-Koordinaten korrekt relativiert werden
    sd->x = x; sd->y = y;
    evas_object_move(sd->img, x, y);
}

static void _smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
    WV_DATA_GET(obj, sd);
    if (!sd || (sd->w == w && sd->h == h)) return;
    // parent_sc hat scheinbar keinen resize-Hook — Clipper wird von EFL automatisch
    // über das Member-System skaliert wenn wir evas_object_resize aufrufen?
    sd->w = w; sd->h = h;
    evas_object_resize(sd->img, w, h);
    evas_object_image_size_set(sd->img, w, h);
    evas_object_image_fill_set(sd->img, 0, 0, w, h);
    // WPE über neue Viewport-Größe informieren
    wpe_view_backend_dispatch_set_size(sd->wpe_backend, (uint32_t)w, (uint32_t)h);
}

// _smart_show, _smart_hide, _smart_clip_set, _smart_clip_unset entfallen —
// EVAS_SMART_CLIPPED übernimmt das automatisch für alle Smart-Member.

/* ── Smart-Klasse registrieren ────────────────────────────────────────────── */
static void _smart_init(void)
{
    if (_smart) return;
    /* evas_object_smart_clipped_smart_set() setzt automatisch:
     *   - sc.parent    = evas_object_smart_clipped_class_get()
     *   - sc.clip_set  = (automatisch alle Member clippen)
     *   - sc.clip_unset = (automatisch alle Member entclippen)
     *   - sc.show/hide = (automatisch alle Member zeigen/verstecken)
     * Wir überschreiben danach nur noch add, del, move, resize.
     */
    static Evas_Smart_Class sc = EVAS_SMART_CLASS_INIT_NAME_VERSION("ewebview");
    evas_object_smart_clipped_smart_set(&sc);
    sc.add    = _smart_add;
    sc.del    = _smart_del;
    sc.move   = _smart_move;
    sc.resize = _smart_resize;
    _smart = evas_smart_class_new(&sc);
}

/* ── Öffentliche API ──────────────────────────────────────────────────────── */

/* ── Konstruktor ──────────────────────────────────────────────────────────── */
EWEBVIEW_API Evas_Object *ewebview_add(Evas *evas)
{
    _smart_init();
    Evas_Object *obj = evas_object_smart_add(evas, _smart);
    return obj;
}

/* ── Navigation ───────────────────────────────────────────────────────────── */
EWEBVIEW_API void ewebview_url_set(Evas_Object *obj, const char *url)
{
    WV_DATA_GET(obj, sd);
    if (!sd || !url) return;
    webkit_web_view_load_uri(sd->web_view, url);
}

EWEBVIEW_API const char *ewebview_url_get(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return NULL;
    return sd->current_url;
}

EWEBVIEW_API const char *ewebview_title_get(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return NULL;
    return sd->current_title;
}

EWEBVIEW_API void ewebview_back(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    webkit_web_view_go_back(sd->web_view);
}

EWEBVIEW_API void ewebview_forward(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    webkit_web_view_go_forward(sd->web_view);
}

EWEBVIEW_API void ewebview_reload(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    webkit_web_view_reload(sd->web_view);
}

EWEBVIEW_API void ewebview_stop(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return;
    webkit_web_view_stop_loading(sd->web_view);
}

EWEBVIEW_API double ewebview_load_progress_get(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd) return 0.0;
    return sd->load_progress;
}

/* ── Download API ─────────────────────────────────────────────────────────── */

// Diese Funktionen werden typischerweise im "download,started" Callback
// aufgerufen um auf einen laufenden Download zu reagieren.
//
// Ablauf:
//   evas_object_smart_callback_add(wv, "download,started", _on_dl, data);
//
//   static void _on_dl(void *data, Evas_Object *obj, void *ei) {
//       const char *uri = (const char*)ei;   // Download-URI
//       ewebview_download_save(obj, "/home/user/Downloads/file.pdf");
//       // oder:
//       ewebview_download_cancel(obj);
//   }
//

EWEBVIEW_API const char *ewebview_download_uri_get(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd || !sd->current_download) return NULL;
    return webkit_uri_request_get_uri(
               webkit_download_get_request(sd->current_download));
}

EWEBVIEW_API void ewebview_download_save(Evas_Object *obj, const char *path)
{
    WV_DATA_GET(obj, sd);
    if (!sd || !sd->current_download || !path) return;

    webkit_download_set_destination(sd->current_download,path);
}

EWEBVIEW_API void ewebview_download_cancel(Evas_Object *obj)
{
    WV_DATA_GET(obj, sd);
    if (!sd || !sd->current_download) return;
    webkit_download_cancel(sd->current_download);
}


/* ── Fokus ────────────────────────────────────────────────────────────────── */

// Fokus hat sich leider als ein sehr schwieriges Thema erwiesen, weil einerseits 
// Tastatureingaben nicht an den WebView übergeben werden dürfen, wenn diese nicht 
// fokussiert ist und wir andererseits aus dem WebView wieder herauskommen müssen 
// (keine Ahnung warum das ohne Modifikationen nicht geht)
// Lösung:
// 	* wir speichern, ob der WebView fokussiert ist (nur dann geben wir 
//	  Tastatureinaben an den WebView weiter
//	* wir speichern das zuletzt fokussierte (Elm_)Widget mit einem Event
//	  auf dem Canvas (s.o.). Hier müssen wir den Fokus dann ausdrücklich
//	  wieder resetten und an dieses zuletzt fokussierte Widget weitergeben!
//
EWEBVIEW_API void ewebview_focus_set(Evas_Object *obj, Eina_Bool focused)
{
    WV_DATA_GET(obj, sd);
    if (!sd || sd->wv_focused == focused) return;
    sd->wv_focused = focused;

    // Unser clip widget muss fokusiert werden oder defokusiert werden
    evas_object_focus_set(sd->clip,focused);

    // Important: Wenn Clip den Fokus verliert, müssen wir diesen auf dem alten 
    // Widget wiederherstellen
    if (!focused) {
	    evas_object_focus_set(sd->focus_saved, EINA_TRUE);
    }
    
    // Das Fokus-Problem: activity_state darf nicht angefasst werden
    // (killt Videos, blockiert X11-Fokus). Stattdessen simulieren wir
    // focus/blur direkt per JavaScript im Dokument.
    //
    // Das informiert Webseiten-JS (Google-Suchfeld, etc.) korrekt
    // ohne WPEs interne GStreamer-Pipeline zu stören.
    //
    // Merke: Nachdem wir den Fokus über focus saved wiederhergestellt haben und die
    // Key_up und Key_Down Ereignisse nun nicht vom Evas, sondern vom Clip abfeuern (so 
    // dass sie nie auch bspw. im url_entry landen) IST FOLGENDES NICHT MEHR ERFORDERLICH!!!
    //const char *js = focused
    //    ? "window.dispatchEvent(new Event('focus')); document.dispatchEvent(new Event('focus'));"
    //    : "window.dispatchEvent(new Event('blur'));  document.dispatchEvent(new Event('blur'));";

    //webkit_web_view_evaluate_javascript(sd->web_view, js, -1,
    //                                    NULL, NULL, NULL, NULL, NULL);
}
