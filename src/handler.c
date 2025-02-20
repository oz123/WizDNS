#include <stdio.h>
#include <string.h>

#include <ldns/ldns.h>

#include "handler.h"

gboolean parse_dns_query(const guint8 *buffer, gsize len, DnsHeader *header,
                         DnsQuestion *question) {
  ldns_status status;
  ldns_pkt *pkt;

  // Parse the entire packet at once
  status = ldns_wire2pkt(&pkt, buffer, len);
  if (status != LDNS_STATUS_OK) {
    g_print("Failed to parse DNS packet: %s\n",
            ldns_get_errorstr_by_id(status));
    return FALSE;
  }

  // Extract header information
  header->id = ldns_pkt_id(pkt);

  // Calculate flags step by step
  guint16 flags = 0;
  flags |= ((guint16)ldns_pkt_qr(pkt)) << 15;
  flags |= ((guint16)ldns_pkt_get_opcode(pkt)) << 11;
  flags |= ((guint16)ldns_pkt_aa(pkt)) << 10;
  flags |= ((guint16)ldns_pkt_tc(pkt)) << 9;
  flags |= ((guint16)ldns_pkt_rd(pkt)) << 8;
  flags |= ((guint16)ldns_pkt_ra(pkt)) << 7;
  flags |= ((guint16)ldns_pkt_ad(pkt)) << 5;
  flags |= ((guint16)ldns_pkt_cd(pkt)) << 4;
  flags |= (guint16)ldns_pkt_get_rcode(pkt);

  header->flags = flags;

  header->qdcount = ldns_pkt_qdcount(pkt);
  header->ancount = ldns_pkt_ancount(pkt);
  header->nscount = ldns_pkt_nscount(pkt);
  header->arcount = ldns_pkt_arcount(pkt);

  // Handle the first question
  ldns_rr_list *questions = ldns_pkt_question(pkt);
  if (ldns_rr_list_rr_count(questions) < 1) {
    ldns_pkt_free(pkt);
    return FALSE;
  }

  ldns_rr *first_q = ldns_rr_list_rr(questions, 0);
  question->name = strdup(ldns_rdf2str(ldns_rr_owner(first_q)));
  question->qtype = ldns_rr_get_type(first_q);
  question->qclass = ldns_rr_get_class(first_q);

  ldns_pkt_free(pkt);
  return TRUE;
}

guint8 *construct_dns_response(DnsHeader *header, const DnsQuestion *question,
                               gsize *response_len) {
  guint8 *response = g_malloc(sizeof(DnsHeader));
  guint8 *ptr = response;

  // Copy ID
  ptr[0] = (header->id >> 8) & 0xFF;
  ptr[1] = header->id & 0xFF;

  // Set response flags (QR=1, RA=1)
  ptr[2] = 0x80; // QR=1 (response), OPCODE=0000, AA=1, TC=0, RD=1
  ptr[3] = 0x80; // RA=1 (recursion available)

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
  ptr[0] = '\0';
  ptr[1] = '\1';
  // Class (IN)
  ptr[2] = '\0';
  ptr[3] = '\1';
  // TTL (3600 seconds)
  ptr[4] = '\0';
  ptr[5] = '\x70';
  ptr[6] = '\0';
  ptr[7] = '\x80';
  // RDLENGTH (4 bytes for IPv4)
  ptr[8] = '\0';
  ptr[9] = '\4';
  // IPv4 address (example: 127.0.0.1)
  ptr[10] = '\127';
  ptr[11] = '\0';

  return response;
}

guint8 *construct_dns_response_with_addresses(GList *addresses, gsize *length) {
  // Implementation for response with addresses
  guint8 *response = g_malloc(512); // Allocate buffer for response
  // ... implement DNS response construction ...
  *length = 512; // Set actual length
  return response;
}

guint8 *construct_dns_response_no_records(guint16 id, gsize *length) {
  // Implementation for no records response
  guint8 *response = g_malloc(512); // Allocate buffer for response
  // ... implement DNS response construction ...
  *length = 512; // Set actual length
  return response;
}

gboolean handle_incoming_message(GIOChannel *channel, GIOCondition condition,
                                 gpointer user_data) {
  if (condition & G_IO_HUP) {
    g_print("Connection closed\n");
    return FALSE;
  }

  guint8 buffer[BUFFER_SIZE];
  gsize bytes_read;
  GError *error = NULL;

  // Get the socket from user_data
  GSocket *socket = G_SOCKET(user_data);

  // Read raw bytes from socket
  g_socket_receive(socket, (gchar *)buffer, BUFFER_SIZE, NULL, &error);

  if (error != NULL) {
    g_printerr("Error reading from socket: %s\n", error->message);
    g_error_free(error);
    return TRUE;
  }

  // Parse DNS query
  DnsHeader header;
  DnsQuestion question;
  if (!parse_dns_query(buffer, bytes_read, &header, &question)) {
    g_print("Invalid DNS query format\n");
    return TRUE;
  }

  g_print("Resolving: %s\n", question.name);

  // Use GResolver to perform the actual resolution
  GResolver *resolver = g_resolver_get_default();
  GList *addresses =
      g_resolver_lookup_by_name(resolver, question.name, NULL, &error);

  guint8 *response = NULL;
  gsize response_len = 0;
  if (addresses != NULL) {
    response = construct_dns_response_with_addresses(addresses, &response_len);
  } else {
    response = construct_dns_response_no_records(header.id, &response_len);
  }

  // Send response
  g_socket_send(socket, (gchar *)response, response_len, NULL, &error);

  if (error != NULL) {
    g_printerr("Error sending response: %s\n", error->message);
    g_error_free(error);
  }

  g_free(response);
  g_list_free_full(addresses, g_object_unref);
  g_free(question.name);

  return TRUE;
}
