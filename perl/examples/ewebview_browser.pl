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
# win 400x400
$win->resize(1280,768);

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
$btn_back->part_content_set("icon",$back_icon);
$btn_back->size_hint_weight_set(0.0,0.0);
$btn_back->show();
$toolbar->pack_end($btn_back);
$btn_back->smart_callback_add("clicked", \&_on_back, undef);

my $btn_next = pEFL::Elm::Button->add($win);
my $icon_next = pEFL::Elm::Icon->add($win);
$icon_next->standard_set("go-next");
$btn_next->part_content_set("icon",$icon_next);
$btn_next->size_hint_weight_set(0.0,0.0);
$btn_next->size_hint_align_set(0.0,0.0);
$btn_next->show();
$toolbar->pack_end($btn_next);
$btn_next->smart_callback_add("clicked", \&_on_fwd, undef);

my $btn_reload = pEFL::Elm::Button->add($win);
my $icon_reload = pEFL::Elm::Icon->add($win);
$icon_reload->standard_set("view-refresh");
$btn_reload->part_content_set("icon",$icon_reload);
$btn_reload->size_hint_weight_set(0.0,0.0);
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
$wv->smart_callback_add("url,changed",\&_on_wv_url_changed, undef);

$win->show();

$wv->url_set($url);

$win->smart_callback_add("delete,request",sub {print "Exiting \n"}, undef);

$win->show();

pEFL::Elm::run();

pEFL::Elm::shutdown();


sub _on_url_activated {
	my ($data, $obj, $ei) = @_;
	
	my $url = $obj->entry_get();
	return if (!$url);
	
	$url = "https://$url" unless $url =~ m{^[a-zA-Z][a-zA-Z0-9+\-.]*://};  
	my $uri = URI->new($url)->as_string;
	
	print "Set URL to $url\n";
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

sub _on_reload {
	$wv->reload;
}

sub _on_wv_url_changed {
	my ($data, $obj, $ei) = @_;
	
	my $url = pEFL::ev_info2s($ei);
	$url_entry->entry_set($url) if ($url);
}