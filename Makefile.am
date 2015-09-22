
AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

SUBDIRS = test src

lms_docrootdir=$(pkgdatadir)/docroot
lms_approotdir=$(pkgdatadir)/approot

lms_cssdir=$(lms_docrootdir)/css
lms_imagesdir=$(lms_docrootdir)/images

lms_css_DATA = \
		 docroot/css/lms.css

lms_images_DATA = \
		 docroot/images/unknown-cover.jpg

lms_approot_DATA = \
		approot/templates.xml

install-data-local:
	ln -s $(datadir)/Wt/resources $(lms_docrootdir)/resources
