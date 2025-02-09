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
#include <stdint.h>
#include "handler.h"

#define DNS_PORT 53

static void print_usage(void) {
    g_print("Usage: wizdns [-h <ip-address>] [-p <port>]\n");
    g_print("\t-h <ip-address>: Bind to specific IPv4 address\n");
    g_print("\t-p <port>: Listen on specified port (default: 53)\n");
}

int main(int argc, char **argv) {
    GSocket *socket;
    GSocketAddress *address;
    GError *error = NULL;
    GIOChannel *channel;
    guint io_watch_id;
    GMainLoop *loop;

    // Parse command-line arguments
    gint opt;
    gchar *bind_addr = NULL;
    guint16 port = DNS_PORT;

    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
            case 'h':
                bind_addr = optarg;
                break;
            case 'p': {
                guint64 parsed_port = g_ascii_strtoull(optarg, NULL, 10);
                if (parsed_port > UINT16_MAX) {
                    g_printerr("Invalid port number: %s\n", optarg);
                    return 1;
                }
                port = (guint16)parsed_port;
                break;
            }
            default:
                print_usage();
                return 1;
        }
    }

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
    GInetAddress *inet_addr;
    if (bind_addr) {
        inet_addr = g_inet_address_new_from_string(bind_addr);
        if (!inet_addr) {
            g_printerr("Invalid IP address: %s\n", bind_addr);
            g_object_unref(socket);
            return 1;
        }
    } else {
        inet_addr = g_inet_address_new_any(G_SOCKET_FAMILY_IPV4);
    }

    address = g_inet_socket_address_new(inet_addr, port);

    // Bind the socket
    if (!g_socket_bind(socket, address, TRUE, &error)) {
        g_printerr("Could not bind socket: %s\n", error->message);
        g_error_free(error);
        g_object_unref(socket);
        g_object_unref(address);
        g_object_unref(inet_addr);
        return 1;
    }

    g_object_unref(address);
    g_object_unref(inet_addr);

    g_print("Starting WizDNS, listening on ");
    if (bind_addr) {
        g_print("%s:%u\n", bind_addr, port);
    } else {
        g_print("all interfaces port %u\n", port);
    }

    // Get file descriptor and create IO channel
    channel = g_io_channel_unix_new(g_socket_get_fd(socket));

    // Add watch to main loop for incoming messages, passing the socket as user_data
    io_watch_id = g_io_add_watch(channel,
                                G_IO_IN | G_IO_HUP | G_IO_ERR,
                                handle_incoming_message,
                                socket);

    // Create and run main loop
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    // Cleanup
    g_source_remove(io_watch_id);
    g_io_channel_unref(channel);
    g_object_unref(socket);
    g_main_loop_unref(loop);

    return 0;
}
