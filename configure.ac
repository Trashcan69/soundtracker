dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([SoundTracker], [1.0.5], [soundtracker-discuss@lists.sourceforge.net], [soundtracker])
AC_CONFIG_SRCDIR([app/endian-conv.h])
AM_INIT_AUTOMAKE([1.11])

AM_CONFIG_HEADER(config.h)

dnl -----------------------------------------------------------------------
dnl Test for GCC
dnl -----------------------------------------------------------------------

AC_PROG_CC
AC_PATH_PROG(sedpath, sed)

# Commonly distributed binutils (e.g., Debian) comes built with
# --enable-deterministic-archives which makes ar default to option "D".
# Option "D" overrides "u" and if "u" is given a cautionary message will be
# printed.  Autoconf seems to set options "cru" by default.  Avoid the
# annoying message by removing "u", and, for completeness, explicitly put "D"
# as well.
ARFLAGS=crD
AC_SUBST(ARFLAGS)

if test $GCC != yes; then
    echo "Fatal error: Need gcc"
    exit 1
fi

CFLAGS="$CFLAGS -Wall -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED"

dnl -----------------------------------------------------------------------
dnl Test for poll()
dnl -----------------------------------------------------------------------

AC_CHECK_HEADER(sys/poll.h,
    [AC_CHECK_FUNCS(poll, [], 
        [AC_CHECK_FUNC(select, [], [AC_MSG_ERROR(no poll and no select?!?)])]
        )
    ]
)

dnl -----------------------------------------------------------------------
dnl Initialize NLS
dnl -----------------------------------------------------------------------

dnl Also add new catalogs to soundtracker.spec (%build)!
#ALL_LINGUAS="da de es fr gl hr it ja no pl ru sk sl sv"
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl -----------------------------------------------------------------------
dnl Test for required GTK+
dnl -----------------------------------------------------------------------
PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.24 glib-2.0 >= 2.32 gthread-2.0 gmodule-2.0 x11])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

dnl -----------------------------------------------------------------------
dnl Test for required libxml2
dnl -----------------------------------------------------------------------
AM_PATH_XML2(2.6.0, [], AC_MSG_ERROR(Fatal error: Need libxml2 >= 2.6.0))

LIBS="$LIBS $GTK_LIBS $XML_LIBS"
CFLAGS="$CFLAGS $GTK_CFLAGS $XML_CPPFLAGS"

dnl -----------------------------------------------------------------------
dnl Test for optional GooCanvas
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(canvas,
AS_HELP_STRING([--disable-canvas], [Disable use of GooCanvas (default = try)]),
        canvas=no)

if test x$canvas != xno; then
  PKG_CHECK_MODULES(CANVAS, [goocanvas], [canvas=yes], [canvas=no])
  if test "$canvas" = yes; then
    AC_SUBST(CANVAS_CFLAGS)
    AC_SUBST(CANVAS_LIBS)
    LIBS="$LIBS $CANVAS_LIBS"
    CFLAGS="$CFLAGS $CANVAS_CFLAGS"
    AC_DEFINE([USE_CANVAS], 1, [Set if GooCanvas support is compiled in])
  fi
fi

AM_CONDITIONAL(USE_CANVAS, test "x$canvas" = "xyes")

dnl -----------------------------------------------------------------------
dnl Test for libsndfile
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(sndfile,
AS_HELP_STRING([--disable-sndfile], [Disable usage of libsndfile (default = try)]),
sndfile_support=no)

ac_cv_sndfile=0

if test x$sndfile_support != xno ; then
  PKG_CHECK_MODULES(SNDFILE, [sndfile > 1.0.1], ac_cv_sndfile=1, ac_cv_sndfile=0)
  if test x$ac_cv_sndfile = x1 ; then
    CFLAGS="$CFLAGS $SNDFILE_CFLAGS"
    LIBS="$LIBS $SNDFILE_LIBS"
  fi
fi

AC_DEFINE_UNQUOTED(USE_SNDFILE,${ac_cv_sndfile},
    [Set to 1 if you wish to use libsndfile.])

dnl -----------------------------------------------------------------------
dnl Test for audiofile library (if libsndfile is not found or disabled)
dnl -----------------------------------------------------------------------

if test x$ac_cv_sndfile != x1 ; then
    PKG_CHECK_MODULES(AUDIOFILE, audiofile > 0.1.5, ac_cv_audiofile=1,
        [AC_MSG_RESULT([* No sample I/O library found, disabling sample loader (ignore error above).])]
    )
    if test x$ac_cv_audiofile = x1; then
        CFLAGS="$CFLAGS $AUDIOFILE_CFLAGS"
        LIBS="$LIBS $AUDIOFILE_LIBS"
        AF_VERSION=$(pkg-config --modversion audiofile | sed "s/^[[0-9]]*\.//;s/\..*$//")
        AC_DEFINE_UNQUOTED([AUDIOFILE_VERSION],
                  ${AF_VERSION},
                  [Audiofile minor version for use in headers])
    fi
fi

dnl -----------------------------------------------------------------------
dnl Checks for header files
dnl -----------------------------------------------------------------------

AC_HEADER_STDC
AC_CHECK_FUNCS(setresuid)

dnl -----------------------------------------------------------------------
dnl Test for OSS headers
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(oss,
AS_HELP_STRING([--disable-oss], [Disable OSS driver (default = try)]),
oss_support=no)

if test x$oss_support != xno; then
  AC_CHECK_HEADERS(sys/soundcard.h machine/soundcard.h soundcard.h)
  if test "${ac_cv_header_sys_soundcard_h}" = "yes" || \
     test "${ac_cv_header_machine_soundcard_h}" = "yes" || \
     test "${ac_cv_header_soundcard_h}" = "yes"; then
     oss_support=yes
     AC_DEFINE([DRIVER_OSS], 1, [Set if OSS driver wanted])
     case `uname` in
        OpenBSD*)
        LIBS="$LIBS -lossaudio"
        ;;
    NetBSD*)
        AC_CHECK_LIB(ossaudio, _oss_ioctl, LIBS="$LIBS -lossaudio")
    ;;
     esac
  else
    oss_support=no
  fi
fi

AM_CONDITIONAL(DRIVER_OSS, test x$oss_support = xyes)

dnl -----------------------------------------------------------------------
dnl Test for ALSA
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(alsa,
AS_HELP_STRING([--disable-alsa], [Disable ALSA driver (default = try)]),
alsa_support=no)

if test x$alsa_support != xno; then
  AM_PATH_ALSA([1.0.29], have_alsa=yes, have_alsa=no)
  if test x$have_alsa = xyes; then
    AC_DEFINE([DRIVER_ALSA], 1, [Set if ALSA driver wanted])
    CFLAGS="$CFLAGS $ALSA_CFLAGS"
    LIBS="$LIBS $ALSA_LIBS"
  fi
fi

AM_CONDITIONAL(DRIVER_ALSA, test x$have_alsa = xyes)

dnl -----------------------------------------------------------------------
dnl Test for JACK
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(jack,
AS_HELP_STRING([--disable-jack], [Disable JACK support (default = try)]),
jack_support=no)

if test x$jack_support != xno; then
  PKG_CHECK_MODULES(JACK, [jack > 0.120.0], jack_support=yes, jack_support=no)
    if test x$jack_support = xyes; then
      CFLAGS="$CFLAGS $JACK_CFLAGS"
      LIBS="$LIBS $JACK_LIBS"
      AC_DEFINE([DRIVER_JACK], 1, [Set if JACK support wanted])
    fi
fi

AM_CONDITIONAL(DRIVER_JACK, test x$jack_support = xyes)

dnl -----------------------------------------------------------------------
dnl Test for SGI audio
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(sgi,
AS_HELP_STRING([--disable-sgi], [Disable SGI driver (default = try)]),
sgi_support=no)

case `uname` in
  IRIX*)
    if test x$sgi_support != xno; then
    AC_CHECK_HEADER(dmedia/audio.h)
    if test $ac_cv_header_dmedia_audio_h = yes
    then
      AC_CHECK_LIB(audio,ALseterrorhandler,driver_sgi=yes)
      if test $driver_sgi = yes
      then
        AC_DEFINE([DRIVER_SGI], 1, [Set if SGI driver wanted])
        LIBS="$LIBS -laudio"
        irix_support=yes
      fi
    fi
  fi
  ;;
esac

AM_CONDITIONAL(DRIVER_IRIX, test x$irix_support = xyes)

dnl -----------------------------------------------------------------------
dnl Test for Sun audio
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(sun,
AS_HELP_STRING([--disable-sun], [Disable Sun driver (default = try)]),
sun_support=no)

if test x$sun_support != xno; then
  AC_CHECK_HEADERS(sys/audioio.h)
  if test "${ac_cv_header_sys_audioio_h}" = "yes"; then
    AC_DEFINE([DRIVER_SUN], 1, [Set if Sun audio driver wanted])
  fi
fi

AM_CONDITIONAL(DRIVER_SUN, test "${ac_cv_header_sys_audioio_h}" = "yes")

dnl -----------------------------------------------------------------------
dnl Test for SDL audio
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(sdl,
AS_HELP_STRING([--disable-sdl], [Disable SDL driver (default = try)]),
sdl_support=no)

if test x$sdl_support != xno; then
  AM_PATH_SDL(1.2.0, have_sdl=yes, have_sdl=no)
  if test x$have_sdl = xyes; then
    AC_DEFINE([DRIVER_SDL], 1, [Set if SDL audio driver wanted])
    CFLAGS="$CFLAGS $SDL_CFLAGS"
    LIBS="$LIBS $SDL_LIBS"
  fi
fi

AM_CONDITIONAL(DRIVER_SDL, test x$have_sdl = xyes)

dnl -----------------------------------------------------------------------
dnl Test for Pulseaudio
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(pulse,
AS_HELP_STRING([--disable-pulse], [Disable Pulseaudio driver (default = try)]),
pulse_support=no)

if test x$pulse_support != xno; then
  PKG_CHECK_MODULES(PULSE, [libpulse], pulse_support=yes, pulse_support=no)
    if test x$pulse_support = xyes; then
        CFLAGS="$CFLAGS $PULSE_CFLAGS"
        LIBS="$LIBS $PULSE_LIBS"
        AC_DEFINE([DRIVER_PULSE], 1, [Set if Pulseaudio driver wanted])
    fi
fi

AM_CONDITIONAL(DRIVER_PULSE, test x$pulse_support = xyes)

dnl -----------------------------------------------------------------------
dnl Selecting graphics backend
dnl -----------------------------------------------------------------------

AC_ARG_WITH([graphics-backend],
AS_HELP_STRING([--with-graphics-backend[=@<:@gdk/X11@:>@]], [Backend used for low-level graphics (default = X11)]),
[graphics_backend=$withval], [graphics_backend=X11])
AS_CASE([$graphics_backend], [gdk], [],
    [X11], AC_CHECK_HEADER(X11/Xlib.h, [
        AC_DEFINE([BACKEND_X11], 1, [Set if X11 graphics backend is used])
        LIBS="$LIBS -lX11"
        CFLAGS="$CFLAGS  -DGDK_DISABLE_DEPRECATED"],  [graphics_backend=gdk]),
    [AC_MSG_ERROR([Invalid argument passed to --with-graphics-backend, should be one of @<:@gdk/X11@:>@])])
AC_MSG_NOTICE([Selecting the graphics backend... $graphics_backend])

dnl -----------------------------------------------------------------------
dnl Other tests
dnl -----------------------------------------------------------------------

AC_PROG_RANLIB

AC_SEARCH_LIBS([rint], [m], [], [
  AC_MSG_ERROR([unable to find the rint() function])
])

AC_C_BIGENDIAN

dnl Check if gtk+ tweaks are used (may cause problems in runtime).
AC_ARG_ENABLE(hacks,
AS_HELP_STRING([--enable-hacks], [Enable Gtk+ hacks (default = no)]),
gtk_hacks=yes)

if test x$gtk_hacks = xyes; then
  AC_DEFINE([GTK_HACKS], 1, [Set if gtk hacks are used])
fi

dnl Check if soundtracker shall be installed suid root
AC_ARG_ENABLE([suid_root],
AS_HELP_STRING([--enable-suid_root], [Install ST suid root (default = no)]),
suid_root=yes)

AM_CONDITIONAL(SUID_ROOT, test x$suid_root = xyes)

dnl Set PREFIX in config.h.
if test "x${prefix}" = "xNONE"; then
  AC_DEFINE_UNQUOTED(PREFIX, "${ac_default_prefix}", [Kein Mensch weiss, warum])
else
  AC_DEFINE_UNQUOTED(PREFIX, "${prefix}", [Kein Mensch weiss, warum])
fi

AC_OUTPUT([
Makefile
app/Makefile
app/drivers/Makefile
app/mixers/Makefile
po/Makefile.in
doc/Makefile
])
