/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)tuba_subr.c	7.7 (Berkeley) %G%
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/clnp.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <netiso/tuba_table.h>

static	struct	sockaddr_iso null_siso = { sizeof(null_siso), AF_ISO, };
extern	int	tuba_table_size;
extern	int	tcppcbcachemiss, tcppredack, tcppreddat, tcprexmtthresh;
extern	struct	tcpiphdr tcp_saveti;
struct	inpcb	*tuba_last_inpcb;
struct	inpcb	tuba_inpcb;
struct	isopcb	tuba_isopcb;
/*
 * Tuba initialization
 */
tuba_init()
{
#define TUBAHDRSIZE (3 /*LLC*/ + 9 /*CLNP Fixed*/ + 42 /*Addresses*/ \
		     + 6 /*CLNP Segment*/ + 20 /*TCP*/)

	tuba_inpcb.inp_next = tuba_inpcb.inp_prev = &tuba_inpcb;
	tuba_isopcb.isop_next = tuba_isopcb.isop_prev = &tuba_isopcb;
	tuba_isopcb.isop_faddr = &tuba_isopcb.isop_sfaddr;
	tuba_isopcb.isop_laddr = &tuba_isopcb.isop_sladdr;
	if (max_protohdr < TUBAHDRSIZE)
		max_protohdr = TUBAHDRSIZE;
	if (max_linkhdr + TUBAHDRSIZE > MHLEN)
		panic("tuba_init");
}

static void
tuba_getaddr(error, sum, siso, index)
	int *error;
	register u_long *sum;
	struct sockaddr_iso *siso;
	u_long index;
{
	register struct tuba_cache *tc;
	if (index <= tuba_table_size && (tc = tuba_table[index])) {
		if (siso) {
			*siso = null_siso;
			siso->siso_addr = tc->tc_addr;
		}
		REDUCE(*sum, *sum + tc->tc_sum_out);
	} else
		*error = 1;
}

tuba_output(m, tp)
	register struct mbuf *m;
	struct tcpcb *tp;
{
	struct isopcb *isop;
	register struct tcpiphdr *n;
	u_long sum, i;

	if (tp == 0 || (n = tp->t_template) == 0 || 
	    (isop = (struct isopcb *)tp->t_tuba_pcb) == 0) {
		isop = &tuba_isopcb;
		n = mtod(m, struct tcpiphdr *);
		i = sum = 0;
		tuba_getaddr(&i, &sum, tuba_isopcb.isop_faddr,
				n->ti_dst.s_addr);
		tuba_getaddr(&i, &sum, tuba_isopcb.isop_laddr,
				n->ti_src.s_addr);
		goto adjust;
	}
	if (n->ti_sum == 0) {
		i = sum = 0;
		tuba_getaddr(&i, &sum, (struct sockaddr_iso *)0,
				n->ti_dst.s_addr);
		tuba_getaddr(&i, &sum, (struct sockaddr_iso *)0,
				n->ti_src.s_addr);
		n->ti_sum = sum;
		n = mtod(m, struct tcpiphdr *);
	adjust:
		if (i) {
			m_freem(m);
			return (EADDRNOTAVAIL);
		}
		REDUCE(n->ti_sum, n->ti_sum + (0xffff ^ sum));
	}
	m->m_len -= sizeof (struct ip);
	m->m_pkthdr.len -= sizeof (struct ip);
	m->m_data += sizeof (struct ip);
	return (clnp_output(m, isop, m->m_pkthdr.len, 0));
}


tuba_refcnt(isop, delta)
	struct isopcb *isop;
{
	register struct tuba_cache *tc;
	unsigned index, sum;

	if (delta != 1)
		delta = -1;
	if (isop == 0 || isop->isop_faddr == 0 || isop->isop_laddr == 0 ||
	    (delta == -1 && isop->isop_tuba_cached == 0) ||
	    (delta == 1 && isop->isop_tuba_cached != 0))
		return;
	isop->isop_tuba_cached = (delta == 1);
	if ((index = tuba_lookup(&isop->isop_sfaddr.siso_addr, M_DONTWAIT)) != 0 &&
	    (tc = tuba_table[index]) != 0 && (delta == 1 || tc->tc_refcnt > 0))
		tc->tc_refcnt += delta;
	if ((index = tuba_lookup(&isop->isop_sladdr.siso_addr, M_DONTWAIT)) != 0 &&
	    (tc = tuba_table[index]) != 0 && (delta == 1 || tc->tc_refcnt > 0))
		tc->tc_refcnt += delta;
}

tuba_pcbdetach(isop)
	struct isopcb *isop;
{
	if (isop == 0)
		return;
	tuba_refcnt(isop, -1);
	isop->isop_socket = 0;
	iso_pcbdetach(isop);
}

/*
 * Avoid  in_pcbconnect in faked out tcp_input()
 */
tuba_pcbconnect(inp, nam)
	register struct inpcb *inp;
	struct mbuf *nam;
{
	register struct sockaddr_iso *siso = mtod(nam, struct sockaddr_iso *);
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);
	struct tcpcb *tp = intotcpcb(inp);
	unsigned index = sin->sin_addr.s_addr;
	struct tuba_cache *tc = tuba_table[index];
	struct isopcb *isop = (struct isopcb *)tp->t_tuba_pcb;
	int error;

	inp->inp_faddr.s_addr = index;
	inp->inp_fport = sin->sin_port;
	*siso = null_siso;
	siso->siso_addr = tc->tc_addr;
	siso->siso_tlen = sizeof(inp->inp_fport);
	bcopy((caddr_t)&inp->inp_fport, TSEL(siso), sizeof(inp->inp_fport));
	nam->m_len = sizeof(*siso);
	if ((error = iso_pcbconnect(isop, nam)) == 0)
		tuba_refcnt(isop, 1);
	return (error);
}

/*
 * CALLED FROM:
 * 	clnp's input routine, indirectly through the protosw.
 * FUNCTION and ARGUMENTS:
 * Take a packet (m) from clnp, strip off the clnp header
 * and do tcp input processing.
 * No return value.  
 */
tuba_tcpinput(m, src, dst, clnp_len, ce_bit)
	register struct mbuf *m;
	struct sockaddr_iso *src, *dst;
	int clnp_len, ce_bit;
{
	int s = splnet();
	unsigned long sum, lindex, findex;
	register struct tcpiphdr *ti;
	register struct inpcb *inp;
	struct mbuf *om;
	int len, tlen, off;
	register struct tcpcb *tp = 0;
	int tiflags;
	struct socket *so;
	int todrop, acked, ourfinisacked, needoutput = 0;
	short ostate;
	struct in_addr laddr;
	int dropsocket = 0, iss = 0;

	if ((m->m_flags & M_PKTHDR) == 0) {
		om = m_gethdr(M_DONTWAIT, MT_DATA);
		if (om == 0)
			goto drop;
		om->m_next = m;
		for (len = 0; m; m = m->m_next)
			len += m->m_len;
		m = om;
		m->m_pkthdr.len = len;
	}
	om = 0;
	/*
	 * Do some housekeeping looking up CLNP addresses.
	 * If we are out of space might as well drop the packet now.
	 */
	tcpstat.tcps_rcvtotal++;
	lindex = tuba_lookup(&dst->siso_addr, M_DONTWAIT);
	findex = tuba_lookup(&dst->siso_addr, M_DONTWAIT);
	if (lindex == 0 || findex == 0)
		goto drop;
	/*
	 * Get CLNP and TCP header together in first mbuf.
	 * CLNP gave us an mbuf chain WITH the clnp header pulled up,
	 * and the length of the clnp header.
	 */
	len = clnp_len + sizeof(struct tcphdr);
	if (m->m_len < len) {
		if ((m = m_pullup(m, len)) == 0) {
			tcpstat.tcps_rcvshort++;
			return;
		}
	}
	/*
	 * Calculate checksum of extended TCP header and data,
	 * by adjusting the checksum for missing parts of the header.
	 */
	m->m_data += clnp_len;
	m->m_len -= clnp_len;
	tlen = m->m_pkthdr.len -= clnp_len;
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	m->m_pkthdr.len += sizeof(struct ip);
	/*
	 * The reassembly code assumes it will be overwriting a useless
	 * part of the packet, which is why we need to have it point
	 * into the packet itself.
	 *
	 * Check to see if the data is properly alligned
	 * so that we can save copying the tcp header.
	 * This code knows way too much about the structure of mbufs!
	 */
	off = ((sizeof (long) - 1) & ((m->m_flags & M_EXT) ?
		(m->m_data - m->m_ext.ext_buf) :  (m->m_data - m->m_pktdat)));
	if (off) {
		struct mbuf *m0 = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m0 == 0) {
			goto drop;
		}
		m0->m_data += max_linkhdr;
		bcopy(mtod(m, caddr_t) + sizeof(struct ip),
		      mtod(m0, caddr_t) + sizeof(struct ip),
		      sizeof(struct tcphdr));
		m->m_data += sizeof(struct tcpiphdr);
		m0->m_next = m;
		m0->m_pkthdr = m->m_pkthdr;
		m0->m_flags = m->m_flags & M_COPYFLAGS;
		m0->m_len = sizeof(struct tcpiphdr);
		m = m0;
	}
	ti = mtod(m, struct tcpiphdr *);
	ti->ti_src.s_addr = tuba_table[findex]->tc_sum_in;
	ti->ti_dst.s_addr = tuba_table[lindex]->tc_sum_in;
	ti->ti_prev = ti->ti_next = 0;
	ti->ti_x1 = 0; ti->ti_pr = ISOPROTO_TCP;
	ti->ti_len = htons((u_short)tlen);
	if (ti->ti_sum = in_cksum(m, m->m_pkthdr.len)) {
		tcpstat.tcps_rcvbadsum++;
		goto drop;
	}
	ti->ti_src.s_addr = findex;
	ti->ti_dst.s_addr = lindex;
	/*
	 * Now include the rest of TCP input
	 */
#define TUBA_INCLUDE
#define	in_pcbconnect	tuba_pcbconnect
#define	tcb		tuba_inpcb
#define tcp_last_inpcb	tuba_last_inpcb

#include <netinet/tcp_input.c>
}
