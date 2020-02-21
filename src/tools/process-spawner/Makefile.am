noinst_PROGRAMS = lms-process-spawner

lms_process_spawner_SOURCES = \
	$(srcdir)/ProcessSpawner.cpp 				\
	$(top_srcdir)/src/utils/ChildProcess.cpp 		\
	$(top_srcdir)/src/utils/ChildProcessManager.cpp 	\
	$(top_srcdir)/src/utils/Exception.cpp 			\
	$(top_srcdir)/src/utils/Logger.cpp 			\
	$(top_srcdir)/src/utils/StreamLogger.cpp

lms_process_spawner_CXXFLAGS=-std=c++17 -I$(top_srcdir)/src -D_REENTRANT

