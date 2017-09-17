/*---------------------------------------------------------------------------*/
/*
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
 */
/*---------------------------------------------------------------------------*/

#ifndef ZVM_CONF_H
# define ZVM_CONF_H

# include "internal.h"
# include "libvirt_internal.h"
# include "virerror.h"
# include "virthread.h"
# include "virsysinfo.h"
# include "vircommand.h"
# include "virhash.h"

# define VIR_FROM_THIS VIR_FROM_ZVM

struct zvm_driver {
    virMutex lock;
    virSysinfoDefPtr hostsysinfo;
};

void zvmFreeDriver(struct zvm_driver *driver);

#endif
