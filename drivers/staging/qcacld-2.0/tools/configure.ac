#                                               -*- Autoconf -*-

# configure.ac -- Autoconf script for qcacld-tools
#

# Process this file with autoconf to produce a configure script

# Requires autoconf tool later than 2.61
AC_PREREQ(2.61)
# Initialize the qcacld-tools package version 1.0.0
AC_INIT([qcacld-tools],1.0.0)
# Does not strictly follow GNU Coding standards
AM_INIT_AUTOMAKE([foreign])
# Disables auto rebuilding of configure, Makefile.ins
AM_MAINTAINER_MODE
# defines some macros variable to be included by source
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
PKG_CHECK_MODULES([DIAG], [diag])
AC_SUBST([DIAG_CFLAGS])
AC_SUBST([DIAG_LIBS])

AC_ARG_ENABLE([debug],
        [  --enable-debug    Turn on debugging],
        [case "${enableval}" in
            yes) debug=true ;;
            no)  debug=false ;;
            *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
        esac],[debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC

AC_CONFIG_FILES([ \
        Makefile \
		fwdebuglog/Makefile \
		athdiag/Makefile \
		pktlog/Makefile \
        ])
AC_OUTPUT

