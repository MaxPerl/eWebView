/**
 * browser.c  –  Demo-Browser für libewebview
 */

#define _GNU_SOURCE
#include <Elementary.h>
#include "ewebview.h"

typedef struct {
    Evas_Object *win;
    Evas_Object *box;
    Evas_Object *toolbar;
    Evas_Object *btn_back;
    Evas_Object *btn_fwd;
    Evas_Object *btn_reload;
    Evas_Object *url_entry;
    Evas_Object *wv;
} Browser;

static void _on_url_activated(void *data, Evas_Object *obj, void *ei)
{
    (void)ei;
    Browser *b = data;
    const char *url = elm_entry_entry_get(obj);
    if (!url || !*url) return;

    char *full = NULL;
    if (strstr(url, "://"))
        full = strdup(url);
    else
        asprintf(&full, "https://%s", url);

    ewebview_url_set(b->wv, full);
    free(full);
}

static void _on_entry_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    (void)e; (void)event_info;
    Browser *b = data;
    ewebview_focus_set(b->wv, EINA_FALSE);
    elm_object_focus_set(obj, EINA_TRUE);
}

static void _on_back(void *data, Evas_Object *o, void *ei)
{ (void)o;(void)ei; ewebview_back(((Browser*)data)->wv); }

static void _on_fwd(void *data, Evas_Object *o, void *ei)
{ (void)o;(void)ei; ewebview_forward(((Browser*)data)->wv); }

static void _on_reload(void *data, Evas_Object *o, void *ei)
{ (void)o;(void)ei; ewebview_reload(((Browser*)data)->wv); }

static void _on_wv_url_changed(void *data, Evas_Object *obj, void *ei)
{
    (void)obj;
    Browser *b = data;
    const char *url = (const char *)ei;
    if (url) elm_entry_entry_set(b->url_entry, url);
}

static void _on_wv_title_changed(void *data, Evas_Object *obj, void *ei)
{
    (void)obj;
    Browser *b = data;
    const char *title = (const char *)ei;
    if (title) elm_win_title_set(b->win, title);
}

static Browser *browser_create(const char *url)
{
    Browser *b = calloc(1, sizeof(Browser));

    b->win = elm_win_util_standard_add("ewebview-browser", "eWebView Browser");
    elm_win_autodel_set(b->win, EINA_TRUE);
    evas_object_resize(b->win, 1280, 768);

    b->box = elm_box_add(b->win);
    elm_box_horizontal_set(b->box, EINA_FALSE);
    evas_object_size_hint_weight_set(b->box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(b->win, b->box);
    evas_object_show(b->box);

    b->toolbar = elm_box_add(b->win);
    elm_box_horizontal_set(b->toolbar, EINA_TRUE);
    elm_box_homogeneous_set(b->toolbar, EINA_FALSE);
    evas_object_size_hint_weight_set(b->toolbar, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(b->toolbar, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(b->toolbar);

    b->btn_back = elm_button_add(b->win);
    elm_object_text_set(b->btn_back, "◀");
    evas_object_smart_callback_add(b->btn_back, "clicked", _on_back, b);
    evas_object_show(b->btn_back);
    elm_box_pack_end(b->toolbar, b->btn_back);

    b->btn_fwd = elm_button_add(b->win);
    elm_object_text_set(b->btn_fwd, "▶");
    evas_object_smart_callback_add(b->btn_fwd, "clicked", _on_fwd, b);
    evas_object_show(b->btn_fwd);
    elm_box_pack_end(b->toolbar, b->btn_fwd);

    b->btn_reload = elm_button_add(b->win);
    elm_object_text_set(b->btn_reload, "↺");
    evas_object_smart_callback_add(b->btn_reload, "clicked", _on_reload, b);
    evas_object_show(b->btn_reload);
    elm_box_pack_end(b->toolbar, b->btn_reload);

    b->url_entry = elm_entry_add(b->win);
    elm_entry_single_line_set(b->url_entry, EINA_TRUE);
    elm_entry_scrollable_set(b->url_entry, EINA_TRUE);
    elm_entry_entry_set(b->url_entry, url ? url : "");
    evas_object_size_hint_weight_set(b->url_entry, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(b->url_entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add(b->url_entry, "activated", _on_url_activated, b);
    evas_object_event_callback_add(b->url_entry, EVAS_CALLBACK_MOUSE_DOWN,
                                   _on_entry_mouse_down, b);
    evas_object_show(b->url_entry);
    elm_box_pack_end(b->toolbar, b->url_entry);
    elm_box_pack_end(b->box, b->toolbar);

    Evas *evas = evas_object_evas_get(b->win);
    b->wv = ewebview_add(evas);
    evas_object_size_hint_weight_set(b->wv, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(b->wv, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(b->wv);
    elm_box_pack_end(b->box, b->wv);

    evas_object_smart_callback_add(b->wv, "url,changed",   _on_wv_url_changed,   b);
    evas_object_smart_callback_add(b->wv, "title,changed", _on_wv_title_changed, b);

    evas_object_show(b->win);
    if (url) ewebview_url_set(b->wv, url);
    return b;
}

EAPI_MAIN int elm_main(int argc, char **argv)
{
    elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
    const char *url = (argc > 1) ? argv[1] : "https://www.webkit.org";
    Browser *b = browser_create(url);
    (void)b;
    elm_run();
    elm_shutdown();
    return 0;
}
ELM_MAIN()