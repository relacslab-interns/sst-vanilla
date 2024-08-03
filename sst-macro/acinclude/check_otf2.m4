
AC_DEFUN([CHECK_OTF2], [

SAVE_CPPFLAGS="$CPPFLAGS" 
SAVE_LDFLAGS="$LDFLAGS"
SAVE_LIBS="$LIBS"

AC_ARG_WITH(otf2,
  [AS_HELP_STRING(
    [--with-otf2],
    [Enable OTF2 features by specificying OTF2 installation],
    )],
  [ enable_otf2=$withval ], 
  [ enable_otf2=no ]
)
SHOULD_HAVE_OTF2=no

#either $enableval is yes,no if yes, then system install of otf2
#if custom, folder specified then add to cppflags and ldflags 
if test "X$enable_otf2" = "Xyes"; then
  SHOULD_HAVE_OTF2=yes
  LIBS="-lotf2"
else
  if test "X$enable_otf2" != "Xno"; then
    SHOULD_HAVE_OTF2=yes
    LDFLAGS="-L$enable_otf2/lib"
    LIBS="-lotf2"
    CPPFLAGS="-I$enable_otf2/include"
  fi
fi

#check header 
AC_CHECK_HEADER([otf2/otf2.h],
  [HAVE_OTF2=yes],
  [HAVE_OTF2=no]
)

if test "X$SHOULD_HAVE_OTF2" = "Xyes" -a "X$HAVE_OTF2" != "Xyes"; then
  AC_MSG_ERROR([OTF2 libraries required by --with-otf2 not found])
fi

AM_CONDITIONAL([HAVE_OTF2], [test "x$HAVE_OTF2" = "xyes" -a "X$enable_otf2" != "X$no"])

if test "x$HAVE_OTF2" = "xyes" -a "X$enable_otf2" != "X$no"; then
build_otf2=yes
AC_DEFINE([OTF2_ENABLED],,[Define OTF2 support as enabled])
else
build_otf2=no
fi

#check lib - try this later
OTF2_CPPFLAGS=$CPPFLAGS
OTF2_LDFLAGS=$LDFLAGS
OTF2_LIBS=$LIBS

AC_SUBST([OTF2_CPPFLAGS])
AC_SUBST([OTF2_LDFLAGS])
AC_SUBST([OTF2_LIBS])

CPPFLAGS="$SAVE_CPPFLAGS"
LDFLAGS="$SAVE_LDFLAGS" 
LIBS="$SAVE_LIBS"

])

