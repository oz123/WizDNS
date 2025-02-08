#include "handler.h"
#include <stdio.h>
#include <string.h>

static gboolean parse_dns_query(const guint8 *buffer, gsize len,
                              DnsHeader *header, DnsQuestion *question) {
    // Check minimum buffer size
    if (len < sizeof(DnsHeader)) {
        return FALSE;
    }

    // Parse header
    const guint8 *ptr = buffer;
    
    // Transaction ID
    header->id = ((ptr[0] << 8) | ptr[1]);
    
    // Flags
    header->flags = ((ptr[2] << 8) | ptr[3]);
    
    // Question count
    header->qdcount = ((ptr[4] << 8) | ptr[5]);
    
    // Answer count
    header->ancount = ((ptr[6] << 8) | ptr[7]);
    
    // Name server count
    header->nscount = ((ptr[8] << 8) | ptr[9]);
    
    // Additional record count
    header->arcount = ((ptr[10] << 8) | ptr[11]);

    // Skip to question section
    ptr += sizeof(DnsHeader);

    // Parse domain name
    GString *domain = g_string_new("");
    while (*ptr != '\0' && (ptr - buffer) < len) {
        guint8 len_byte = *(ptr++);
        if (len_byte == 0 || (ptr - buffer) >= len) break;
        
        if (ptr - buffer + len_byte <= len) {
            g_string_append_len(domain, (const gchar*)ptr, len_byte);
            g_string_append_c(domain, '.');
        }
        
        ptr += len_byte;
    }

    // Remove trailing dot if present
    if (domain->len > 0) {
        domain->str[domain->len - 1] = '\0';
    }

    question->name = g_string_free(domain, FALSE);

    // Parse QTYPE and QCLASS
    if ((ptr - buffer + 4) <= len) {
        question->qtype = ((ptr[0] << 8) | ptr[1]);
        question->qclass = ((ptr[2] << 8) | ptr[3]);
    } else {
        return FALSE;
    }

    return TRUE;
}

static guint8 *construct_dns_response(DnsHeader *header, 
                                    const DnsQuestion *question,
                                    gsize *response_len) {
    guint8 *response = g_malloc(sizeof(DnsHeader));
    guint8 *ptr = response;

    // Copy ID
    ptr[0] = (header->id >> 8) & 0xFF;
    ptr[1] = header->id & 0xFF;

    // Set response flags (QR=1, RA=1)
    ptr[2] = 0x80;  // QR=1 (response), OPCODE=0000, AA=1, TC=0, RD=1
    ptr[3] = 0x80;  // RA=1 (recursion available)

    // Copy counts
    ptr += 2;
    memcpy(ptr, &header->qdcount, sizeof(guint16));
    ptr += 2;
    memcpy(ptr, &header->ancount, sizeof(guint16));
    ptr += 2;
    memcpy(ptr, &header->nscount, sizeof(guint16));
    ptr += 2;
    memcpy(ptr, &header->arcount, sizeof(guint16));

    // Add domain name
    gchar **labels = g_strsplit(question->name, ".", -1);
    for (gint i = 0; labels[i]; i++) {
        guint8 len = strlen(labels[i]);
        *ptr++ = len;
        memcpy(ptr, labels[i], len);
        ptr += len;
    }
    g_strfreev(labels);
    *ptr++ = '\0';

    // Add question section
    memcpy(ptr, &question->qtype, sizeof(guint16));
    ptr += 2;
    memcpy(ptr, &question->qclass, sizeof(guint16));

    // Add answer section (simple A record response)
    *response_len = ptr - response;
    response = g_realloc(response, *response_len + 12);
    ptr = response + (*response_len);

    // Type (A record)
    ptr[0] = '\0'; ptr[1] = '\1';
    // Class (IN)
    ptr[2] = '\0'; ptr[3] = '\1';
    // TTL (3600 seconds)
    ptr[4] = '\0'; ptr[5] = '\x70';
    ptr[6] = '\0'; ptr[7] = '\x80';
    // RDLENGTH (4 bytes for IPv4)
    ptr[8] = '\0'; ptr[9] = '\4';
    // IPv4 address (example: 127.0.0.1)
    ptr[10] = '\127'; ptr[11] = '\0';

    return response;
}


gboolean handle_incoming_message(GIOChannel *channel,
                                        GIOCondition condition,
                                        gpointer user_data) {
    if (condition & G_IO_HUP) {
        g_print("Connection closed\n");
        return FALSE;
    }

    guint8 buffer[BUFFER_SIZE];
    gsize bytes_read;
    GError *error = NULL;
    GSocket *socket = G_SOCKET(user_data);  // Get the socket from user_data

    // Read incoming message
    g_io_channel_read_chars(channel, (gchar*)buffer, BUFFER_SIZE,
                           &bytes_read, &error);
    
    if (error != NULL) {
        g_printerr("Error reading from socket: %s\n", error->message);
        g_error_free(error);
        return TRUE;  // Continue watching
    }

    // Parse DNS query
    DnsHeader header;
    DnsQuestion question;
    if (!parse_dns_query(buffer, bytes_read, &header, &question)) {
        g_print("Invalid DNS query format\n");
        return TRUE;
    }

    g_print("Received DNS query for: %s\n", question.name);
    g_print("QTYPE: %u, QCLASS: %u\n", g_ntohs(question.qtype), g_ntohs(question.qclass));

    // Construct response
    gsize response_len;
    guint8 *response = construct_dns_response(&header, &question, &response_len);

    // Send response using the socket directly
    GError *send_error = NULL;
    g_socket_send(socket, (gchar*)response, response_len, NULL, &send_error);
    
    if (send_error != NULL) {
        g_printerr("Error sending response: %s\n", send_error->message);
        g_error_free(send_error);
    }

    g_free(response);
    g_free(question.name);

    return TRUE;
}
