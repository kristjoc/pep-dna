#ifndef _PEPDNA_MINIP_H
#define _PEPDNA_MINIP_H

#ifdef CONFIG_PEPDNA_MINIP
#include <linux/workqueue.h>
#include <linux/timer.h>

#define ETH_ALEN      6
#define ETH_P_MINIP   0x88FF
#define ETH_BROADCAST { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }


/**
 * MINIP_FIRST_SEQ - First seqno of each session. The number is rather high
 *  in order to immediately trigger a wrap around (test purposes)
 */
#define MINIP_FIRST_SEQ 9u

#define WINDOW_SIZE	12u
#define MAX_MINIP_RETRY 3u
/**
 * MINIP_RECV_TIMEOUT - Sender activity timeout. If the sender does not
 *  get any 'good' ACK for such amount of milliseconds, it resends unacked pkts.
 */
#define MINIP_RTO 3000u

/**
 * struct minip_hdr - MINIP header
 * @pkt_type: MINIP packet type
 * @sdu_len:  SDU length in bytes
 * @conn_id:  hash(src IP, src port, dst IP, dst port)
 * @seq:      sequence number
 * @ack:      acknowledge number
 * @ts:       time when the packet has been sent

 */
struct minip_hdr {
	u8  pkt_type;
	u16 sdu_len;
	u32 id;
	u32 seq;
	u32 ack;
	__be32 ts;
} __attribute__ ((packed));

/**
 * enum minip_packet_type - MINIP packet type
 * @MINIP_CONN_REQUEST:  SYN
 * @MINIP_CONN_RESPONSE: SYN/ACK
 * @MINIP_CONN_DELETE:   FIN
 * @MINIP_CONN_FINISHED: FIN/ACK
 * @MINIP_CONN_DATA:     DATA
 * @MINIP_CONN_ACK:      ACK
 */
enum minip_packet_type {
	MINIP_CONN_REQUEST  = 0x01,
	MINIP_CONN_RESPONSE = 0x02,
	MINIP_CONN_DELETE   = 0x03,
	MINIP_CONN_FINISHED = 0x04,
	MINIP_CONN_DATA     = 0x05,
	MINIP_CONN_ACK      = 0x06,
};

enum minip_state {
	REQUEST_SENT  = 0x01,
	REQUEST_RECVD = 0x02,
	ESTABLISHED   = 0x03,
	RECOVERY      = 0x04,
};

struct rtxq_entry {
	/* unsigned long    time_stamp; */
	struct sk_buff *skb;
	u8 retries;
	struct list_head next;
};

struct rtxqueue {
	int len;
	struct list_head head;
};

struct rtxq {
	spinlock_t       lock;
	struct rtxqueue *queue;
};

/**
 * minip_has_timed_out() - compares current time (jiffies) and timestamp +
 *  timeout
 * @timestamp:		base value to compare with (in jiffies)
 * @timeout:		added to base value before comparing (in milliseconds)
 *
 * Return: true if current time is after timestamp + timeout
 */
static inline bool minip_has_timed_out(unsigned long timestamp,
				       unsigned int timeout)
{
	return time_is_before_jiffies(timestamp + msecs_to_jiffies(timeout));
}


int pepdna_minip_send_response(u32, u8 *);
int pepdna_minip_recv_packet(struct sk_buff *);
void pepdna_minip_handshake(struct work_struct *);
void pepdna_con_i2m_work(struct work_struct *);
void pepdna_con_m2i_work(struct work_struct *);
void minip_sender_timeout(struct timer_list *);

struct rtxq *rtxq_create(void);
int rtxq_destroy(struct rtxq *);

#endif /* CONFIG_PEPDNA_MINIP */

#endif /* _PEPDNA_MINIP_H */
