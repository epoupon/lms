AC_PREREQ(2.59)
AC_INIT(lms, 1.0.0, test@test)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER(src/config/config.h)

AC_LANG_CPLUSPLUS

# Checks for programs
AC_PROG_CXX

AC_ARG_ENABLE([video], AS_HELP_STRING([--enable-video],
				      [Enable Video support. Default to no.])],
			[enable_video="$enableval"],
			[enable_video="no"])

AM_CONDITIONAL([VIDEO], [test x$enable_video = xyes])
AS_IF([test x$enable_video = xyes], [AC_DEFINE(HAVE_VIDEO, [1], [Enable Video support])])

PKG_CHECK_MODULES(IMAGEMAGICKXX, "ImageMagick++", [ HAVE_IMAGEMAGICKXX=yes ], [ ])
if test -n "$HAVE_IMAGEMAGICKXX"; then
	MAGICKXX_CFLAGS="$IMAGEMAGICKXX_CFLAGS"
	MAGICKXX_LIBS="$IMAGEMAGICKXX_LIBS"
fi
AC_SUBST(MAGICKXX_CFLAGS)
AC_SUBST(MAGICKXX_LIBS)

AC_CHECK_HEADERS([Wt/WApplication pstreams/pstream.h],
                 [],
                 [AC_MSG_ERROR([Header not found or unusable !])])

AC_CHECK_LIB([wt],
             [main],
             ,
             [AC_MSG_ERROR([libwt not found!])])

AC_CHECK_LIB([wtdbo],
             [main],
             ,
             [AC_MSG_ERROR([libwtdbo not found!])])

AC_CHECK_LIB([wtdbosqlite3],
             [main],
             ,
             [AC_MSG_ERROR([libwtdbosqlite3 not found!])])

AC_CHECK_LIB([wthttp],
	     [main],
	     ,
	     [AC_MSG_ERROR([libwthttp not found!])])


AC_CHECK_LIB([avutil],
	     [av_free],
	     ,
	     [AC_MSG_ERROR([libavutil not found!])])

AC_CHECK_LIB([avcodec],
	     [avcodec_find_decoder],
	     ,
	     [AC_MSG_ERROR([libavcodec not found!])])

AC_CHECK_LIB([avformat],
	     [av_read_frame],
	     ,
	     [AC_MSG_ERROR([libavformat not found!])])

AC_CHECK_LIB([boost_system],
		[main],
		,
		[AC_MSG_ERROR([libboost_system not found!])])

AC_CHECK_LIB(	[boost_filesystem],
		[main],
		,
		[AC_MSG_ERROR([libboost_filesystem not found!])])

AC_CHECK_LIB(	[boost_locale],
		[main],
		,
		[AC_MSG_ERROR([libboost_locale not found!])])

AC_CHECK_LIB(	[boost_date_time],
		[main],
		,
		[AC_MSG_ERROR([libboost_date_time not found!])])

AC_CHECK_LIB(	[boost_iostreams],
		[main],
		,
		[AC_MSG_ERROR([libboost_iostreams not found!])])

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 test/Makefile])

AC_OUTPUT


