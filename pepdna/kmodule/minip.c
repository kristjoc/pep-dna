#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include "minip.h"
#include "core.h"
#include "connection.h"
#include "hash.h"
#include "tcp_utils.h"
#include <linux/ip.h>
#include <net/route.h>
#include <net/ip.h>

#ifdef CONFIG_PEPDNA_LOCAL_SENDER
#include <net/ip.h>
#endif

extern char *ifname; /* Declared in 'core.c' */
/* #define MINIP_HDR_LEN (sizeof(struct minip_hdr)) */
/* #define MINIP_MTU (READ_ONCE(dev_get_by_name(&init_net, ifname)->mtu)) */
/* #define MINIP_MSS (MINIP_MTU - MINIP_HDR_LEN) */
#define MINIP_MSS 1481

static int pepdna_con_i2minip_fwd(struct pepdna_con *);
static int pepdna_con_minip2i_fwd(struct pepdna_con *, struct sk_buff *);
static int pepdna_minip_conn_snd_data(struct pepdna_con *, unsigned char *, size_t);
static int rtxqueue_push(struct rtxqueue *, struct sk_buff *);
static int pepdna_minip_conn_delete(u32, u8 *);

static u32 skb_seq_get(struct sk_buff *skb)
{
	struct minip_hdr *hdr = (struct minip_hdr *)skb_network_header(skb);

	return ntohl(hdr->seq);
}

static u32 rtxq_size(struct pepdna_con *con)
{
	return (u32)con->rtxq->queue->len;
}

static int rtxq_entry_destroy(struct rtxq_entry *entry)
{
	if (!entry) {
		pep_dbg("Entry to be destroyed not found in rtx queue");
		return -1;
	}

	if (skb_unref(entry->skb)) {
		pep_dbg("Decrementing and freeing skb now");
		kfree_skb(entry->skb);
	}
	list_del(&entry->next);
	kfree(entry);

	return 0;
}

static int rtxqueue_entries_ack(struct rtxqueue *q, u32 ack)
{
	struct rtxq_entry *cur, *n;
	int credit = 0;
	u32 seq;

	list_for_each_entry_safe(cur, n, &q->head, next) {
		seq = skb_seq_get(cur->skb);
		if (seq >= ack)
			break;
		pep_dbg("Seq num acked: %u. rtxq size %d", seq, q->len);
		if (rtxq_entry_destroy(cur)) {
			pep_dbg("Failed to delete seq %u from rtxq", seq);
			credit = -1;
			break;
		}
		pep_dbg("Deleted seq %u from rtxq", seq);
		q->len--;
		credit++;
	}

	return credit;
}

static int rtxq_entry_rtx(struct rtxq_entry *entry, u32 seq)
{
	if (entry->retries < MAX_MINIP_RETRY) {
		struct minip_hdr *hdr;

		hdr = (struct minip_hdr *)skb_network_header(entry->skb);
		hdr->ts = htonl(jiffies_to_msecs(jiffies));
		entry->retries++;
		skb_get(entry->skb); // bump the ref count
		return dev_queue_xmit(entry->skb);
	} else {
		pep_err("Max MINIP retransmissions reached for seq %u", seq);

		return -1;
	}
}

static int rtxqueue_entries_rtx(struct rtxqueue *q, u32 ack)
{
	struct rtxq_entry *cur, *n;
	u32 seq;

	list_for_each_entry_safe(cur, n, &q->head, next) {
		seq = skb_seq_get(cur->skb);
		if (seq >= ack) {
			if (rtxq_entry_rtx(cur, seq) < 0) {
				pep_dbg("Failed to rtx seq %u", seq);
				return -1;
			}
			pep_dbg("Retransmitted seq %u", seq);
		}
	}

	return 0;
}

int rtxq_full_rtx(struct rtxq * q, u32 ack)
{
	if (!q)
		return -1;

	if (rtxqueue_entries_rtx(q->queue, ack) < 0)
		return -1;

	return 0;
}

static int rtxqueue_fast_rtx(struct rtxqueue *q, u32 ack)
{
	struct rtxq_entry *cur, *n;
	u32 seq;

	list_for_each_entry_safe(cur, n, &q->head, next) {
		seq = skb_seq_get(cur->skb);
		if (seq == ack) {
			if (rtxq_entry_rtx(cur, seq) < 0) {
				pep_dbg("Failed to rtx seq %u", seq);
				return -1;
			}
			pep_dbg("Fast retransmitted seq %u", seq);
			break;
		}
	}

	return 0;
}

int rtxq_single_rtx(struct rtxq * q, u32 ack)
{
	if (!q)
		return -1;

	if (rtxqueue_fast_rtx(q->queue, ack) < 0)
		return -1;

	return 0;
}

int rtxq_ack(struct rtxq *q, u32 ack)
{
	int rc = 0;

	if (!q)
		return -1;

	spin_lock_bh(&q->lock);
	rc = rtxqueue_entries_ack(q->queue, ack);
	/* rtimer_restart(&q->parent->timers.rtx, tr); */
	spin_unlock_bh(&q->lock);

	return rc;
}

int rtxq_push(struct rtxq * q, struct sk_buff *skb)
{
	int res;

	spin_lock_bh(&q->lock);
	res = rtxqueue_push(q->queue, skb);
	spin_unlock_bh(&q->lock);

	return res;
}

static struct rtxqueue *rtxqueue_create(void)
{
	struct rtxqueue *tmp;

	tmp = kzalloc(sizeof(*tmp), GFP_ATOMIC);
	if (!tmp)
		return NULL;

	INIT_LIST_HEAD(&tmp->head);
	tmp->len = 0;

	return tmp;
}


static void rtxqueue_flush(struct rtxqueue * q)
{
	struct rtxq_entry *cur, *n;

	list_for_each_entry_safe(cur, n, &q->head, next) {
		rtxq_entry_destroy(cur);
		q->len --;
	}
}

static int rtxqueue_destroy(struct rtxqueue * q)
{
	if (!q)
		return -1;

	rtxqueue_flush(q);
	kfree(q);

	return 0;
}

struct rtxq * rtxq_create(void)
{
	struct rtxq * tmp;

	tmp = kzalloc(sizeof(*tmp), GFP_ATOMIC);
	if (!tmp)
		return NULL;

	/* rtimer_init(rtx_timer_func, &dtp->timers.rtx, dtp); */

	tmp->queue = rtxqueue_create();
	if (!tmp->queue) {
		pep_err("Failed to create retransmission queue");
		rtxq_destroy(tmp);
		return NULL;
	}
	spin_lock_init(&tmp->lock);

	return tmp;
}

int rtxq_destroy(struct rtxq * q)
{
	unsigned long flags;

	if (!q)
		return -1;

	spin_lock_irqsave(&q->lock, flags);
	if (q->queue && rtxqueue_destroy(q->queue))
		pep_err("Failed to destroy queue for RTXQ %pK", q->queue);

	spin_unlock_irqrestore(&q->lock, flags);

	kfree(q);

	return 0;
}

static struct rtxq_entry * rtxq_entry_create(struct sk_buff *skb)
{
	struct rtxq_entry *tmp;

	tmp = kzalloc(sizeof(*tmp), GFP_ATOMIC);
	if (!tmp)
		return NULL;

	tmp->skb = skb;
	tmp->retries = 0;

	INIT_LIST_HEAD(&tmp->next);

	return tmp;
}

/* push in seq_num order */
static int rtxqueue_push(struct rtxqueue *q, struct sk_buff *skb)
{
	struct rtxq_entry *tmp = rtxq_entry_create(skb);
	if (!tmp)
		return -1;

	list_add_tail(&tmp->next, &q->head);
	q->len++;

	pep_dbg("Pushed PDU with seq: %u to rtxq queue", skb_seq_get(skb));

	return 0;
}

static void minip_update_cwnd(struct pepdna_con *con)
{
	u32 flight = con->next_seq - (u32)(atomic_read(&con->last_acked));

	/* Adjust window size based on difference */
	if (WINDOW_SIZE >= flight) {
		/* More packets needed, increase window size */
		con->window++;
	} else if (WINDOW_SIZE < flight) {
		/* Too many packets, decrease window size */
		if (con->window > 0)
			con->window--;
	}

	/* Ensure window size stays within bounds */
	if (con->window > WINDOW_SIZE)
		con->window = WINDOW_SIZE;

	pep_dbg("%u pkts in flight", flight);
}

/**
 * minip_sender_timeout() - timer that fires in case of packet loss
 * @t: address to timer_list inside con
 *
 * If fired it means that there was packet loss.
 * Retransmit unacked packets
 * Switch to Slow Start, set the ss_threshold to half of the current cwnd and
 * reset the cwnd to 3*MSS
 */
void minip_sender_timeout(struct timer_list *t)
{
	struct pepdna_con *con = from_timer(con, t, timer);

	/* resend the non-ACKed packets... if any */
	if (rtxq_size(con)) {
		atomic_set(&con->sending, 0);
		/* Retransmit all pkts in the rtx queue */
		con->state = RECOVERY;
		if (rtxq_full_rtx(con->rtxq, (u32)atomic_read(&con->last_acked) + 1) < 0) {
			/* Send a MINIP_CONN_DELETE to deallocate the flow */
			if (pepdna_minip_conn_delete(con->id,
						     con->server->to_mac) < 0) {
				pep_err("failed to send MINIP_CONN_DELETE");
			}
			pep_dbg("Sent MINIP_CONN_DELETE [cid %u]", con->id);
			pepdna_con_close(con);

			return;
		}

		pep_dbg("Rtxd pkts [%u, %u] due to timeout (rto=%u ms)",
			(u32)atomic_read(&con->last_acked) + 1,
			con->next_seq - 1, con->rto);
	}

	mod_timer(&con->timer, jiffies + msecs_to_jiffies(con->rto));
}

/* minip_update_rto() - calculate new retransmission timeout
 * @con: connection instance
 * @new_rtt: new roundtrip time in msec
 */
static void minip_update_rto(struct pepdna_con *con, u32 new_rtt)
{
	long m = new_rtt;

	/* RTT update
	 * Details in Section 2.2 and 2.3 of RFC6298
	 *
	 * It's tricky to understand. Don't lose hair please.
	 * Inspired by tcp_rtt_estimator() tcp_input.c
	 */
	if (con->srtt != 0) {
		m -= (con->srtt >> 3); /* m is now error in rtt est */
		con->srtt += m; /* rtt = 7/8 srtt + 1/8 new */
		if (m < 0)
			m = -m;

		m -= (con->rttvar >> 2);
		con->rttvar += m; /* mdev ~= 3/4 rttvar + 1/4 new */
	} else {
		/* first measure getting in */
		con->srtt = m << 3;	/* take the measured time to be srtt */
		con->rttvar = m << 1; /* new_rtt / 2 */
	}

	/* rto = srtt + 4 * rttvar.
	 * rttvar is scaled by 4, therefore doesn't need to be multiplied
	 */
	con->rto = (con->srtt >> 3) + con->rttvar;
}

/*
 * Send a MINIP_CONN_DELETE packet, a.k.a FIN, to deallocate the MINIP flow
 * -------------------------------------------------------------------------- */
static int pepdna_minip_conn_delete(u32 id, u8 *to_mac)
{
	/* FIXME */
	struct net_device *dev = dev_get_by_name(&init_net, ifname);
	struct minip_hdr *hdr;
	static u16 proto = ETH_P_MINIP;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	int hdr_len = sizeof(struct minip_hdr);

	/* skb */
	struct sk_buff* skb = alloc_skb(hdr_len + hlen + tlen, GFP_ATOMIC);
	if (!skb) {
		return -1;
	}

	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	hdr = skb_put(skb, hdr_len);
	skb->dev = dev;

	/*
	 * Fill the device header for the MINIP frame
	 */
	if (dev_hard_header(skb, dev, proto, to_mac, dev->dev_addr, skb->len) < 0)
		goto out;

	/*
	 * Fill out the MINIP protocol part
	 */
	hdr->pkt_type = MINIP_CONN_DELETE;
	hdr->sdu_len = 0u;
	hdr->id = htonl(id);

	skb->protocol = htons(proto);
	skb->no_fcs = 1;
	skb->pkt_type = PACKET_OUTGOING;

	return dev_queue_xmit(skb);
out:
	kfree_skb(skb);
	return -1;
}

/*
 * Send a MINIP_CONN_FINISHED packet, a.k.a FIN/ACK
 * -------------------------------------------------------------------------- */
int pepdna_minip_conn_finished(u32 id, u8 *to_mac)
{
	/* FIXME */
	struct net_device *dev = dev_get_by_name(&init_net, ifname);
	struct minip_hdr *hdr;
	static u16 proto = ETH_P_MINIP;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	int hdr_len = sizeof(struct minip_hdr);

	/* skb */
	struct sk_buff* skb = alloc_skb(hdr_len + hlen + tlen, GFP_ATOMIC);
	if (!skb) {
		return -1;
	}

	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	hdr = skb_put(skb, hdr_len);
	skb->dev = dev;

	/*
	 * Fill the device header for the MINIP frame
	 */
	if (dev_hard_header(skb, dev, proto, to_mac, dev->dev_addr, skb->len) < 0)
		goto out;

	/*
	 * Fill out the MINIP protocol part
	 */
	hdr->pkt_type = MINIP_CONN_FINISHED;
	hdr->sdu_len = 0u;
	hdr->id = htonl(id);

	skb->protocol = htons(proto);
	skb->no_fcs = 1;
	skb->pkt_type = PACKET_OUTGOING;

	return dev_queue_xmit(skb);
out:
	kfree_skb(skb);
	return -1;
}

int pepdna_minip_send_response(u32 id, u8 *to_mac)
{
	/* FIXME */
	struct net_device *dev = dev_get_by_name(&init_net, ifname);
	static uint16_t proto = ETH_P_MINIP;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	int hdr_len = sizeof(struct minip_hdr);
	struct minip_hdr *hdr;

	/* skb */
	struct sk_buff* skb = alloc_skb(hdr_len + hlen + tlen, GFP_ATOMIC);
	if (!skb)
		return -1;
	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	hdr = skb_put(skb, hdr_len);
	skb->dev = dev;

	/*
	 * Fill the device header for the MINIP frame
	 */
	if (dev_hard_header(skb, dev, proto, to_mac, dev->dev_addr, skb->len) < 0)
		goto out;

	/*
	 * Fill out the MINIP protocol part
	 */
	hdr->pkt_type = MINIP_CONN_RESPONSE;
	hdr->sdu_len = 0u;
	hdr->id = htonl(id);
	hdr->seq = htonl(MINIP_FIRST_SEQ);
	hdr->ack = htonl(MINIP_FIRST_SEQ + 1);

	skb->protocol = htons(proto);
	skb->no_fcs = 1;
	skb->pkt_type = PACKET_OUTGOING;

	pep_dbg("Sent MINIP_CONN_RESPONSE [cid %u]", id);

	return dev_queue_xmit(skb);
out:
	kfree_skb(skb);

	return -1;
}

static int pepdna_minip_send_ack(u32 id, u32 ack, u8 *to_mac, __be32 ts)
{
	/* FIXME */
	struct net_device *dev = dev_get_by_name(&init_net, ifname);
	static uint16_t proto = ETH_P_MINIP;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	int hdr_len = sizeof(struct minip_hdr);
	struct minip_hdr *hdr;

	/* skb */
	struct sk_buff* skb = alloc_skb(hdr_len + hlen + tlen, GFP_ATOMIC);
	if (!skb)
		return -1;
	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	hdr = skb_put(skb, hdr_len);
	skb->dev = dev;

	/*
	 * Fill the device header for the MINIP frame
	 */
	if (dev_hard_header(skb, dev, proto, to_mac, dev->dev_addr, skb->len) < 0)
		goto out;

	/*
	 * Fill out the MINIP protocol part
	 */
	hdr->pkt_type = MINIP_CONN_ACK;
	hdr->sdu_len = 0u;
	hdr->id = htonl(id);
	hdr->ack = htonl(ack);
	hdr->ts = ts;

	skb->protocol = htons(proto);
	skb->no_fcs = 1;
	skb->pkt_type = PACKET_OUTGOING;

	return dev_queue_xmit(skb);
out:
	kfree_skb(skb);
	return -1;
}

static int pepdna_minip_skb_send(struct pepdna_con *con, unsigned char *buf,
				 size_t len)
{
	/* FIXME */
	struct net_device *dev = dev_get_by_name(&init_net, ifname);
	struct sk_buff *cskb = NULL;
	struct minip_hdr *hdr;
	static uint16_t proto = ETH_P_MINIP;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	int hdr_len = sizeof(struct minip_hdr);

	/* skb */
	struct sk_buff* skb = alloc_skb(len + hdr_len + hlen + tlen, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	hdr = skb_put(skb, hdr_len);
	skb_put_data(skb, buf, len);
	skb->dev = dev;

	/*
	 * Fill the device header for the MINIP frame
	 */
	if (dev_hard_header(skb, dev, proto, con->server->to_mac, dev->dev_addr,
			    skb->len) < 0)
		goto out;

	/*
	 * Fill out the MINIP protocol part
	 */
	hdr->pkt_type = MINIP_CONN_DATA;
	hdr->sdu_len = (u16)len;
	hdr->id = htonl(con->id);
	hdr->seq = htonl(con->next_seq);
	hdr->ts = htonl(jiffies_to_msecs(jiffies));

	skb->protocol = htons(proto);
	skb->no_fcs = 1;
	skb->pkt_type = PACKET_OUTGOING;

	/* Clone skb for the rtx queue */
	cskb = skb_clone(skb, GFP_ATOMIC);
	if (!cskb)
		goto out;

	/* Copy skb to retransmission queue */
	if (rtxq_push(con->rtxq, cskb)) {
		pep_dbg("Failed to copy skb (seq %u) to rtxq", con->next_seq);
		kfree_skb(cskb);
		goto out;
	}
	pep_dbg("Transmitted skb (seq %u)", con->next_seq);

	return dev_queue_xmit(skb);
out:
	kfree_skb(skb);
	return -1;
}

/*
 * Send buffer over a MINIP flow
 * ------------------------------------------------------------------------- */
static int pepdna_minip_conn_snd_data(struct pepdna_con *con, unsigned char *buf,
				      size_t len)
{
	size_t left = len, mtu = MINIP_MSS, copylen = 0, sent = 0;
	int rc	= 0;

	pep_dbg("Trying to forward a total of %lu bytes to MINIP", len);

	while (left) {
		copylen = min(left, mtu);

		rc = pepdna_minip_skb_send(con, buf + sent, copylen);
		pep_dbg("minip_skb_send() returned %d", rc);

		if (rc < 0) {
			pep_err("error forwarding skb to MINIP");
			rc = -EIO;
			goto out;
		}

		left -= copylen;
		sent += copylen;

		pep_dbg("Forwarded %lu out of %lu bytes to MINIP", sent, len);

		/* Update window */
		con->next_seq++;
		if (con->window > 0) {
			con->window--;
			pep_dbg("Sent SKB Cwnd update %u", con->window);
		}
	}
out:
	return sent ? sent : rc;
}

static int pepdna_minip_conn_request(__be32 saddr, __be16 source,
				     __be32 daddr, __be16 dest,
				     __u32 id, __u8 *to_mac)
{
	/* FIXME */
	struct net_device *dev = dev_get_by_name(&init_net, ifname);
	static uint16_t proto = ETH_P_MINIP;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	int hdr_len = sizeof(struct minip_hdr);
	int syn_len = sizeof(struct syn_tuple);
	struct minip_hdr *hdr;
	struct syn_tuple *syn;

	/* skb */
	struct sk_buff* skb = alloc_skb(syn_len + hdr_len + hlen + tlen, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	hdr = skb_put(skb, hdr_len);
	syn = skb_put(skb, syn_len);
	skb->dev = dev;

	/*
	 * Fill the device header for the MINIP frame
	 * FIXME: originally, dev->broadcast
	 */
	if (dev_hard_header(skb, dev, proto, to_mac, dev->dev_addr, skb->len) < 0)
		goto out;

	/*
	 * Fill out the MINIP protocol part
	 */
	hdr->pkt_type = MINIP_CONN_REQUEST;
	hdr->sdu_len = (u16)syn_len;
	hdr->id = htonl(id);
	hdr->seq = htonl(MINIP_FIRST_SEQ);
	hdr->ack = htonl(MINIP_FIRST_SEQ);
	hdr->ts = htonl(jiffies_to_msecs(jiffies));

	syn->saddr = saddr;
	syn->source = source;
	syn->daddr = daddr;
	syn->dest = dest;

	skb->protocol = htons(proto);
	skb->no_fcs = 1;
	skb->pkt_type = PACKET_OUTGOING;

	return dev_queue_xmit(skb);
out:
	kfree_skb(skb);

	return -1;
}

/* static int pepdna_minip_conn_request(__be32 saddr, __be16 source, */
/*				     __be32 daddr, __be16 dest, */
/*				     __u32 id) */
/* { */
/*	/\* FIXME *\/ */
/*	struct net_device *dev = dev_get_by_name(&init_net, ifname); */
/*	struct net *net = dev_net(dev); */
/*	/\* static uint16_t proto = ETH_P_MINIP; *\/ */
/*	int hlen = LL_RESERVED_SPACE(dev); */
/*	int tlen = dev->needed_tailroom; */
/*	int hdr_len = sizeof(struct minip_hdr); */
/*	int syn_len = sizeof(struct syn_tuple); */
/*	struct minip_hdr *hdr; */
/*	struct syn_tuple *syn; */
/*	struct flowi4 fl4; */
/*	struct iphdr *iph; */

/*	struct rtable *rt; */
/*	rt = ip_route_output_ports(net, &fl4, NULL, daddr, 0, */
/*				   0, 0, */
/*				   IPPROTO_IP, 0, dev->ifindex); */

/*	if (IS_ERR(rt)) { */
/*		pep_err("Cannot route skb\n"); */
/*		return -1; */
/*	} */

/*	/\* skb *\/ */
/*	struct sk_buff* skb = alloc_skb(syn_len + MINIP_SIZE + hlen + tlen, GFP_ATOMIC); */
/*	if (!skb) */
/*		return -ENOMEM; */

/*	skb_dst_set(skb, &rt->dst); */
/*	skb_reserve(skb, hlen); */

/*	skb_reset_network_header(skb); */

/*	iph = ip_hdr(skb); */
/*	skb_put(skb, sizeof(struct iphdr)); */

/*	iph->version = 4; */
/*	iph->ihl = (sizeof(*iph)) >> 2; */
/*	iph->frag_off =	0; */
/*	iph->protocol = IPPROTO_MINIP; */
/*	iph->check = 0; */
/*	iph->tos = 0; */
/*	iph->tot_len = htons(syn_len + MINIP_SIZE); */
/*	iph->daddr = daddr; */
/*	iph->saddr = fl4.saddr; */
/*	iph->ttl = 64; */

/*	hdr = skb_put(skb, hdr_len); */
/*	/\* */
/*	 * Fill out the MINIP protocol part */
/*	 *\/ */
/*	hdr->pkt_type = MINIP_CONN_REQUEST; */
/*	hdr->sdu_len = (u16)syn_len; */
/*	hdr->id = htonl(id); */
/*	hdr->seq = htonl(1u); */
/*	hdr->ack = htonl(1u); */

/*	syn = skb_put(skb, syn_len); */

/*	syn->saddr = saddr; */
/*	syn->source = source; */
/*	syn->daddr = daddr; */
/*	syn->dest = dest; */


/*	return ip_local_out(&init_net, skb->sk, skb); */

/* out: */
/*	kfree_skb(skb); */

/*	return -1; */
/* } */

void pepdna_minip_handshake(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, tcfa_work);
	int rc = 0;

	rc = pepdna_minip_conn_request(con->tuple.saddr, con->tuple.source,
				       con->tuple.daddr, con->tuple.dest,
				       con->id, con->server->to_mac);
	if (rc < 0) {
		pep_err("Failed to send MINIP_CONN_REQUEST");
		pepdna_con_close(con);
	}

	con->state = REQUEST_SENT;
	con->next_seq++; // init value MINIP_FIRST_SEQ
}

static int pepdna_minip_recv_finished(struct minip_hdr *hdr)
{
	struct pepdna_con *con = NULL;
	u32 hash = ntohl(hdr->id);

	pep_dbg("Recvd MINIP_CONN_FINISHED [cid %u]", hash);

	con = pepdna_con_find(hash);
	if (!con) {
		pep_err("Conn. instance %u not found", hash);
		return -1;
	}
	atomic_set(&con->rflag, 0);
	pepdna_con_close(con);

	return 0;
}

static int pepdna_minip_recv_delete(struct minip_hdr *hdr)
{
	struct pepdna_con *con = NULL;
	u32 hash = ntohl(hdr->id);
	pep_dbg("Recvd MINIP_CONN_DELETE [cid %u]", hash);

	con = pepdna_con_find(hash);
	if (!con) {
		pep_err("Conn. instance %u not found", hash);
		return -1;
	}

	pepdna_minip_conn_finished(con->id, con->server->to_mac);
	pep_dbg("Sent MINIP_CONN_FINISHED [cid %u]", hash);
	atomic_set(&con->rflag, 0);
	pepdna_con_close(con);

	return 0;
}

static int pepdna_minip_recv_ack(struct minip_hdr *hdr)
{
	struct pepdna_con *con = NULL;
	u32 ack, hash, rtt;
	int credit;

	hash = ntohl(hdr->id);

	con = pepdna_con_find(hash);
	if (!con) {
		pep_err("Conn. instance %u not found", hash);
		return -1;
	}

	ack = ntohl(hdr->ack);
	pep_dbg("Recvd ACK %u (last_acked %u) (SRTT=%ums) [cid %u]", ack,
		(u32)atomic_read(&con->last_acked), con->srtt >> 3, hash);

		/* old ACK? silently drop it.. */
	if (unlikely(ack < (u32)atomic_read(&con->last_acked))) {
		pep_dbg("Dropping old ACK");
		return 0;
	}

	/* update RTO with the new sampled RTT, if this is a good ACK */
	rtt = jiffies_to_msecs(jiffies) - ntohl(hdr->ts);
	if (hdr->ts && rtt)
		minip_update_rto(con, rtt);

	/* ACK arrived... reset the timer */
	mod_timer(&con->timer, jiffies + msecs_to_jiffies(con->rto));

	/* check if this ACK acks new data */
	if (ack > (u32)atomic_read(&con->last_acked)) {
		pep_dbg("ACK %u arrived (last_acked %u)", ack, (u32)atomic_read(&con->last_acked));

		/* reset the duplicate ACKs counter */
		atomic_set(&con->dup_acks, 0);

		/* Update LWE */
		atomic_set(&con->last_acked, ack - 1);
		credit = rtxq_ack(con->rtxq, ack);

		if (con->state == RECOVERY) {
			/* Resume sending new data */
			atomic_set(&con->sending, 1);
			con->state = ESTABLISHED;
			pep_dbg("Resume sending new data");
		}

		/* Update cwnd */
		con->window += credit;
		pep_dbg("Good ACK Cwnd update to %u", con->window);

		/* Double check cwnd once per RTT */
                if (rtxq_size(con) <  (WINDOW_SIZE >> 1)) {
			pep_dbg("Updating cwnd since rtxq is almost empty");
			minip_update_cwnd(con);
		}

		con->lsock->sk->sk_data_ready(con->lsock->sk);

		return 0;
	}

	/* check if this ACK is a duplicate */
	if ((u32)atomic_read(&con->last_acked) == ack) {
		atomic_inc(&con->dup_acks);
		con->state = RECOVERY;
		atomic_set(&con->sending, 0);

		if (atomic_read(&con->dup_acks) != 1) {
			pep_dbg("Dropping DupACKs");
			return 0;
		}

		/* if this is the first duplicate ACK do Full Retransmit */
		if (rtxq_full_rtx(con->rtxq, ack) < 0) {
			/* Send a MINIP_CONN_DELETE to deallocate the flow */
			if (pepdna_minip_conn_delete(con->id,
						     con->server->to_mac) < 0) {
				pep_err("failed to send MINIP_CONN_DELETE");
			}
			pep_dbg("Sent MINIP_CONN_DELETE [cid %u]", con->id);
			pepdna_con_close(con);
		}
	}

	pep_dbg("window changed to %u", con->window);

	return 0;
}

static int pepdna_minip_recv_data(struct minip_hdr *hdr, struct sk_buff *skb)
{
	struct pepdna_con *con = NULL;
	u32 hash = ntohl(hdr->id);
	int rc = 0;

	pep_dbg("Recvd MINIP pkt seq %u [cid %u]", ntohl(hdr->seq), hash);

	con = pepdna_con_find(hash);
	if (!con) {
		pep_err("Conn. instance %u not found", hash);
		return -1;
	}

	/* FIXME FIXME FIXME FIXME FIXME FIXME */
	rc = pepdna_con_minip2i_fwd(con, skb);

	if (unlikely(rc <= 0)) {
		if (unlikely(rc == -EAGAIN)) {
			pep_dbg("No MINIP data available right now");
		} else {
			/* Send a MINIP_CONN_DELETE to deallocate the flow */
			if (pepdna_minip_conn_delete(con->id,
						     con->server->to_mac) < 0) {
				pep_err("Failed to send MINIP_CONN_DELETE");
			}
			pep_dbg("Sent MINIP_CONN_DELETE [cid %u]", con->id);
			pepdna_con_close(con);
		}
	}

	/* read_lock_bh(&sk->sk_callback_lock); */
	/* pepdna_con_get(con); */
	/* if (!queue_work(con->server->r2l_wq, &con->r2l_work)) { */
	/*	pepdna_con_put(con); */
	/* } */
	/* read_unlock_bh(&sk->sk_callback_lock); */

	return 0;
}

static int pepdna_minip_recv_response(struct minip_hdr *hdr)
{
	struct pepdna_con *con = NULL;
	u32 hash = ntohl(hdr->id);

	pep_dbg("Recvd MINIP_CONN_RESPONSE [cid %u]", hash);

	con = pepdna_con_find(hash);
	if (!con) {
		pep_err("Conn. instance %u not found", hash);
		return -1;
	}

	atomic_set(&con->rflag, 1);
	atomic_inc(&con->last_acked);
	con->state = ESTABLISHED;
	con->next_recv++;

	/* At this point, MINIP flow is allocated. Reinject SYN in back
	 * in the stack so that the left TCP connection can be
	 * established. There is no need to set callbacks here for the
	 * left socket as pepdna_tcp_accept() will take care of it.
	 */
	pep_dbg("Reinjecting initial SYN back to the stack");
#ifndef CONFIG_PEPDNA_LOCAL_SENDER
	netif_receive_skb(con->skb);
#else
	struct net *net = sock_net(con->server->listener->sk);
	ip_local_out(net, con->server->listener->sk, con->skb);
#endif

	return 0;
}

static int pepdna_minip_recv_request(struct sk_buff *skb)
{
	struct pepdna_con *con = NULL;
	struct syn_tuple *syn  = NULL;
	u32 hash;

	skb_pull(skb, sizeof(struct minip_hdr));
	syn = (struct syn_tuple *)skb->data;
	hash = pepdna_hash32_rjenkins1_2(syn->saddr, syn->source);

	pep_dbg("Recvd MINIP_CONN_REQUEST [cid %u]", hash);

	con = pepdna_con_alloc(syn, NULL, hash, 0ull, 0);
	if (!con) {
		pep_err("Failed to allocate a pepdna conn. instance");
		return -1;
	}

	return 0;
}

int pepdna_minip_recv_packet(struct sk_buff *skb)
{
	struct minip_hdr *hdr = (struct minip_hdr *)skb_network_header(skb);
	int ret = 0;

	switch (hdr->pkt_type) {
	case MINIP_CONN_REQUEST:
		ret = pepdna_minip_recv_request(skb);
		break;
	case MINIP_CONN_RESPONSE:
		ret = pepdna_minip_recv_response(hdr);
		break;
	case MINIP_CONN_DATA:
		ret = pepdna_minip_recv_data(hdr, skb);
		break;
	case MINIP_CONN_ACK:
		ret = pepdna_minip_recv_ack(hdr);
		break;
	case MINIP_CONN_DELETE:
		ret = pepdna_minip_recv_delete(hdr);
		break;
	case MINIP_CONN_FINISHED:
		ret = pepdna_minip_recv_finished(hdr);
		break;
	default:
		break;
	}

	kfree_skb(skb); skb = NULL;

	return ret;
}

static bool can_send(struct pepdna_con *con)
{
	if (!atomic_read(&con->sending) || !con->window)
		return false;
	return true;
}

/*
 * Forward data from TCP socket to MINIP flow
 * ------------------------------------------------------------------------- */
static int pepdna_con_i2minip_fwd(struct pepdna_con *con)
{
	struct socket *lsock = con->lsock;
	unsigned char *buff = NULL;
	size_t how_much = MINIP_MSS;
	int read = 0, sent = 0;
	struct msghdr msg;
	struct kvec vec;

	if (!can_send(con)) {
		pep_dbg("Cannot forward to MINIP at this moment");
		return -EAGAIN;
	}

	how_much *= con->window;

	/* allocate buffer memory */
	buff = kzalloc(how_much, GFP_KERNEL);
	if (!buff) {
		pep_err("Failed to allocate buffer");
		return -ENOMEM;
	}
	msg.msg_flags = MSG_DONTWAIT,
	vec.iov_base = buff;
	vec.iov_len  = how_much;

	read = kernel_recvmsg(lsock, &msg, &vec, 1, vec.iov_len, MSG_DONTWAIT);
	pep_dbg("read %d/%zu bytes from TCP sock", read, how_much);
	if (likely(read > 0)) {
		sent = pepdna_minip_conn_snd_data(con, buff, read);
		if (sent < 0) {
			pep_err("error forwarding to minip");
			kfree(buff);
			return -1;
		}
	} else {
		if (read == -EAGAIN || read == -EWOULDBLOCK) {
			pep_dbg("No TCP data available");
		}
	}

	kfree(buff);
	return read;
}

/*
 * Forward data from MINIP flow to TCP socket
 * ------------------------------------------------------------------------- */
static int pepdna_con_minip2i_fwd(struct pepdna_con *con, struct sk_buff *skb)
{
	struct socket *lsock = con->lsock;
	struct minip_hdr *hdr;
	unsigned char *buf;
	int read = 0, sent = 0;

	hdr = (struct minip_hdr *)skb_network_header(skb);
	read = hdr->sdu_len;

	skb_pull(skb, sizeof(struct minip_hdr));
	buf = (unsigned char *)skb->data;

	pep_dbg("Forwarding MINIP pkt seq %u to TCP", ntohl(hdr->seq));

	if (ntohl(hdr->seq) == con->next_recv) {
		pep_dbg("Yesss, seq %u is in order", ntohl(hdr->seq));
		sent = pepdna_sock_write(lsock, buf, read);
		if (sent < 0) {
			pep_dbg("Forwarded %d out of %d bytes from MINIP to TCP", read, sent);
			read = -1;
		} else {
			con->next_recv++;
		}
	} else {
		pep_dbg("Nooo, seq %u is out of order", ntohl(hdr->seq));
		read = -EAGAIN;
	}

	/* Send an ACK with the expected sequence */
	pepdna_minip_send_ack(con->id, con->next_recv, con->server->to_mac,
			      hdr->ts);
	pep_dbg("Sent ACK %u", con->next_recv);

	return read;
}

/* TCP2MINIP
 * Forward traffic from INTERNET to MINIP
 * ------------------------------------------------------------------------- */
void pepdna_con_i2m_work(struct work_struct *work)
{
	struct pepdna_con *con = container_of(work, struct pepdna_con, l2r_work);
	int rc = 0;

	while (lconnected(con)) {
		if ((rc = pepdna_con_i2minip_fwd(con)) <= 0) {
			if (rc == -EAGAIN) { // FIXME Handle -EAGAIN flood
				pep_dbg("Try again later...");
				break;
			}
			/* Send a MINIP_CONN_DELETE to deallocate the flow, but
			 * wait for the MINIP_CONN_FINISHED to free resources
			 */
			if (pepdna_minip_conn_delete(con->id,
						     con->server->to_mac) < 0) {
				pep_err("failed to send MINIP_CONN_DELETE");
			}
			pep_dbg("Sent MINIP_CONN_DELETE [cid %u]", con->id);
			pepdna_con_close(con);
		}
	}
	/* this work is launched with pepdna_con_get() */
	pepdna_con_put(con);
}

/*
 * MINIP2TCP
 * Forward traffic from MINIP to INTERNET
 * ------------------------------------------------------------------------- */
void pepdna_con_m2i_work(struct work_struct *work)
{
	/* struct pepdna_con *con = container_of(work, struct pepdna_con, r2l_work); */
	/* int rc = pepdna_con_minip2i_fwd(con); */

	/* if (unlikely(rc <= 0)) { */
	/*	if (unlikely(rc == -EAGAIN)) { */
	/*		pep_dbg("Received an unexpected MINIP packet %d", rc); */
	/*		/\* cond_resched(); *\/ */
	/*	} else { */
	/*		atomic_set(&con->rflag, 0); */
	/*		pepdna_con_close(con); */
	/*	} */
	/* } */
	/* pepdna_con_put(con); */
}

		/* /\* FIXME FIXME FIXME FIXME FIXME FIXME *\/ */
		/* int rc = pepdna_con_minip2i_fwd(con, skb); */

		/* if (unlikely(rc <= 0)) { */
		/*	if (unlikely(rc == -EAGAIN)) { */
		/*		pep_dbg("No MINIP data available right now"); */
		/*	} else { */
		/*		atomic_set(&con->rflag, 0); */

		/*		/\* Send a MINIP_CONN_DELETE to deallocate the flow *\/ */
		/*		pep_dbg("Sending MINIP_CONN_DELETE with cid %u", con->id); */
		/*		if (pepdna_minip_conn_delete(con->id) < 0) { */
		/*			pep_err("failed to send MINIP_CONN_DELETE"); */
		/*		} */

		/*		pepdna_con_close(con); */
		/*	} */
		/* } */

		/* /\* read_lock_bh(&sk->sk_callback_lock); *\/ */
		/* pepdna_con_get(con); */
		/* if (!queue_work(con->server->r2l_wq, &con->r2l_work)) { */
		/*	pepdna_con_put(con); */
		/* } */
		/* /\* read_unlock_bh(&sk->sk_callback_lock); *\/ */
