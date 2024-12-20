## Process this file with automake to produce Makefile.in

SUBDIRS = po doc app

EXTRA_DIST = config.rpath TODO FAQ ABOUT-NLS soundtracker.spec \
	soundtracker.desktop \
	soundtracker_splash.png muted.png unmuted.png \
	icons/*.xpm icons/*.png \
	ChangeLog-v0.6 ChangeLog-v0.5 ChangeLog-v0.3 ChangeLog-v0.1 \
	soundtracker.xml envelope-box.xml sample-editor.xml cheat-sheet.*.txt \
	soundtracker-icon.png soundtracker.1 soundtracker.desktop \
	soundtracker.appdata.xml \
	extensions/*/*

man_MANS = soundtracker.1
dist_bin_SCRIPTS = soundtracker_convert_config

app_DATA = soundtracker.desktop
appdir = $(datadir)/applications
appdata_DATA  = soundtracker.appdata.xml
appdatadir = $(datadir)/appdata
icon_DATA = soundtracker-icon.png
icondir = $(datadir)/pixmaps
se_extensions_DATA = extensions/sample-editor/*
se_extensionsdir = $(pkgdatadir)/extensions/sample-editor

st_DATA = soundtracker_splash.png muted.png unmuted.png \
	icons/*.xpm icons/*.png \
	*.ui cheat-sheet.*.txt \
	soundtracker-icon.png
stdir = $(pkgdatadir)

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST += m4
