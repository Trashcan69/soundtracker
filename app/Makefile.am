SUBDIRS = drivers mixers

bin_PROGRAMS = soundtracker

soundtracker_SOURCES = \
	audio-subs.c audio-subs.h \
	audio.c audio.h \
	audioconfig.c audioconfig.h \
	cheat-sheet.c cheat-sheet.h \
	clavier.c clavier.h \
	clock.c clock.h \
	colors.c colors.h \
	customdrawing.c customdrawing.h \
	draw-interlayer.c draw-interlayer.h \
	driver.h \
	endian-conv.c endian-conv.h \
	envelope-box.c envelope-box.h \
	errors.c errors.h \
	event-waiter.c event-waiter.h \
	extspinbutton.c extspinbutton.h \
	file-operations.c file-operations.h \
	gui-settings.c gui-settings.h \
	gui-subs.c gui-subs.h \
	gui.c gui.h \
	history.c history.h \
	instrument-editor.c instrument-editor.h \
	keys.c keys.h \
	loop-factory.c loop-factory.h \
	main.c main.h \
	marshal.c marshal.h\
	menubar.c menubar.h \
	midi-settings.c mixer.h \
	module-info.c module-info.h \
	playlist.c playlist.h \
	poll.c poll.h \
	preferences.c preferences.h \
	recode.c recode.h \
	sample-display.c sample-display.h \
	sample-editor.c sample-editor.h \
	sample-editor-extensions.c sample-editor-extensions.h \
	scalablepic.c scalablepic.h \
	scope-group.c scope-group.h \
	st-subs.c st-subs.h \
	time-buffer.c time-buffer.h \
	tips-dialog.c tips-dialog.h \
	tracer.c tracer.h \
	track-editor.c track-editor.h \
	tracker.c tracker.h \
	tracker-settings.c tracker-settings.h \
	transposition.c transposition.h \
	xm.c xm.h \
	xm-player.c xm-player.h

if DRIVER_ALSA
  soundtracker_SOURCES += midi.c midi-utils.c \
	midi.h midi-settings.h midi-utils.h
endif

soundtracker_LDADD = drivers/libdrivers.a mixers/libmixers.a

if SUID_ROOT
install-exec-local:
	case `uname` in \
	  OpenBSD) \
	    chown root.wheel $(bindir)/soundtracker; \
	    ;; \
	  NetBSD) \
	    chown root.wheel $(bindir)/soundtracker; \
	    ;; \
	  Darwin) \
	    chown root:admin $(bindir)/soundtracker; \
	    ;; \
	  *) \
	    chown root:root $(bindir)/soundtracker; \
	    ;; \
	esac
	chmod +s $(bindir)/soundtracker
	@echo ""
	@echo "***"
	@echo "*** Installing SoundTracker suid root."
	@echo "*** This improves sound responsivity, but may be a security risk"
	@echo "*** if this is a multi-user machine. See file INSTALL."
	@echo "***"
	@echo ""
endif

marshal.h: marshal.list
	glib-genmarshal --header --prefix=__marshal marshal.list > marshal.h

marshal.c: marshal.list
	glib-genmarshal --body --prefix=__marshal marshal.list > marshal.c

playlist.c: marshal.h

sample-display.c: marshal.h

clavier.c: marshal.h

tracker.c: marshal.h

AM_CPPFLAGS = -DLOCALEDIR=\"$(datadir)/locale\" -DDATADIR=\"$(datadir)\"

EXTRA_DIST = marshal.list

CLEANFILES = marshal.c marshal.h
