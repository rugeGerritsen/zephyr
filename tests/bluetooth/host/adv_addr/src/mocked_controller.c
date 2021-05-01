#include <ztest.h>
#include <sys/byteorder.h>
#include <drivers/bluetooth/hci_driver.h>
#include <bluetooth/buf.h>

static bool using_extended_commands;
static bool advertising_enabled;

static bool scan_type_active;
static bool scan_enabled;

static bool random_addr_valid;
static bt_addr_t random_addr;
static bool bdaddr_valid;
static bt_addr_t bdaddr;

void mocked_controller_random_addr_get(bt_addr_t *addr, bool *valid)
{
	memcpy(addr, &random_addr, sizeof(random_addr));
	*valid = random_addr_valid;
}



/* Command handler structure for cmd_handle(). */
struct cmd_handler {
	uint16_t opcode; /* HCI command opcode */
	uint8_t len;     /* HCI command response length */
	void (*handler)(struct net_buf *buf, struct net_buf **evt,
			uint8_t len, uint16_t opcode);
};

/* Add event to net_buf. */
static void evt_create(struct net_buf *buf, uint8_t evt, uint8_t len)
{
	struct bt_hci_evt_hdr *hdr;

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = evt;
	hdr->len = len;
}

/* Create a command complete event. */
static void *cmd_complete(struct net_buf **buf, uint8_t plen, uint16_t opcode)
{
	struct bt_hci_evt_cmd_complete *cc;

	*buf = bt_buf_get_evt(BT_HCI_EVT_CMD_COMPLETE, false, K_FOREVER);
	evt_create(*buf, BT_HCI_EVT_CMD_COMPLETE, sizeof(*cc) + plen);
	cc = net_buf_add(*buf, sizeof(*cc));
	cc->ncmd = 1U;
	cc->opcode = sys_cpu_to_le16(opcode);
	return net_buf_add(*buf, plen);
}

/* Generic command complete with success status. */
static void generic_success(struct net_buf *buf, struct net_buf **evt,
			    uint8_t len, uint16_t opcode)
{
	struct bt_hci_evt_cc_status *ccst;

	ccst = cmd_complete(evt, len, opcode);

	/* Fill any event parameters with zero */
	(void)memset(ccst, 0, len);

	ccst->status = BT_HCI_ERR_SUCCESS;
}

/* Bogus handler for BT_HCI_OP_READ_LOCAL_FEATURES. */
static void read_local_features(struct net_buf *buf, struct net_buf **evt,
				uint8_t len, uint16_t opcode)
{
	struct bt_hci_rp_read_local_features *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = 0x00;
	(void)memset(&rp->features[0], 0xFF, sizeof(rp->features));
}

/* Bogus handler for BT_HCI_OP_READ_SUPPORTED_COMMANDS. */
static void read_supported_commands(struct net_buf *buf, struct net_buf **evt,
				    uint8_t len, uint16_t opcode)
{
	struct bt_hci_rp_read_supported_commands *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	(void)memset(&rp->commands[0], 0xFF, sizeof(rp->commands));
	rp->status = 0x00;
}

/* Bogus handler for BT_HCI_OP_LE_READ_LOCAL_FEATURES. */
static void le_read_local_features(struct net_buf *buf, struct net_buf **evt,
				   uint8_t len, uint16_t opcode)
{
	struct bt_hci_rp_le_read_local_features *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = 0x00;
	(void)memset(&rp->features[0], 0xFF, sizeof(rp->features));
}

/* Bogus handler for BT_HCI_OP_LE_READ_SUPP_STATES. */
static void le_read_supp_states(struct net_buf *buf, struct net_buf **evt,
				uint8_t len, uint16_t opcode)
{
	struct bt_hci_rp_le_read_supp_states *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = 0x00;
	(void)memset(&rp->le_states, 0xFF, sizeof(rp->le_states));
}

/* Validation of what has been sent to le_set_random_address. */
static void le_set_random_address(struct net_buf *buf, struct net_buf **evt,
				uint8_t len, uint16_t opcode)
{
	struct bt_hci_evt_cc_status *ccst;
	uint8_t status;

	if ((advertising_enabled && !using_extended_commands) ||
		(scan_enabled && scan_type_active)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
	} else {
		status = BT_HCI_ERR_SUCCESS;
		memcpy(&random_addr.val[0], (void*)buf->data, sizeof(random_addr));
		random_addr_valid = true;
	}

	ccst = cmd_complete(evt, len, opcode);

	/* Fill any event parameters with zero */
	(void)memset(ccst, 0, len);
	ccst->status = status;
}

static void le_set_scan_params(struct net_buf *buf, struct net_buf **evt,
				uint8_t len, uint16_t opcode)
{
	scan_type_active = net_buf_pull_u8(buf);
	generic_success(buf, evt, len, opcode);
}

static void le_set_scan_enable(struct net_buf *buf, struct net_buf **evt,
				uint8_t len, uint16_t opcode)
{
	scan_enabled = net_buf_pull_u8(buf);
	generic_success(buf, evt, len, opcode);
}

/* Setup handlers needed for bt_enable to function. */
static const struct cmd_handler cmds[] = {
	{ BT_HCI_OP_READ_LOCAL_VERSION_INFO,
	  sizeof(struct bt_hci_rp_read_local_version_info),
	  generic_success },
	{ BT_HCI_OP_READ_SUPPORTED_COMMANDS,
	  sizeof(struct bt_hci_rp_read_supported_commands),
	  read_supported_commands },
	{ BT_HCI_OP_READ_LOCAL_FEATURES,
	  sizeof(struct bt_hci_rp_read_local_features),
	  read_local_features },
	{ BT_HCI_OP_READ_BD_ADDR,
	  sizeof(struct bt_hci_rp_read_bd_addr),
	  generic_success },
	{ BT_HCI_OP_SET_EVENT_MASK,
	  sizeof(struct bt_hci_evt_cc_status),
	  generic_success },
	{ BT_HCI_OP_LE_SET_EVENT_MASK,
	  sizeof(struct bt_hci_evt_cc_status),
	  generic_success },
	{ BT_HCI_OP_LE_READ_LOCAL_FEATURES,
	  sizeof(struct bt_hci_rp_le_read_local_features),
	  le_read_local_features },
	{ BT_HCI_OP_LE_READ_SUPP_STATES,
	  sizeof(struct bt_hci_rp_le_read_supp_states),
	  le_read_supp_states },
	{ BT_HCI_OP_LE_RAND,
	  sizeof(struct bt_hci_rp_le_rand),
	  generic_success },

	/* Custom implementations of commands handling controller
	 * addresses */
	{ BT_HCI_OP_LE_SET_RANDOM_ADDRESS,
	  sizeof(struct bt_hci_evt_cc_status),
	  le_set_random_address },
	{ BT_HCI_OP_LE_SET_SCAN_PARAM,
	  sizeof(struct bt_hci_evt_cc_status),
	  le_set_scan_params },
	{ BT_HCI_OP_LE_SET_SCAN_ENABLE,
	  sizeof(struct bt_hci_evt_cc_status),
	  le_set_scan_enable },
};

static int driver_open(void)
{
	return 0;
}


/* Loop over handlers to try to handle the command given by opcode. */
static void cmd_handle_helper(uint16_t opcode, struct net_buf *cmd,
			     struct net_buf **evt)
{
	for (size_t i = 0; i < ARRAY_SIZE(cmds); i++) {
		const struct cmd_handler *handler = &cmds[i];

		if (handler->opcode != opcode) {
			continue;
		}

		if (handler->handler) {
			handler->handler(cmd, evt, handler->len, opcode);
			return;
		}
	}

	/* Not found, use a generic success implementation */
	generic_success(cmd, evt, sizeof(struct bt_hci_evt_cc_status), opcode);
}


/*  HCI driver send.  */
static int driver_send(struct net_buf *buf)
{
	struct net_buf *evt = NULL;
	struct bt_hci_cmd_hdr *chdr;
	uint16_t opcode;

	chdr = net_buf_pull_mem(buf, sizeof(*chdr));
	opcode = sys_le16_to_cpu(chdr->opcode);

	cmd_handle_helper(opcode, buf, &evt);

	if (evt) {
		bt_recv(evt);
		net_buf_unref(buf);
	}

	return 0;
}

/* HCI driver structure. */
static const struct bt_hci_driver drv = {
	.name         = "mocked_controller",
	.bus          = BT_HCI_DRIVER_BUS_VIRTUAL,
	.open         = driver_open,
	.send         = driver_send,
	.quirks       = 0,
};

static int hci_driver_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	bt_hci_driver_register(&drv);

	return 0;
}

SYS_INIT(hci_driver_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
