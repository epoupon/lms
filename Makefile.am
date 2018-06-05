
AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

SUBDIRS = test tools src

lms_docrootdir=$(pkgdatadir)/docroot
lms_approotdir=$(pkgdatadir)/approot

lms_cssdir=$(lms_docrootdir)/css
lms_imagesdir=$(lms_docrootdir)/images
lms_jsdir=$(lms_docrootdir)/js

lms_css_DATA = \
		docroot/css/lms.css

lms_images_DATA = \
		docroot/images/unknown-cover.jpg	\
		docroot/images/unknown-artist.jpg

lms_js_DATA = \
	      	docroot/js/mediaplayer.js

lms_approot_DATA = \
		approot/admin-database.xml	\
		approot/admin-user.xml		\
		approot/admin-users.xml		\
		approot/admin-initwizard.xml	\
		approot/artist.xml	\
		approot/artists.xml	\
		approot/explore.xml	\
		approot/login.xml	\
		approot/mediaplayer.xml	\
		approot/messages.xml	\
		approot/playqueue.xml	\
		approot/release.xml	\
		approot/releases.xml	\
		approot/settings.xml	\
		approot/templates.xml	\
		approot/tracks.xml

sysconf_DATA = \
	        $(top_srcdir)/conf/lms.conf

