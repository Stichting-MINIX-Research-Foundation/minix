/* dbus_mgr.h
 *
 *  named module to provide dynamic forwarding zones in 
 *  response to D-BUS dhcp events
 *
 *  Copyright(C) Jason Vas Dias, Red Hat Inc., 2005
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation at 
 *           http://www.fsf.org/licensing/licenses/gpl.txt
 *  and included in this software distribution as the "LICENSE" file.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

extern isc_result_t 
dbus_mgr_create
(   isc_mem_t *mctx, 
    isc_taskmgr_t *taskmgr,
    isc_socketmgr_t *socketmgr,
    isc_timermgr_t *timermgr,
    ns_dbus_mgr_t **dbus_mgr
);

extern void 
dbus_mgr_shutdown
(   ns_dbus_mgr_t *dus_mgr_t
);





