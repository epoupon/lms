
AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

SUBDIRS = test tools src

lms_docrootdir=$(pkgdatadir)/docroot
lms_approotdir=$(pkgdatadir)/approot

lms_cssdir=$(lms_docrootdir)/css
lms_jsdir=$(lms_docrootdir)/js
lms_imagesdir=$(lms_approotdir)/images

dist_pkgdata_DATA = \
		systemd/default.service	\
	        conf/lms.conf

dist_lms_css_DATA = \
		docroot/css/lms.css

dist_lms_js_DATA = \
	      	docroot/js/bootstrap-notify.js	\
	      	docroot/js/jquery-1.10.2.min.js	\
	      	docroot/js/mediaplayer.js

dist_lms_images_DATA = \
		approot/images/unknown-cover.jpg	\
		approot/images/unknown-artist.jpg

dist_lms_approot_DATA = \
		approot/admin-database.xml	\
		approot/admin-user.xml		\
		approot/admin-users.xml		\
		approot/admin-initwizard.xml	\
		approot/artist.xml	\
		approot/artistinfo.xml	\
		approot/artistlink.xml	\
		approot/artists.xml	\
		approot/artistsinfo.xml	\
		approot/explore.xml	\
		approot/login.xml	\
		approot/mediaplayer.xml	\
		approot/messages.xml	\
		approot/messages_fr.xml	\
		approot/playhistory.xml	\
		approot/playqueue.xml	\
		approot/release.xml	\
		approot/releaseinfo.xml	\
		approot/releaselink.xml	\
		approot/releases.xml	\
		approot/releasesinfo.xml \
		approot/settings.xml	\
		approot/templates.xml	\
		approot/tracks.xml	\
		approot/tracksinfo.xml

