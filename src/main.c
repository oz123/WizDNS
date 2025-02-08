/*
 WizDNS - A pluggable DNS

Copyright (C) 2025 Oz Tiram <oz.tiram@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "handler.h"

#define DNS_PORT 53

int main(int argc, char **argv) {
    GSocket *socket;
    GSocketAddress *address;
    GError *error = NULL;
    GIOChannel *channel;
    guint io_watch_id;

    // Create UDP socket
    socket = g_socket_new(G_SOCKET_FAMILY_IPV4,
                         G_SOCKET_TYPE_DATAGRAM,
                         G_SOCKET_PROTOCOL_UDP,
                         &error);
    if (error != NULL) {
        g_printerr("Could not create socket: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    // Create address to bind to
    address = g_inet_socket_address_new(g_inet_address_new_any(G_SOCKET_FAMILY_IPV4),
                                      DNS_PORT);

    // Bind the socket
    if (!g_socket_bind(socket, address, TRUE, &error)) {
        g_printerr("Could not bind socket: %s\n", error->message);
        g_error_free(error);
        g_object_unref(socket);
        g_object_unref(address);
        return 1;
    }

    g_object_unref(address);

    // Get file descriptor and create IO channel
    channel = g_io_channel_unix_new(g_socket_get_fd(socket));

    // Add watch to main loop for incoming messages, passing the socket as user_data
    io_watch_id = g_io_add_watch(channel,
                                G_IO_IN | G_IO_HUP | G_IO_ERR,
                                handle_incoming_message,
                                socket);

    // Create and run main loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    // Cleanup
    g_source_remove(io_watch_id);
    g_io_channel_unref(channel);
    g_object_unref(socket);
    g_main_loop_unref(loop);

    return 0;
}
