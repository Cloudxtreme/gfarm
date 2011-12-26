struct abstract_host;
struct host;
struct mdhost;

struct abstract_host_ops {
	/* downcast functions */
	struct host *(*abstract_host_to_host)(struct abstract_host *);
	struct mdhost *(*abstract_host_to_mdhost)(struct abstract_host *);

	const char *(*get_name)(struct abstract_host *);
	int (*get_port)(struct abstract_host *);
	void (*set_peer_locked)(struct abstract_host *, struct peer *);
	void (*set_peer_unlocked)(struct abstract_host *, struct peer *);
	void (*unset_peer)(struct abstract_host *, struct peer *);
	gfarm_error_t (*disable)(struct abstract_host *, void **);
	void (*disabled)(struct abstract_host *, struct peer *, void *);
};

/* common struct of host and mdhost */
struct abstract_host {
	struct abstract_host_ops *ops;

	int invalid;	/* set when deleted */

	pthread_mutex_t mutex;
	/*
	 * resources which are protected by the abstrac_host::mutex
	 */
	pthread_cond_t ready_to_send, ready_to_receive;

	int can_send, can_receive;

	struct peer *peer;
	int protocol_version;
	int is_active;

	gfarm_time_t busy_time;
};

struct host *abstract_host_to_host(struct abstract_host *);
struct mdhost *abstract_host_to_mdhost(struct abstract_host *);

void abstract_host_init(struct abstract_host *, struct abstract_host_ops *,
	const char *diag);
int abstract_host_get_protocol_version(struct abstract_host *);
void abstract_host_invalidate(struct abstract_host *);
void abstract_host_validate(struct abstract_host *);
int abstract_host_is_invalid_unlocked(struct abstract_host *);
int abstract_host_is_valid_unlocked(struct abstract_host *);
int abstract_host_is_valid(struct abstract_host *, const char *);
void abstract_host_activate(struct abstract_host *, const char *);
int abstract_host_is_up_unlocked(struct abstract_host *);
int abstract_host_is_up(struct abstract_host *);
const char *abstract_host_get_name(struct abstract_host *);
int abstract_host_get_port(struct abstract_host *);
int abstract_host_check_busy(struct abstract_host *, gfarm_int64_t,
	const char *);
struct peer *abstract_host_get_peer_unlocked(struct abstract_host *);
struct peer *abstract_host_get_peer(struct abstract_host *, const char *);
void abstract_host_set_peer(struct abstract_host *, struct peer *, int);
void abstract_host_disconnect(struct abstract_host *, struct peer *,
	const char *);

typedef gfarm_error_t (*channel_protocol_switch_t)(struct abstract_host *,
	struct peer *, int, gfp_xdr_xid_t, size_t, int *);

void *gfm_server_channel_main(void *arg,
	channel_protocol_switch_t
#ifdef COMPAT_GFARM_2_3
	,void (*)(struct abstract_host *),
	gfarm_error_t (*)(struct abstract_host *, struct peer *)
#endif
	);
void gfm_server_channel_disconnect_request(struct abstract_host *,
	struct peer *, const char *, const char *, const char *);
void gfm_server_channel_already_disconnected_message(struct abstract_host *,
	const char *, const char *, const char *);
gfarm_error_t gfm_server_channel_vget_request(struct peer *, size_t,
	const char *, const char *, va_list *);
gfarm_error_t gfm_server_channel_vput_reply(struct abstract_host *,
	struct peer *, gfp_xdr_xid_t, const char *, gfarm_error_t,
	const char *, va_list *);

gfarm_error_t gfm_client_channel_vsend_wrapped_request(struct abstract_host *,
	struct peer *, const char *, gfarm_int32_t (*)(void *, void *, size_t),
	void (*)(void *, void *), void *,
#ifdef COMPAT_GFARM_2_3
	void (*)(struct abstract_host *, struct peer *,
	    gfarm_int32_t (*)(void *, void *, size_t),
	    void (*)(void *, void *), void *),
#endif
	const char *, va_list *,
	gfarm_int32_t, const char *, va_list *);
gfarm_error_t gfm_client_channel_vsend_request(struct abstract_host *,
	struct peer *, const char *, gfarm_int32_t (*)(void *, void *, size_t),
	void (*)(void *, void *), void *,
#ifdef COMPAT_GFARM_2_3
	void (*)(struct abstract_host *, struct peer *,
	    gfarm_int32_t (*)(void *, void *, size_t),
	    void (*)(void *, void *), void *),
#endif
	gfarm_int32_t, const char *, va_list *);
gfarm_error_t gfm_client_channel_vrecv_result(struct peer *,
	struct abstract_host *, size_t, const char *, const char **,
	gfarm_error_t *, va_list *);
