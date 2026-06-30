#! /usr/bin/env perl

use strict;
use warnings;

use pEFL::Elm;
use pEFL::Evas;
use eWebView;
use URI;

my $url = "https://www.webkit.org";

pEFL::Elm::init($#ARGV, \@ARGV);

pEFL::Elm::policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

my $win = pEFL::Elm::Win->util_standard_add("ewebview-browser", "eWebView Browser");
$win->autodel_set(1);
$win->resize(1280, 768);

my $box = pEFL::Elm::Box->add($win);
$box->horizontal_set(0);
$box->size_hint_weight_set(EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
$win->resize_object_add($box);
$box->show();

my $toolbar = pEFL::Elm::Box->add($win);
$toolbar->horizontal_set(1);
$toolbar->size_hint_weight_set(EVAS_HINT_EXPAND, 0.0);
$toolbar->size_hint_align_set(EVAS_HINT_FILL, EVAS_HINT_FILL);
$toolbar->show();

my $btn_back = pEFL::Elm::Button->add($win);
my $back_icon = pEFL::Elm::Icon->add($win);
$back_icon->standard_set("go-previous");
$btn_back->part_content_set("icon", $back_icon);
$btn_back->size_hint_weight_set(0.0, 0.0);
$btn_back->show();
$toolbar->pack_end($btn_back);
$btn_back->smart_callback_add("clicked", \&_on_back, undef);

my $btn_next = pEFL::Elm::Button->add($win);
my $icon_next = pEFL::Elm::Icon->add($win);
$icon_next->standard_set("go-next");
$btn_next->part_content_set("icon", $icon_next);
$btn_next->size_hint_weight_set(0.0, 0.0);
$btn_next->size_hint_align_set(0.0, 0.0);
$btn_next->show();
$toolbar->pack_end($btn_next);
$btn_next->smart_callback_add("clicked", \&_on_fwd, undef);

my $btn_reload = pEFL::Elm::Button->add($win);
my $icon_reload = pEFL::Elm::Icon->add($win);
$icon_reload->standard_set("view-refresh");
$btn_reload->part_content_set("icon", $icon_reload);
$btn_reload->size_hint_weight_set(0.0, 0.0);
$btn_reload->show();
$toolbar->pack_end($btn_reload);
$btn_reload->smart_callback_add("clicked", \&_on_reload, undef);

my $url_entry = pEFL::Elm::Entry->add($win);
$url_entry->single_line_set(1);
$url_entry->scrollable_set(1);
$url_entry->entry_set($url);
$url_entry->size_hint_weight_set(EVAS_HINT_EXPAND, 0.0);
$url_entry->size_hint_align_set(EVAS_HINT_FILL, EVAS_HINT_FILL);
$url_entry->smart_callback_add("activated", \&_on_url_activated, undef);
$url_entry->event_callback_add(EVAS_CALLBACK_MOUSE_DOWN, \&_on_entry_mouse_down, undef);
$url_entry->show();
$toolbar->pack_end($url_entry);

$box->pack_end($toolbar);

my $evas = $win->evas_get();
my $wv = eWebView->add($evas);
$wv->size_hint_weight_set(EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
$wv->size_hint_align_set(EVAS_HINT_FILL, EVAS_HINT_FILL);
$wv->show();
$box->pack_end($wv);

my $hbox = pEFL::Elm::Box->add($box);
$hbox->horizontal_set(1);
$hbox->size_hint_weight_set(EVAS_HINT_EXPAND,0);
$hbox->size_hint_align_set(EVAS_HINT_FILL,0);
$box->pack_end($hbox);
$hbox->show();
	
my $separator = pEFL::Elm::Separator->add($hbox);
$separator->horizontal_set(1);
$separator->size_hint_weight_set(EVAS_HINT_EXPAND,0);
$separator->size_hint_align_set(0.5,0);
$hbox->pack_end($separator);
$separator->show();

my $progress = pEFL::Elm::Progressbar->add($hbox);
$progress->size_hint_weight_set(0.2,0);
$progress->size_hint_align_set(EVAS_HINT_FILL,0);
$progress->size_hint_max_set(350,-1);
$progress->pulse_set(0); 
$progress->value_set(0.0);
$hbox->pack_end($progress);
$progress->show();

$wv->smart_callback_add("url,changed",   \&_on_wv_url_changed,   undef);
$wv->smart_callback_add("title,changed", \&_on_wv_title_changed, undef);
$wv->smart_callback_add("load,progress", \&_on_wv_load_progress, $progress);

# ── Download Callbacks ─────────────────────────────────────────────────────
$wv->smart_callback_add("download,requested",  \&_on_download_requested,  undef);
$wv->smart_callback_add("download,finished", \&_on_download_finished, undef);
$wv->smart_callback_add("download,failed",   \&_on_download_failed,   undef);

$win->show();
$wv->url_set($url);
$win->smart_callback_add("delete,request", sub { print "Exiting\n" }, undef);

pEFL::Elm::run();
pEFL::Elm::shutdown();


sub _on_url_activated {
    my ($data, $obj, $ei) = @_;
    my $url = $obj->entry_get();
    return unless $url;
    $url = "https://$url" unless $url =~ m{^[a-zA-Z][a-zA-Z0-9+\-.]*://};
    my $uri = URI->new($url)->as_string;
    print "Set URL to $uri\n";
    $wv->url_set($uri);
    $obj->focus_set(0);
    $wv->focus_set(1);
}

sub _on_entry_mouse_down {
    my ($data, $e, $obj, $event_info) = @_;
    $wv->focus_set(0);
    $obj->focus_set(1);
}

sub _on_back {
	$wv->back;
}

sub _on_fwd {
	$wv->forward;
}


sub _on_wv_url_changed {
    my ($data, $obj, $ei) = @_;
    my $url = pEFL::ev_info2s($ei);
    $url_entry->entry_set($url) if $url;
}

sub _on_reload {
	$wv->reload;
}

sub _on_wv_title_changed {
    my ($data, $obj, $ei) = @_;
    my $title = pEFL::ev_info2s($ei);
    $win->title_set($title) if $title;
}

sub _on_wv_load_progress {
	my ($data, $obj, $ei) = @_;
	my $progress = $obj->load_progress_get();
	$data->value_set($progress);
}

# ── Download ───────────────────────────────────────────────────────────────

# Globale Variable für den laufenden Download-Dialog
my $dl_popup;

sub _on_download_requested {
    my ($data, $obj, $ei) = @_;
    my $download_request = pEFL::ev_info2obj($ei, "eWebView::DownloadRequest");
    print "Download gestartet: "- $download_request->uri ."\n";

    # Fileselector-Popup zeigen
    $dl_popup = pEFL::Elm::Popup->add($win);
    $dl_popup->size_hint_weight_set(EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    my $dl_box = pEFL::Elm::Box->add($dl_popup);
    $dl_box->size_hint_weight_set(EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    $dl_box->size_hint_align_set(EVAS_HINT_FILL, EVAS_HINT_FILL);

    # Fileselector
    my $fs = pEFL::Elm::Fileselector->add($dl_popup);
    $fs->is_save_set(1);
    $fs->expandable_set(0);
    $fs->path_set($ENV{HOME} . "/Downloads");
    $fs->current_name_set($download_request->suggested_filename);
    $fs->size_hint_weight_set(EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    $fs->size_hint_align_set(EVAS_HINT_FILL, EVAS_HINT_FILL);
    $fs->show();
    $dl_box->pack_end($fs);
    $dl_box->show();

    $dl_popup->part_content_set("default", $dl_box);
    $dl_popup->part_text_set("title,text", "Speichern unter...");

    # Fileselector-Callbacks
    $fs->smart_callback_add("done", sub {
        my ($d, $o, $path) = @_;
        my $dest = pEFL::ev_info2s($path);
        if ($dest) {
            print "Speichern nach: $dest\n";
            $wv->download_save($dest);
        } else {
            # Abbrechen gedrückt
            $wv->download_cancel();
        }
        $dl_popup->del();
        $dl_popup = undef;
    }, undef);

    $dl_popup->show();
}

sub _on_download_finished {
    my ($data, $obj, $ei) = @_;
    my $dest = pEFL::ev_info2s($ei);
    print "Download fertig: $dest\n";

    # Kurze Erfolgsmeldung
    my $notify = pEFL::Elm::Notify->add($win);
    $notify->align_set(0.5, 1.0);  # unten zentriert
    $notify->timeout_set(3.0);

    my $lbl = pEFL::Elm::Label->add($notify);
    $lbl->text_set("Download abgeschlossen: $dest");
    $lbl->show();
    $notify->content_set($lbl);
    $notify->show();
}

sub _on_download_failed {
    my ($data, $obj, $ei) = @_;
    my $msg = pEFL::ev_info2s($ei);
    print "Download fehlgeschlagen: $msg\n";

    my $popup = pEFL::Elm::Popup->add($win);
    $popup->part_text_set("title,text", "Download fehlgeschlagen");
    $popup->part_text_set("default", $msg);

    my $btn = pEFL::Elm::Button->add($popup);
    $btn->part_text_set("default", "OK");
    $btn->smart_callback_add("clicked", sub {
        $popup->del();
    }, undef);
    $popup->part_content_set("button1", $btn);
    $popup->show();
}
