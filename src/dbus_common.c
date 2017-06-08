/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dbus_common.h"
#include "messages.h"

int dbus_common_handle_error(GError **error, const char *what)
{
    if(*error == NULL)
        return 0;

    if(what == NULL)
        what = "<UNKNOWN>";

    if((*error)->message != NULL)
        msg_error(0, LOG_EMERG,
                  "%s: Got D-Bus error: %s", what, (*error)->message);
    else
        msg_error(0, LOG_EMERG,
                  "%s: Got D-Bus error without any message", what);

    g_error_free(*error);
    *error = NULL;

    return -1;
}
