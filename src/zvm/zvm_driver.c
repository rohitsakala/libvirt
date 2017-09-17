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

 #include <fcntl.h>
 #include <sys/utsname.h>

 #include "virerror.h"
 #include "datatypes.h"
 #include "virbuffer.h"
 #include "viruuid.h"
 #include "configmake.h"
 #include "viralloc.h"
 #include "network_conf.h"
 #include "interface_conf.h"
 #include "domain_audit.h"
 #include "domain_event.h"
 #include "snapshot_conf.h"
 #include "virfdstream.h"
 #include "storage_conf.h"
 #include "node_device_conf.h"
 #include "virdomainobjlist.h"
 #include "virxml.h"
 #include "virthread.h"
 #include "virlog.h"
 #include "virfile.h"
 #include "virtypedparam.h"
 #include "virrandom.h"
 #include "virstring.h"
 #include "cpu/cpu.h"
 #include "viraccessapicheck.h"
 #include "virhostcpu.h"
 #include "virhostmem.h"
 #include "virportallocator.h"
 #include "conf/domain_capabilities.h"

 #include "zvm_driver.h"
 #include "zvm_conf.h"

#define VIR_FROM_THIS	VIR_FROM_ZVM

VIR_LOG_INIT("zvm.zvm_driver");

static struct zvm_driver *zvm_driver;

/* Functions */

static virDrvOpenStatus
zvmConnectOpen(virConnectPtr conn,
                 virConnectAuthPtr auth ATTRIBUTE_UNUSED,
                 virConfPtr conf ATTRIBUTE_UNUSED,
                 unsigned int flags)
{
     virCheckFlags(VIR_CONNECT_RO, VIR_DRV_OPEN_ERROR);

     if (conn->uri == NULL) {
         if (zvm_driver == NULL)
             return VIR_DRV_OPEN_DECLINED;

         if (!(conn->uri = virURIParse("zvm:///system")))
             return VIR_DRV_OPEN_ERROR;
     } else {
         if (!conn->uri->scheme || STRNEQ(conn->uri->scheme, "zvm"))
             return VIR_DRV_OPEN_DECLINED;

         if (conn->uri->server)
             return VIR_DRV_OPEN_DECLINED;

         if (STRNEQ_NULLABLE(conn->uri->path, "/system")) {
            virReportError(VIR_ERR_INTERNAL_ERROR,
                           _("Unexpected zvm URI path '%s', try zvm:///system"),
                           conn->uri->path);
            return VIR_DRV_OPEN_ERROR;
         }

         if (zvm_driver == NULL) {
             virReportError(VIR_ERR_INTERNAL_ERROR,
                            "%s", _("ZVM state driver is not active"));
             return VIR_DRV_OPEN_ERROR;
         }
     }

     if (virConnectOpenEnsureACL(conn) < 0)
         return VIR_DRV_OPEN_ERROR;

     conn->privateData = zvm_driver;

     return VIR_DRV_OPEN_SUCCESS;
}

static int
zvmConnectClose(virConnectPtr conn)
{
    struct zvm_driver *driver = conn->privateData;

    zvmFreeDriver(driver);

    conn->privateData = NULL;

    return 0;
}

static char *zvmConnectGetHostname(virConnectPtr conn)
{
    if (virConnectGetHostnameEnsureACL(conn) < 0)
        return NULL;

    return virGetHostname();
}

static int zvmStateCleanup(void)
{
    if (!zvm_driver)
        return -1;

    virMutexDestroy(&zvm_driver->lock);
    VIR_FREE(zvm_driver);

    return 0;
}

static int zvmStateInitialize(bool privileged,
                              virStateInhibitCallback callback ATTRIBUTE_UNUSED,
                              void *opaque ATTRIBUTE_UNUSED)
{

    if (!privileged) {
      VIR_INFO("Not running privileged, disabling driver");
      return 0;
    }

    if (VIR_ALLOC(zvm_driver) < 0)
        return -1;

    if (virMutexInit(&zvm_driver->lock) < 0) {
        VIR_FREE(zvm_driver);
        return -1;
    }

    zvm_driver->hostsysinfo = virSysinfoRead();
        goto cleanup;

    return 0;

 cleanup:
    zvmStateCleanup();
    return -1;
}

static const char *zvmConnectGetType(virConnectPtr conn) {

    if (virConnectGetTypeEnsureACL(conn) < 0)
        return NULL;

    return "ZVM";
}

/*----- Register with libvirt.c, and initialize zVM drivers. -----*/

/* The interface which we export upwards to libvirt.c. */

static virHypervisorDriver zvmHypervisorDriver = {
    .name = "ZVM",
    .connectOpen = zvmConnectOpen, /* 0.2.0 */
    .connectClose = zvmConnectClose, /* 0.2.0 */
    .connectGetType = zvmConnectGetType, /* 0.2.0 */
    .connectGetHostname = zvmConnectGetHostname, /* 0.2.0 */
};

static virConnectDriver zvmConnectDriver = {
    .hypervisorDriver = &zvmHypervisorDriver,
};

static virStateDriver zvmStateDriver = {
    .name = "ZVM",
    .stateInitialize = zvmStateInitialize,
    .stateCleanup = zvmStateCleanup,
};

int zvmRegister(void)
{
    if (virRegisterConnectDriver(&zvmConnectDriver,
                                 true) < 0)
        return -1;
    if (virRegisterStateDriver(&zvmStateDriver) < 0)
        return -1;
    return 0;
}
