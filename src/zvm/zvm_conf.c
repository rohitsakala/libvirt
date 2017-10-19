/*---------------------------------------------------------------------------*/
/*
 * Copyright (C) 2011-2014 Red Hat, Inc.
 * Copyright (C) 2010-2014, diateam (www.diateam.net)
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
 */
/*---------------------------------------------------------------------------*/

#include <config.h>

#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "zvm_conf.h"
#include "viruuid.h"
#include "virbuffer.h"
#include "virconf.h"
#include "viralloc.h"
#include "virlog.h"
#include "vircommand.h"
#include "virnetdevtap.h"
#include "virnodesuspend.h"
#include "virstring.h"

#define VIR_FROM_THIS VIR_FROM_ZVM

VIR_LOG_INIT("zvm.zvm_driver");

/* Free all memory associated with a vmware_driver structure */

void zvmFreeDriver(struct zvm_driver *driver)
{
    if (!driver)
        return;

    virMutexDestroy(&driver->lock);

    VIR_FREE(driver);
}

static int zvmExtractVersionInfo(int *retversion)
{
    int ret = -1;
    char *help = NULL;
    const char *tmp;
    unsigned long version;
    virCommandPtr cmd = virCommandNewArgList(ZVM,"Query_API_Functional_Level","-T","zhcp",NULL);

    if (retversion)
        *retversion = 0;

    virCommandSetOutputBuffer(cmd, &help);

    if (virCommandRun(cmd, NULL) < 0)
        goto cleanup;

    tmp = help;

    /* expected format: The API functional level is z/VM V<major>.<minor> */
    if ((tmp = STRSKIP(tmp, "The API functional level is z/VM V")) == NULL)
        goto cleanup;

    if (virParseVersionString(tmp, &version, true) < 0)
        goto cleanup;

    if (retversion)
        *retversion = version;

    ret = 0;

 cleanup:
    virCommandFree(cmd);
    VIR_FREE(help);

    return ret;
}

virCapsPtr zvmCapsInit(void)
{
    virCapsPtr caps;
    virCapsGuestPtr guest;

    if ((caps = virCapabilitiesNew(virArchFromHost(),
                                   false, false)) == NULL)
        goto error;

    if (virCapabilitiesInitNUMA(caps) < 0) {
        virCapabilitiesFreeNUMAInfo(caps);
        VIR_WARN
            ("Failed to query host NUMA topology, disabling NUMA capabilities");
    }

    // TODO need to add features for guest and host if any

    if (virCapabilitiesInitCaches(caps) < 0)
        goto error;

    if (virNodeSuspendGetTargetMask(&caps->host.powerMgmt) < 0)
        VIR_WARN("Failed to get host power management capabilities");

    if (virGetHostUUID(caps->host.host_uuid)) {
        virReportError(VIR_ERR_INTERNAL_ERROR,
                       "%s", _("cannot get the host uuid"));
        goto error;
    }

    if ((guest = virCapabilitiesAddGuest(caps,
                                         VIR_DOMAIN_OSTYPE_LINUX,
                                         caps->host.arch,
                                         NULL,
                                         NULL,
                                         0,
                                         NULL)) == NULL)
        goto error;

    if (virCapabilitiesAddGuestDomain(guest,
                                      VIR_DOMAIN_VIRT_ZVM,
                                      NULL,
                                      NULL,
                                      0,
                                      NULL) == NULL)
        goto error;

    return caps;

    error:
       virObjectUnref(caps);
       return NULL;
}

char *
virZVMFormatConfig(virDomainDefPtr def)
{
    char *zvm_file = NULL;
    unsigned char zero[VIR_UUID_BUFLEN];
    virBuffer buffer = VIR_BUFFER_INITIALIZER;


    memset(zero, 0, VIR_UUID_BUFLEN);

    if (def->virtType != VIR_DOMAIN_VIRT_ZVM) {
        virReportError(VIR_ERR_INTERNAL_ERROR,
                      _("Expecting virt type to be '%s' but found '%s'"),
                      virDomainVirtTypeToString(VIR_DOMAIN_VIRT_ZVM),
                      virDomainVirtTypeToString(def->virtType));
        return NULL;
    }

    /* Add User Details */
    
    virBufferAddLit(&buffer, "USER TEST WD5JU8QP 32M 128M ABG\n");

    /* Get final zvm_file output */
    if (virBufferCheckError(&buffer) < 0)
        goto cleanup;

    zvm_file = virBufferContentAndReset(&buffer);

  cleanup:
    if (zvm_file == NULL)
        virBufferFreeAndReset(&buffer);

    return zvm_file;
}

int zvmExtractVersion(struct zvm_driver *driver)
{
    if (driver->version > 0)
      return 0;

    if (zvmExtractVersionInfo(&driver->version) < 0) {
      virReportError(VIR_ERR_INTERNAL_ERROR, "%s",
                     _("Could not extract zvm version"));
      return -1;
    }

    return 0;
}
