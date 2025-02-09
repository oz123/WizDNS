#ifndef HANDLER_H
#define HANDLER_H

#include <gio/gio.h>
#include <glib.h>

#define BUFFER_SIZE 1024

// Structure declarations
typedef struct {
    guint16 id;
    guint16 flags;
    guint16 qdcount;
    guint16 ancount;
    guint16 nscount;
    guint16 arcount;
} DnsHeader;

typedef struct {
    gchar *name;
    guint16 qtype;
    guint16 qclass;
} DnsQuestion;

// Function declarations
gboolean handle_incoming_message(GIOChannel *channel,
                               GIOCondition condition,
                               gpointer user_data);

guint8 *construct_dns_response(DnsHeader *header, 
                               const DnsQuestion *question,
                               gsize *response_len);

gboolean parse_dns_query(const guint8 *buffer, 
                         gsize len,
                         DnsHeader *header,
                         DnsQuestion *question);

#endif
