#include <linux/module.h>
#include <net/tcp.h>

struct fld_tcp {
	u32	last_una;
	u32	last_nxt;
	u32	sent_bytes;
	u32	acked_bytes;
	u32 	agr;
};

static void tcp_fld_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct fld_tcp *fld = inet_csk_ca(sk);
	tp->turbo = 1;
	tp->snd_cwnd = 10;
	tp->snd_cwnd_clamp = ~0;
	tp->snd_ssthresh = ~0;
	fld->sent_bytes = 0;
	fld->acked_bytes = 0;
	fld->agr = 1;
	fld->last_una = tp->snd_una;
	fld->last_nxt = tp->snd_nxt;	
}

static void tcp_fld_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct fld_tcp *fld = inet_csk_ca(sk);
	
	if (fld->agr) {
		fld->sent_bytes = fld->sent_bytes +
					(tp->snd_nxt - fld->last_nxt);
		fld->last_nxt = tp->snd_nxt;
		if (fld->sent_bytes > 2097152)
			fld->agr = 0;
	}

	if (tp->turbo) {
		fld->acked_bytes = fld->acked_bytes +
						(tp->snd_una - fld->last_una);
		fld->last_una = tp->snd_una;
		if (fld->acked_bytes > 2097152)
			tp->turbo = 0;
	}

	if (tp->snd_cwnd <= tp->snd_ssthresh && fld->agr
	      && tp->snd_ssthresh == ~0){
		tp->snd_cwnd += 2;	
	} else if (tp->snd_cwnd <= tp->snd_ssthresh) {
		tp->snd_cwnd += 1;	
	} else {
		if (tp->snd_cwnd_cnt >= tp->snd_cwnd){
			if (tp->snd_cwnd < tp->snd_cwnd_clamp)
				tp->snd_cwnd++;
			tp->snd_cwnd_cnt = 0;
		} else {
			tp->snd_cwnd_cnt++;		
		}
	}
}

static u32 tcp_fld_ssthresh(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	if (tp->turbo)
		return max(tp->snd_cwnd, 2U);
	else
		return max(tp->snd_cwnd >> 1U, 2U);
}

static u32 tcp_fld_min_cwnd(const struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	if (tp->turbo)
		return tp->snd_cwnd + 1;
	else
		return tp->snd_ssthresh/2;
}

struct tcp_congestion_ops tcp_fld = {
	.name		= "fld",
	.init		= tcp_fld_init,
	.owner		= THIS_MODULE,
	.ssthresh	= tcp_fld_ssthresh,
	.cong_avoid	= tcp_fld_cong_avoid,
	.min_cwnd	= tcp_fld_min_cwnd,
};

static int __init tcp_fld_register(void)
{
	BUILD_BUG_ON(sizeof(struct fld_tcp) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_fld);
}

static void __exit tcp_fld_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_fld);
}

module_init(tcp_fld_register);
module_exit(tcp_fld_unregister);

MODULE_AUTHOR("xxx");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("tcp_fld");
