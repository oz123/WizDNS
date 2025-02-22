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

guint8 *construct_dns_response(DnsHeader *header, 
                               DnsQuestion *question,
                               gsize *response_len) 
{
    // Step 1: Create ldns structures for the DNS packet and question
    ldns_pkt *response_pkt = ldns_pkt_new();
    ldns_rr *question_rr = ldns_rr_new();

    // Step 2: Set the question name, type and class in the question RR
    ldns_rdf *question_name_rdf = ldns_dname_new_frm_str(question->name);
    ldns_rr_set_owner(question_rr, question_name_rdf);
    ldns_rr_set_type(question_rr, question->qtype);
    ldns_rr_set_class(question_rr, question->qclass);

    // Step 3: Set header fields and add question section to packet 
    ldns_pkt_set_id(response_pkt, header->id);
    ldns_pkt_set_qr(response_pkt, 1);
    ldns_pkt_set_aa(response_pkt, 1);
    ldns_pkt_set_qdcount(response_pkt, 1);
    ldns_pkt_push_rr(response_pkt, LDNS_SECTION_QUESTION, question_rr);

    // Step 4: Add answer, authority and additional sections as needed
    // This will depend on the specific response required
    // Use ldns_pkt_push_rr to add RRs to the different sections
    ldns_rr *answer_a_rr = ldns_rr_new();
    ldns_rdf *answer_name_rdf = ldns_dname_new_frm_str("somehost.domain.tld");
    ldns_rr_set_owner(answer_a_rr, answer_name_rdf);
    ldns_rr_set_type(answer_a_rr, LDNS_RR_TYPE_A);
    ldns_rr_set_class(answer_a_rr, LDNS_RR_CLASS_IN);
    ldns_rr_set_ttl(answer_a_rr, 3600);

    ldns_rdf *ip_rdf = ldns_rdf_new_frm_str(LDNS_RDF_TYPE_A, "123.29.293.12");
    ldns_rr_push_rdf(answer_a_rr, ip_rdf);

    ldns_pkt_push_rr(response_pkt, LDNS_SECTION_ANSWER, answer_a_rr);
    ldns_pkt_set_ancount(response_pkt, 1);

    // Step 6: Convert packet to wire format
    guint8 *response_wire = NULL; 
    ldns_status status = ldns_pkt2wire(&response_wire, response_pkt, response_len);
    
    if (status != LDNS_STATUS_OK) {
        // Handle error
        *response_len = 0;
        return NULL;  
    }

    // Step 7: Clean up
    ldns_pkt_free(response_pkt);
    ldns_rr_free(question_rr);
    ldns_rr_free(answer_a_rr);

    return response_wire;
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
  guint8 *response = NULL;
  gsize response_len = 0;

  response = construct_dns_response(&header, &question, &response_len);
  // Send response
  g_socket_send(socket, (gchar *)response, response_len, NULL, &error);

  if (error != NULL) {
    g_printerr("Error sending response: %s\n", error->message);
    g_error_free(error);
  }

  g_free(response);
  g_free(question.name);

  return TRUE;
}
