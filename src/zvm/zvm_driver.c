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


#include "virsysinfo.h"

#include "zvm_driver.h"

#define VIR_FROM_THIS	VIR_FROM_ZVM

VIR_LOG_INIT("zvm.zvm_driver");


static char *zvmConnectGetHostname(virConnectPtr conn)
{
    if (virConnectGetHostnameEnsureACL(conn) < 0)
        return NULL;

    return virGetHostname();
}


/*----- Register with libvirt.c, and initialize zVM drivers. -----*/

/* The interface which we export upwards to libvirt.c. */

static virHypervisorDriver zvmHypervisorDriver = {
    .name = "ZVM",
    .connectGetHostname = zvmConnectGetHostname,
};

static virConnectDriver zvmConnectDriver = {
    .hypervisorDriver = &zvmHypervisorDriver,
};

static virStateDriver zvmStateDriver = {
    .name = "zvm",
};

int
zvmRegister(void)
{
    if (virRegisterConnectDriver(&zvmConnectDriver,
                                 true) < 0)
        return -1;
    if (virRegisterStateDriver(&zvmStateDriver) < 0)
        return -1;
    return 0;
}