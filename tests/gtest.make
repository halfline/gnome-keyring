
# The following need to be declared before this file is included:
#   UNIT_AUTO     A list of C files with tests
#   UNIT_PROMPT   A list of C files with prompting tests
#   UNIT_LIBS     Libraries to link the tests to

# ------------------------------------------------------------------------------

INCLUDES=				\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/daemon 		\
	-I$(top_builddir) 		\
	$(GTK_CFLAGS)			\
	$(GLIB_CFLAGS)  
	
LIBS = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) 
	
noinst_PROGRAMS= \
	run-auto-test \
	run-prompt-test
	
run-auto-test.c: $(UNIT_AUTO) Makefile.am $(top_srcdir)/tests/prep-gtest.sh
	sh $(top_srcdir)/tests/prep-gtest.sh -b run-auto-test $(UNIT_AUTO)

run_auto_test_SOURCES = \
	run-auto-test.c run-auto-test.h \
	$(UNIT_AUTO)
	
run_auto_test_LDADD = \
	$(UNIT_LIBS) \
	$(DAEMON_LIBS)
	
run_auto_test_CFLAGS = \
	$(UNIT_FLAGS)

run-prompt-test.c: $(UNIT_PROMPT) Makefile.am $(top_srcdir)/tests/prep-gtest.sh
	sh $(top_srcdir)/tests/prep-gtest.sh -b run-prompt-test $(UNIT_PROMPT)

run_prompt_test_SOURCES = \
	run-prompt-test.c run-prompt-test.h \
	$(UNIT_PROMPT)
	
run_prompt_test_LDADD = \
	$(UNIT_LIBS) \
	$(DAEMON_LIBS)

run_prompt_test_CFLAGS = \
	$(UNIT_FLAGS)

# ------------------------------------------------------------------------------
# Run the tests

test-auto: $(noinst_PROGRAMS)
	gtester -k -m=slow ./run-auto-test

test-prompt: $(noinst_PROGRAMS)
	gtester -k -m=slow ./run-prompt-test