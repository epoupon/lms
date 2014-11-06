AC_PREREQ(2.59)
AC_INIT(lms, 1.0.0, test@test)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER(include/config.h)

AC_LANG_CPLUSPLUS

# Checks for programs
AC_PROG_CXX


AC_ARG_ENABLE(remote, AS_HELP_STRING([--enable-remote],
	    				[Build with the remote protocol support. Defaults to yes.])],
		[enable_remote="$enableval"],
		[enable_remote="yes"])

# ProtoBuf compiler.
# First, specify with --with-protoc=/path/of/protoc.
# Or, specify with env variable PROTOC.
# If neither of the above, find it in the path.
AS_IF([test "$enable_remote" = "yes"],
      [AC_ARG_WITH([protoc],
	    [AS_HELP_STRING([--with-protoc=/path/of/protoc],
			    [Location of the protocol buffers compiler protoc. Defaults to looking on path.])],
	    [PROTOC="$withval"],
	    [ AS_IF([test "x${PROTOC}" == "x"],
		    [AC_PATH_PROG([PROTOC], [protoc], [no])])]
	  )
	]
      	[ AS_IF([test "${PROTOC}" == "no"], [AC_MSG_ERROR([ProtoBuf compiler "protoc" not found.])]) ]
	[AC_CHECK_LIB(	[protobuf],
	     	[main],
		,
		[AC_MSG_ERROR([libprotobuf not found!])])]
      [AC_DEFINE(REMOTE_ENABLE, 1, [Remote support disabled])]
      ,
      [AC_DEFINE(REMOTE_ENABLE, 0, [Remote support enabled])]
)

AC_CHECK_HEADERS([Wt/WApplication jpeglib.h],
                 [],
                 [AC_MSG_ERROR([Header not found or unusable !])])

AC_CHECK_LIB([config++],
	     [main],
	     ,
	     [AC_MSG_ERROR([config++ not found!])])

AC_CHECK_LIB([jpeg],
	     [jpeg_destroy],
	     ,
	     [AC_MSG_ERROR([libpthread not found!])])

AC_CHECK_LIB([pthread],
	     [pthread_create],
	     ,
	     [AC_MSG_ERROR([libpthread not found!])])

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

AC_CHECK_LIB([ssl],
	     [SSL_CTX_new],
	     ,
	     [AC_MSG_ERROR([libssl not found!])])

AC_CHECK_LIB([crypto],
	     [ASN1_STRING_to_UTF8],
	     ,
	     [AC_MSG_ERROR([libcrypto not found!])])

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

AC_CHECK_LIB(	[boost_thread],
 		[main],
		,
		[AC_MSG_ERROR([libboost_thread not found!])])

AC_CHECK_LIB(	[boost_date_time],
		[main],
		,
		[AC_MSG_ERROR([libboost_date_time not found!])])

#AC_CHECK_LIB(	[boost_signals],
#		[main],
#		,
#		[AC_MSG_ERROR([libboost_signals not found!])])

AC_CHECK_LIB(	[boost_iostreams],
		[main],
		,
		[AC_MSG_ERROR([libboost_iostreams not found!])])

AC_CHECK_LIB(	[boost_log],
		[main],
		,
		[AC_MSG_ERROR([libboost_log not found!])])

AC_CHECK_LIB(	[boost_log_setup],
		[main],
		,
		[AC_MSG_ERROR([libboost_log_setup not found!])])

# TODO -> PARAMETRIZE THIS
AC_CHECK_LIB([wthttp],
	     [main],
	     ,
	     [AC_MSG_ERROR([libwthttp not found!])])

AC_CHECK_LIB([wtdbosqlite3],
             [main],
             ,
             [AC_MSG_ERROR([libwtdbosqlite3 not found!])])

AC_CHECK_LIB([wtdbo],
             [main],
             ,
             [AC_MSG_ERROR([libwtdbo not found!])])

AC_CHECK_LIB([wt],
             [main],
             ,
             [AC_MSG_ERROR([libwt not found!])])


AC_CONFIG_FILES([Makefile
		 src/Makefile
		 test/Makefile])

AC_OUTPUT


