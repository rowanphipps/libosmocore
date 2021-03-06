/*! \file gprs_ns.h */

#pragma once

#include <stdint.h>

/* Our Implementation */
#include <netinet/in.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/select.h>
#include <osmocom/gprs/gprs_msgb.h>

#include <osmocom/gprs/protocol/gsm_08_16.h>

#define NS_TIMERS_COUNT 7
#define NS_TIMERS "(tns-block|tns-block-retries|tns-reset|tns-reset-retries|tns-test|tns-alive|tns-alive-retries)"
#define NS_TIMERS_HELP	\
	"(un)blocking Timer (Tns-block) timeout\n"		\
	"(un)blocking Timer (Tns-block) number of retries\n"	\
	"Reset Timer (Tns-reset) timeout\n"			\
	"Reset Timer (Tns-reset) number of retries\n"		\
	"Test Timer (Tns-test) timeout\n"			\
	"Alive Timer (Tns-alive) timeout\n"			\
	"Alive Timer (Tns-alive) number of retries\n"

/* Educated guess - LLC user payload is 1500 bytes plus possible headers */
#define NS_ALLOC_SIZE	3072
#define NS_ALLOC_HEADROOM 20

enum ns_timeout {
	NS_TOUT_TNS_BLOCK,
	NS_TOUT_TNS_BLOCK_RETRIES,
	NS_TOUT_TNS_RESET,
	NS_TOUT_TNS_RESET_RETRIES,
	NS_TOUT_TNS_TEST,
	NS_TOUT_TNS_ALIVE,
	NS_TOUT_TNS_ALIVE_RETRIES,
};

#define NSE_S_BLOCKED	0x0001
#define NSE_S_ALIVE	0x0002
#define NSE_S_RESET	0x0004

/*! Osmocom NS link layer types */
enum gprs_ns_ll {
	GPRS_NS_LL_UDP,		/*!< NS/UDP/IP */
	GPRS_NS_LL_E1,		/*!< NS/E1 */
	GPRS_NS_LL_FR_GRE,	/*!< NS/FR/GRE/IP */
};

/*! Osmoco NS events */
enum gprs_ns_evt {
	GPRS_NS_EVT_UNIT_DATA,
};

/*! Osmocom NS VC create status */
enum gprs_ns_cs {
	GPRS_NS_CS_CREATED,     /*!< A NSVC object has been created */
	GPRS_NS_CS_FOUND,       /*!< A NSVC object has been found */
	GPRS_NS_CS_REJECTED,    /*!< Rejected and answered message */
	GPRS_NS_CS_SKIPPED,     /*!< Skipped message */
	GPRS_NS_CS_ERROR,       /*!< Failed to process message */
};

struct gprs_nsvc;
/*! Osmocom GPRS callback function type */
typedef int gprs_ns_cb_t(enum gprs_ns_evt event, struct gprs_nsvc *nsvc,
			 struct msgb *msg, uint16_t bvci);

/*! An instance of the NS protocol stack */
struct gprs_ns_inst {
	/*! callback to the user for incoming UNIT DATA IND */
	gprs_ns_cb_t *cb;

	/*! linked lists of all NSVC in this instance */
	struct llist_head gprs_nsvcs;

	/*! a NSVC object that's needed to deal with packets for
	 * 	   unknown NSVC */
	struct gprs_nsvc *unknown_nsvc;

	uint16_t timeout[NS_TIMERS_COUNT];

	/*! NS-over-IP specific bits */
	struct {
		struct osmo_fd fd;
		uint32_t local_ip;
		uint16_t local_port;
		int dscp;
	} nsip;
	/*! NS-over-FR-over-GRE-over-IP specific bits */
	struct {
		struct osmo_fd fd;
		uint32_t local_ip;
		unsigned int enabled:1;
	} frgre;
};

enum nsvc_timer_mode {
	/* standard timers */
	NSVC_TIMER_TNS_TEST,
	NSVC_TIMER_TNS_ALIVE,
	NSVC_TIMER_TNS_RESET,
	_NSVC_TIMER_NR,
};

/*! Structure representing a single NS-VC */
struct gprs_nsvc {
	/*! list of NS-VCs within NS Instance */
	struct llist_head list;
	/*! pointer to NS Instance */
	struct gprs_ns_inst *nsi;

	uint16_t nsei;	/*! end-to-end significance */
	uint16_t nsvci;	/*! uniquely identifies NS-VC at SGSN */

	uint32_t state;
	uint32_t remote_state;

	struct osmo_timer_list timer;
	enum nsvc_timer_mode timer_mode;
	struct timeval timer_started;
	int alive_retries;

	unsigned int remote_end_is_sgsn:1;
	unsigned int persistent:1;
	unsigned int nsvci_is_valid:1;

	struct rate_ctr_group *ctrg;
	struct osmo_stat_item_group *statg;

	/*! which link-layer are we based on? */
	enum gprs_ns_ll ll;

	union {
		struct {
			struct sockaddr_in bts_addr;
		} ip;
		struct {
			struct sockaddr_in bts_addr;
		} frgre;
	};
};

/* Create a new NS protocol instance */
struct gprs_ns_inst *gprs_ns_instantiate(gprs_ns_cb_t *cb, void *ctx);

/* Close a NS protocol instance */
void gprs_ns_close(struct gprs_ns_inst *nsi);

/* Close and Destroy a NS protocol instance */
void gprs_ns_destroy(struct gprs_ns_inst *nsi);

/* Listen for incoming GPRS packets via NS/UDP */
int gprs_ns_nsip_listen(struct gprs_ns_inst *nsi);

/* Establish a connection (from the BSS) to the SGSN */
struct gprs_nsvc *gprs_ns_nsip_connect(struct gprs_ns_inst *nsi,
					struct sockaddr_in *dest,
					uint16_t nsei, uint16_t nsvci);


struct sockaddr_in;

/* main function for higher layers (BSSGP) to send NS messages */
int gprs_ns_sendmsg(struct gprs_ns_inst *nsi, struct msgb *msg);

int gprs_ns_tx_reset(struct gprs_nsvc *nsvc, uint8_t cause);
int gprs_ns_tx_block(struct gprs_nsvc *nsvc, uint8_t cause);
int gprs_ns_tx_unblock(struct gprs_nsvc *nsvc);

/* Listen for incoming GPRS packets via NS/FR/GRE */
int gprs_ns_frgre_listen(struct gprs_ns_inst *nsi);

struct gprs_nsvc *gprs_nsvc_create(struct gprs_ns_inst *nsi, uint16_t nsvci);
void gprs_nsvc_delete(struct gprs_nsvc *nsvc);
struct gprs_nsvc *gprs_nsvc_by_nsei(struct gprs_ns_inst *nsi, uint16_t nsei);
struct gprs_nsvc *gprs_nsvc_by_nsvci(struct gprs_ns_inst *nsi, uint16_t nsvci);

/* Initiate a RESET procedure (including timer start, ...)*/
int gprs_nsvc_reset(struct gprs_nsvc *nsvc, uint8_t cause);

/* Add NS-specific VTY stuff */
int gprs_ns_vty_init(struct gprs_ns_inst *nsi);

/* Resturn peer info as string (NOTE: the buffer is allocated statically) */
const char *gprs_ns_ll_str(struct gprs_nsvc *nsvc);

/* Copy the link layer info from other into nsvc */
void gprs_ns_ll_copy(struct gprs_nsvc *nsvc, struct gprs_nsvc *other);

/* Clear the link layer info (will never match a real link then) */
void gprs_ns_ll_clear(struct gprs_nsvc *nsvc);

struct msgb *gprs_ns_msgb_alloc(void);

enum signal_ns {
	S_NS_RESET,
	S_NS_BLOCK,
	S_NS_UNBLOCK,
	S_NS_ALIVE_EXP,	/* Tns-alive expired more than N times */
	S_NS_REPLACED, /* nsvc object is replaced (sets old_nsvc) */
	S_NS_MISMATCH, /* got an unexpected IE (sets msg, pdu_type, ie_type) */
};

extern const struct value_string gprs_ns_signal_ns_names[];

struct ns_signal_data {
	struct gprs_nsvc *nsvc;
	struct gprs_nsvc *old_nsvc;
	uint8_t cause;
	uint8_t pdu_type;
	uint8_t ie_type;
	struct msgb *msg;
};

void gprs_ns_set_log_ss(int ss);

/*! @} */
