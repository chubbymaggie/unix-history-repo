/*
 * testcode/pktview.c - debug program to disassemble a DNS packet.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This program shows a dns packet wire format.
 */

#include "config.h"
#include <ldns/ldns.h>
#include "util/log.h"
#include "util/data/dname.h"
#include "util/data/msgparse.h"
#include "testcode/unitmain.h"
#include "testcode/readhex.h"

/** usage information for pktview */
static void usage(char* argv[])
{
	printf("usage: %s\n", argv[0]);
	printf("present hex packet on stdin.\n");
	exit(1);
}

/** read hex input */
static void read_input(ldns_buffer* pkt, FILE* in)
{
	char buf[102400];
	char* np = buf;
	while(fgets(np, (int)sizeof(buf) - (np-buf), in)) {
		if(buf[0] == ';') /* comment */
			continue;
		np = &np[strlen(np)];
	}
	hex_to_buf(pkt, buf);
}

/** analyze domain name in packet, possibly compressed */
static void analyze_dname(ldns_buffer* pkt)
{
	size_t oldpos = ldns_buffer_position(pkt);
	size_t len;
	printf("[pos %d] dname: ", (int)oldpos);
	dname_print(stdout, pkt, ldns_buffer_current(pkt));
	len = pkt_dname_len(pkt);
	printf(" len=%d", (int)len);
	if(ldns_buffer_position(pkt)-oldpos != len)
		printf(" comprlen=%d\n", 
			(int)(ldns_buffer_position(pkt)-oldpos));
	else	printf("\n");
}

/** analyze rdata in packet */
static void analyze_rdata(ldns_buffer*pkt, const ldns_rr_descriptor* desc, 
	uint16_t rdlen)
{
	int rdf = 0;
	int count = (int)desc->_dname_count;
	size_t len, oldpos;
	while(rdlen > 0 && count) {
		switch(desc->_wireformat[rdf]) {
		case LDNS_RDF_TYPE_DNAME:
			oldpos = ldns_buffer_position(pkt);
			analyze_dname(pkt);
			rdlen -= ldns_buffer_position(pkt)-oldpos;
			count --;
			len = 0;
			break;
		case LDNS_RDF_TYPE_STR:
			len = ldns_buffer_current(pkt)[0] + 1;
			break;
		default:
			len = get_rdf_size(desc->_wireformat[rdf]);
		}
		if(len) {
			printf(" wf[%d]", (int)len);
			ldns_buffer_skip(pkt, (ssize_t)len);
			rdlen -= len;
		}
		rdf++;
	}
	if(rdlen) {
		size_t i;
		printf(" remain[%d]\n", (int)rdlen);
		for(i=0; i<rdlen; i++)
			printf(" %2.2X", (unsigned)ldns_buffer_current(pkt)[i]);
		printf("\n");
	}
	else	printf("\n");
	ldns_buffer_skip(pkt, (ssize_t)rdlen);
}

/** analyze rr in packet */
static void analyze_rr(ldns_buffer* pkt, int q)
{
	uint16_t type, dclass, len;
	uint32_t ttl;
	analyze_dname(pkt);
	type = ldns_buffer_read_u16(pkt);
	dclass = ldns_buffer_read_u16(pkt);
	printf("type %s(%d)", ldns_rr_descript(type)?  
		ldns_rr_descript(type)->_name: "??" , (int)type);
	printf(" class %s(%d) ", ldns_lookup_by_id(ldns_rr_classes, 
		(int)dclass)?ldns_lookup_by_id( ldns_rr_classes, 
		(int)dclass)->name:"??", (int)dclass);
	if(q) {
		printf("\n");
	} else {
		ttl = ldns_buffer_read_u32(pkt);
		printf(" ttl %d (0x%x)", (int)ttl, (unsigned)ttl);
		len = ldns_buffer_read_u16(pkt);
		printf(" rdata len %d:\n", (int)len);
		if(ldns_rr_descript(type))
			analyze_rdata(pkt, ldns_rr_descript(type), len);
		else ldns_buffer_skip(pkt, (ssize_t)len);
	}
}

/** analyse pkt */
static void analyze(ldns_buffer* pkt)
{
	uint16_t i, f, qd, an, ns, ar;
	int rrnum = 0;
	printf("packet length %d\n", (int)ldns_buffer_limit(pkt));
	if(ldns_buffer_limit(pkt) < 12) return;

	i = ldns_buffer_read_u16(pkt);
	printf("id (hostorder): %d (0x%x)\n", (int)i, (unsigned)i);
	f = ldns_buffer_read_u16(pkt);
	printf("flags: 0x%x\n", (unsigned)f);
	qd = ldns_buffer_read_u16(pkt);
	printf("qdcount: %d\n", (int)qd);
	an = ldns_buffer_read_u16(pkt);
	printf("ancount: %d\n", (int)an);
	ns = ldns_buffer_read_u16(pkt);
	printf("nscount: %d\n", (int)ns);
	ar = ldns_buffer_read_u16(pkt);
	printf("arcount: %d\n", (int)ar);
	
	printf(";-- query section\n");
	while(ldns_buffer_remaining(pkt) > 0) {
		if(rrnum == (int)qd) 
			printf(";-- answer section\n");
		if(rrnum == (int)qd+(int)an) 
			printf(";-- authority section\n");
		if(rrnum == (int)qd+(int)an+(int)ns) 
			printf(";-- additional section\n");
		printf("rr %d ", rrnum);
		analyze_rr(pkt, rrnum < (int)qd);
		rrnum++;
	}
}

/** main program for pktview */
int main(int argc, char* argv[]) 
{
	ldns_buffer* pkt = ldns_buffer_new(65553);
	if(argc != 1) {
		usage(argv);
	}
	if(!pkt) fatal_exit("out of memory");

	read_input(pkt, stdin);
	analyze(pkt);

	ldns_buffer_free(pkt);
	return 0;
}
