/*
 * $Id$
 */

extern struct mdhost mdhost_list;
struct peer;
struct gfm_connection;
struct abstract_host;

struct abstract_host *mdhost_to_abstract_host(struct mdhost *);

void mdhost_init();
const char *mdhost_get_name(struct mdhost *);
int mdhost_get_port(struct mdhost *);
struct peer *mdhost_get_peer(struct mdhost *);
int mdhost_is_master(struct mdhost *);
int mdhost_is_self(struct mdhost *);
int mdhost_is_up(struct mdhost *);
void mdhost_activate(struct mdhost *);
void mdhost_set_peer(struct mdhost *, struct peer *, int);
struct gfm_connection *mdhost_get_connection(struct mdhost *);
void mdhost_set_connection(struct mdhost *, struct gfm_connection *);
struct mdhost *mdhost_lookup(const char *);
struct mdhost *mdhost_lookup_master(void);
struct mdhost *mdhost_lookup_self(void);
void mdhost_disconnect(struct mdhost *, struct peer *);
#ifdef ENABLE_JOURNAL
struct journal_file_reader;
struct journal_file_reader *mdhost_get_journal_file_reader(struct mdhost *);
gfarm_uint64_t mdhost_get_last_fetch_seqnum(struct mdhost *);
void mdhost_set_last_fetch_seqnum(struct mdhost *, gfarm_uint64_t);
#endif
