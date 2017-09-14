dnl The zvm driver
dnl
dnl Copyright (C) 2014 Sakala Venkata Krishna Rohit
dnl
dnl This library is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU Lesser General Public
dnl License as published by the Free Software Foundation; either
dnl version 2.1 of the License, or (at your option) any later version.
dnl
dnl This library is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl Lesser General Public License for more details.
dnl
dnl You should have received a copy of the GNU Lesser General Public
dnl License along with this library.  If not, see
dnl <http://www.gnu.org/licenses/>.
dnl

AC_DEFUN([LIBVIRT_DRIVER_ARG_ZVM],[
    LIBVIRT_ARG_WITH_FEATURE([ZVM], [zvm], [check])
])

AC_DEFUN([LIBVIRT_DRIVER_CHECK_ZVM],[
    if test "$with_zvm" != "no"; then
        AC_PATH_PROG([ZVM], [smcli], [], [/opt/zhcp/bin])

        if test -z "$ZVM"; then
            if test "$with_zvm" = "check"; then
                with_zvm="no"
            else
                AC_MSG_ERROR([The zvm driver cannot be enabled])
            fi
        else
            with_zvm="yes"
        fi
    fi

    if test "$with_zvm" = "yes"; then
        AC_DEFINE_UNQUOTED([WITH_ZVM], 1, [whether zvm driver is enabled])
        AC_DEFINE_UNQUOTED([ZVM], ["$ZVM"],
                           [Location of the zvm tool])
    fi
    AM_CONDITIONAL([WITH_ZVM], [test "$with_zvm" = "yes"])
])

AC_DEFUN([LIBVIRT_DRIVER_RESULT_ZVM],[
    LIBVIRT_RESULT([ZVM], [$with_zvm])
])
