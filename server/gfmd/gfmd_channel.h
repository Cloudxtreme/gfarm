/*
 * $Id$
 */

struct peer;
struct gfmdc_journal_send_closure;

gfarm_error_t gfm_server_switch_gfmd_channel(
	struct peer *, gfp_xdr_xid_t, size_t *, int, int);
void gfmdc_init(void);
void gfmdc_pre_init(void);
void *gfmdc_journal_asyncsend_thread(void *);
void *gfmdc_connect_thread(void *);
gfarm_error_t gfmdc_client_remote_peer_alloc(struct peer *);
gfarm_error_t gfmdc_client_remote_peer_free(gfarm_uint64_t);
