eWebView is a Evas SmartObject and WPE/Webkit WebViewer for the enlightenment foundation libraries.

you need the following C libraries:

* libwpe
* wpebackend-fdo
* wpewebkit (see for all https://wpewebkit.org/)

# Features

At the moment eWebView has the following features:

* rendering Web Content via shm-Buffer
* Navigation API
* Download API
* Create context-menu in Perl
* Copy & Paste API

# Smart Callbacks

As Webkit calls often are asynchronous, the use of smart callbacks is very important.

The following smart events are exposed by eWebView:

* evas_object_smart_callback_add(wv, "url,changed",   cb, data)
* evas_object_smart_callback_add(wv, "title,changed", cb, data)
* evas_object_smart_callback_add(wv, "load,started",  cb, data)
* evas_object_smart_callback_add(wv, "load,finished", cb, data)
* evas_object_smart_callback_add(wv, "load,progress", cb, data)  
	→ event_info = double*
* evas_object_smart_callback_add(wv, "download,requested", cb, data) 
	→ event_info = eWebView_DownloadRequest
* evas_object_smart_callback_add(wv, "download,finished", cb, data)  
 	→ event_info = const char* (destination path / Zielpfad)
* evas_object_smart_callback_add(wv,"download,failed", cb, data)
	→ event_info = const char* (error message / Fehlermeldung)
* evas_object_smart_callback_add(wv, "selected-text,requested", cb, data)
	→ event_info = gchar *selected_text
* evas_object_smart_callback_add(wv, "context-menu,requested", cb, data)
	→ event_info = Evas_Event_Mouse_Down *ev

# Documentation & Example

At the moment there is no detailed documentation. See ewebview.h for the public interface.

An example using almost all features is written in perl at perl/examples/ewebview_browser.pl. 

There you can get an impression how to implement WebKit stuff to efl.
