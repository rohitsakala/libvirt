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

static void zvmDriverLock(struct zvm_driver *driver)
{
    virMutexLock(&driver->lock);
}

static void zvmDriverUnlock(struct zvm_driver *driver)
{
    virMutexUnlock(&driver->lock);
}

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

         if (conn->uri->server != NULL)
             return VIR_DRV_OPEN_DECLINED;

         if (STRNEQ_NULLABLE(conn->uri->path, "/system")) {
            virReportError(VIR_ERR_INTERNAL_ERROR,
                           _("Unexpected zvm URI path '%s', try zvm:///system"),
                           conn->uri->path);
            return VIR_DRV_OPEN_ERROR;
         }
     }

     /* We now know the URI is definitely for this driver, so beyond
      * here, don't return DECLINED, always use ERROR */

     if (VIR_ALLOC(zvm_driver) < 0)
         return VIR_DRV_OPEN_ERROR;

     /* TODO Remove the comments below */
     //if (zvmExtractVersion(zvm_driver) < 0)
     //     goto cleanup;

     if (!(zvm_driver->caps = zvmCapsInit()))
            goto cleanup;

     conn->privateData = zvm_driver;

     return VIR_DRV_OPEN_SUCCESS;

  cleanup:
     zvmFreeDriver(zvm_driver);
     return VIR_DRV_OPEN_ERROR;

}

static int
zvmConnectClose(virConnectPtr conn)
{
    struct zvm_driver *driver = conn->privateData;

    zvmFreeDriver(driver);

    conn->privateData = NULL;

    return 0;
}

static char *zvmConnectGetHostname(virConnectPtr conn ATTRIBUTE_UNUSED)
{
    return virGetHostname();
}

static const char *zvmConnectGetType(virConnectPtr conn ATTRIBUTE_UNUSED)
{
    return "ZVM";
}

static int zvmConnectGetVersion(virConnectPtr conn, unsigned long *version)
{
    struct  zvm_driver *driver = conn->privateData;
    zvmDriverLock(driver);
    *version = driver->version;
    zvmDriverUnlock(driver);
    return 0;
}

static int zvmConnectGetMaxVcpus(virConnectPtr conn ATTRIBUTE_UNUSED,
                                    const char *type)
{
    if (type == NULL || STRCASEEQ(type, "zvm"))
        return 64;

    virReportError(VIR_ERR_INVALID_ARG,
                   _("unknown type '%s'"), type);
    return -1;
}

static int zvmNodeGetInfo(virConnectPtr conn ATTRIBUTE_UNUSED,
                  virNodeInfoPtr nodeinfo)
{
    return virCapabilitiesGetNodeInfo(nodeinfo);
}

static int zvmNodeGetCPUStats(virConnectPtr conn ATTRIBUTE_UNUSED,
                      int cpuNum,
                      virNodeCPUStatsPtr params,
                      int *nparams,
                      unsigned int flags)
{
    return virHostCPUGetStats(cpuNum, params, nparams, flags);
}

static int zvmNodeGetMemoryStats(virConnectPtr conn ATTRIBUTE_UNUSED,
                         int cellNum,
                         virNodeMemoryStatsPtr params,
                         int *nparams,
                         unsigned int flags)
{
    return virHostMemGetStats(cellNum, params, nparams, flags);
}

static int zvmNodeGetCellsFreeMemory(virConnectPtr conn ATTRIBUTE_UNUSED,
                             unsigned long long *freeMems,
                             int startCell,
                             int maxCells)
{
    return virHostMemGetCellsFree(freeMems, startCell, maxCells);
}

static unsigned long long zvmNodeGetFreeMemory(virConnectPtr conn ATTRIBUTE_UNUSED)
{
    unsigned long long freeMem;
    if (virHostMemGetInfo(NULL, &freeMem) < 0)
        return 0;
    return freeMem;
}

static int zvmNodeGetCPUMap(virConnectPtr conn ATTRIBUTE_UNUSED,
                    unsigned char **cpumap,
                    unsigned int *online,
                    unsigned int flags)
{
    return virHostCPUGetMap(cpumap, online, flags);
}

static char *zvmConnectGetCapabilities(virConnectPtr conn) {
    struct zvm_driver *driver = conn->privateData;
    char *ret;

    zvmDriverLock(driver);
    ret = virCapabilitiesFormatXML(driver->caps);
    zvmDriverUnlock(driver);

    return ret;
}

static virDomainPtr
zvmDomainCreateXML(virConnectPtr conn, const char *xml,
                      unsigned int flags)
{
    struct zvm_driver *driver = conn->privateData;
    virDomainPtr ret = NULL;
    virDomainDefPtr def;
    virDomainObjPtr dom = NULL;
    virObjectEventPtr event = NULL;
    unsigned int parse_flags = VIR_DOMAIN_DEF_PARSE_INACTIVE;

    virCheckFlags(VIR_DOMAIN_START_VALIDATE, NULL);

    if (flags & VIR_DOMAIN_START_VALIDATE)
        parse_flags |= VIR_DOMAIN_DEF_PARSE_VALIDATE_SCHEMA;

    zvmDriverLock(driver);

    if ((def = virDomainDefParseString(xml, privconn->caps, privconn->xmlopt,
                                       NULL, parse_flags)) == NULL)
        goto cleanup;

    if (testDomainGenerateIfnames(def) < 0)
        goto cleanup;

    if (!(dom = virDomainObjListAdd(privconn->domains,
                                    def,
                                    privconn->xmlopt,
                                    VIR_DOMAIN_OBJ_LIST_ADD_LIVE |
                                    VIR_DOMAIN_OBJ_LIST_ADD_CHECK_LIVE,
                                    NULL)))
        goto cleanup;
    def = NULL;

    if (testDomainStartState(privconn, dom, VIR_DOMAIN_RUNNING_BOOTED) < 0) {
        if (!dom->persistent) {
            virDomainObjListRemove(privconn->domains, dom);
            dom = NULL;
        }
        goto cleanup;
    }

    event = virDomainEventLifecycleNewFromObj(dom,
                                     VIR_DOMAIN_EVENT_STARTED,
                                     VIR_DOMAIN_EVENT_STARTED_BOOTED);

    ret = virGetDomain(conn, dom->def->name, dom->def->uuid, dom->def->id);

  cleanup:
    if (dom)
        virObjectUnlock(dom);
    testObjectEventQueue(privconn, event);
    virDomainDefFree(def);
    testDriverUnlock(privconn);
    return ret;
}

/*----- Register with libvirt.c, and initialize zVM drivers. -----*/

/* The interface which we export upwards to libvirt.c. */

static virHypervisorDriver zvmHypervisorDriver = {
    .name = "ZVM",
    .connectOpen = zvmConnectOpen, /* 1.2.14 */
    .connectClose = zvmConnectClose, /* 1.2.14 */
    .connectGetType = zvmConnectGetType, /* 1.2.14 */
    .connectGetVersion = zvmConnectGetVersion, /* 1.2.14 */
    .connectGetHostname = zvmConnectGetHostname, /* 1.2.14 */
    .connectGetMaxVcpus = zvmConnectGetMaxVcpus, /* 1.2.14 */
    .connectGetCapabilities = zvmConnectGetCapabilities, /* 1.2.14 */
    .connectListDomains = zvmConnectListDomains, /* 1.2.14 */
    .nodeGetInfo = zvmNodeGetInfo, /* 1.2.14 */
    .nodeGetCPUStats = zvmNodeGetCPUStats, /* 1.2.14 */
    .nodeGetMemoryStats = zvmNodeGetMemoryStats, /* 1.2.14 */
    .nodeGetCellsFreeMemory = zvmNodeGetCellsFreeMemory, /* 1.2.14 */
    .nodeGetFreeMemory = zvmNodeGetFreeMemory, /* 1.2.14 */
    .nodeGetCPUMap = zvmNodeGetCPUMap, /* 1.2.14 */
    .domainCreateXML = zvmDomainCreateXML, /* 0.8.7 */
};

static virConnectDriver zvmConnectDriver = {
    .hypervisorDriver = &zvmHypervisorDriver,
};

int zvmRegister(void)
{
    if (virRegisterConnectDriver(&zvmConnectDriver,
                                 true) < 0)
        return -1;

    return 0;
}
