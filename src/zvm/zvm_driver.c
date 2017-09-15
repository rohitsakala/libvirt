/*
 * zvm_driver.c: zvm driver
 *
 * Copyright (C) 2017 Sakala Venkata Krishna Rohit
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Sakala Venkata Krishna Rohit <rohitsakala@gmail.com>
 */


 #include <config.h>

 #include <sys/types.h>
 #include <sys/poll.h>
 #include <sys/time.h>
 #include <dirent.h>
 #include <limits.h>
 #include <string.h>
 #include <stdio.h>
 #include <stdarg.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <errno.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <signal.h>
 #include <paths.h>
 #include <sys/wait.h>
 #include <sys/ioctl.h>
 #include <sys/un.h>
 #include <byteswap.h>


 #include "zvm_driver.h"

 #include "virerror.h"
 #include "virlog.h"
 #include "datatypes.h"
 #include "virbuffer.h"
 #include "capabilities.h"
 #include "viralloc.h"
 #include "viruuid.h"
 #include "domain_conf.h"
 #include "domain_audit.h"
 #include "node_device_conf.h"
 #include "virpci.h"
 #include "virusb.h"
 #include "virprocess.h"
 #include "libvirt_internal.h"
 #include "virxml.h"
 #include "cpu/cpu.h"
 #include "virsysinfo.h"
 #include "domain_nwfilter.h"
 #include "nwfilter_conf.h"
 #include "virhook.h"
 #include "virstoragefile.h"
 #include "virfile.h"
 #include "configmake.h"
 #include "virthreadpool.h"
 #include "virkeycode.h"
 #include "virnodesuspend.h"
 #include "virtime.h"
 #include "virtypedparam.h"
 #include "virbitmap.h"
 #include "virstring.h"
 #include "viraccessapicheck.h"

#define VIR_FROM_THIS	VIR_FROM_ZVM

VIR_LOG_INIT("zvm.zvm_driver");


static char *zvmConnectGetHostname(void)
{
    if (virConnectGetHostnameEnsureACL(conn) < 0)
        return NULL;

    return virGetHostname();
}

/*----- Register with libvirt.c, and initialize zVM drivers. -----*/

/* The interface which we export upwards to libvirt.c. */

static virDriver zvmDriver = {
    .name = "ZVM",
    .connectGetHostname = zvmConnectGetHostname,
};

static virStateDriver zvmStateDriver = {
    .name = "ZVM",
};

int zvmRegister(void)
{
    virRegisterDriver(&zvmDriver);
    virRegisterStateDriver(&zvmStateDriver);
    return 0;
}
