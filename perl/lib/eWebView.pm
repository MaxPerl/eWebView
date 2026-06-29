package eWebView;

use 5.006000;
use strict;
use warnings;

require Exporter;

use pEFL::Evas;
use pEFL::PLSide;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use eWebView ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('eWebView', $VERSION);

sub add {
    my ($class,$evas) = @_;
    my $widget = ewebview_add($evas);
    $widget->event_callback_add(pEFL::Evas::EVAS_CALLBACK_DEL(), \&pEFL::PLSide::cleanup, $widget);
    return $widget;
}

*new = \&add;

package eWebViewPtr;

use pEFL::Evas::Object;

our @ISA = qw(EvasObjectPtr);

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

eWebView - WPE/WebKit Widget for pEFL

=head1 SYNOPSIS

  use pEFL::Evas;
  use eWebView;

  my $evas = $win->evas_get();
  my $wv = eWebView->add($evas);
  $wv->size_hint_weight_set(EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  $wv->size_hint_align_set(EVAS_HINT_FILL, EVAS_HINT_FILL);
  $wv->show();

=head1 DESCRIPTION

eWebView is a Evas SmartObject and WPE/Webkit WebViewer for the enlightenment foundation libraries.

=head1 PREREQUESITES

you need the following C libraries und perl modules:

=over 4

=item* efl

=item* libwpe

=item* wpebackend-fdo

=item* wpewebkit

=item* L<pEFL>

=back

=head1 METHODS

eWebView has, like pEFL, an object-oriented structure. It provides the following methods:

=over 4

=item* C<my $wv = eWebView->new($evas);>

=item* C<$wv->url_set($url);>

=item* C<my $url = $wv->url_get();>

=item* C<my $title = $wv->title_get();>

=item* C<$wv->back();>

=item* C<$wv->forward();>

=item* C<$wv->reload();>

=item* C<$wv->stop();>

=item* C<my $progress = $wv->load_progress_get();>

=item* C<my $uri = $wv->download_uri_get();>

=item* C<$wv->download_save($path);>

=item* C<$wv->download_cancel();>

=item* C<$wv->focus_set($focused);>

=back 

=head1 SMART EVENTS

eWebView has the following smart events, you can register event handlers with C<$wv->smart_callback_add($event => \&sub, $data);>:

=over 4

=item* "url,changed"

=item* "title,changed"

=item* "load,started"

=item* "load,finished"

=item* "load,progress"
$event_info is void pointer with a double containing the URI; not implemented at the moment

=item* "download,started" 
$event_info is void pointer with a string containing the URI; convert it with C<pEFL::ev_info2s($event_info)>

=item* "print, requested"

=back

=head1 SEE ALSO

L<WPE Webkit Port | https://wpewebkit.org/>

L<Enlightenment Foundation Libraries|https://www.enlightenment.org/docs/start>

L<Perl-EFL Github Repository|https://github.com/MaxPerl/Perl-EFL>

=head1 AUTHOR

Maximilian Lika

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2026 by Maximilian Lika

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.40.1 or,
at your option, any later version of Perl 5 you may have available.

=cut
