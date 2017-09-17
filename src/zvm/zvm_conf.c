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

/* Free all memory associated with a vmware_driver structure */
void
zvmFreeDriver(struct zvm_driver *driver)
{
    if (!driver)
        return;
        
    virMutexDestroy(&driver->lock);
    VIR_FREE(driver);
}
