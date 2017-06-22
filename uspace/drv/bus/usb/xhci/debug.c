/*
 * Copyright (c) 2017 Ondrej Hlavaty
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup drvusbxhci
 * @{
 */
/** @file
 * Memory-mapped register structures of the xHC.
 */

#include <inttypes.h>
#include <usb/debug.h>

#include "hw_struct/trb.h"
#include "debug.h"
#include "hc.h"

#define PX "\t%-21s = "

#define DUMP_REG_FIELD(ptr, title, size, ...) \
	usb_log_debug2(PX "%" PRIu##size, title, XHCI_REG_RD_FIELD(ptr, size, ##__VA_ARGS__))

#define DUMP_REG_RANGE(ptr, title, size, ...) \
	usb_log_debug2(PX "%" PRIu##size, title, XHCI_REG_RD_RANGE(ptr, size, ##__VA_ARGS__))

#define DUMP_REG_FLAG(ptr, title, size, ...) \
	usb_log_debug2(PX "%s", title, XHCI_REG_RD_FLAG(ptr, size, ##__VA_ARGS__) ? "true" : "false")

#define DUMP_REG_INNER(set, title, field, size, type, ...) \
	DUMP_REG_##type(&(set)->field, title, size, ##__VA_ARGS__)

#define DUMP_REG(set, c) DUMP_REG_INNER(set, #c, c)

/**
 * Dumps all capability registers.
 */
void xhci_dump_cap_regs(xhci_cap_regs_t *cap)
{
	usb_log_debug2("Capabilities:");

	DUMP_REG(cap, XHCI_CAP_LENGTH);
	DUMP_REG(cap, XHCI_CAP_VERSION);
	DUMP_REG(cap, XHCI_CAP_MAX_SLOTS);
	DUMP_REG(cap, XHCI_CAP_MAX_INTRS);
	DUMP_REG(cap, XHCI_CAP_MAX_PORTS);
	DUMP_REG(cap, XHCI_CAP_IST);
	DUMP_REG(cap, XHCI_CAP_ERST_MAX);
	usb_log_debug2(PX "%u", "Max Scratchpad bufs", xhci_get_max_spbuf(cap));
	DUMP_REG(cap, XHCI_CAP_SPR);
	DUMP_REG(cap, XHCI_CAP_U1EL);
	DUMP_REG(cap, XHCI_CAP_U2EL);
	DUMP_REG(cap, XHCI_CAP_AC64);
	DUMP_REG(cap, XHCI_CAP_BNC);
	DUMP_REG(cap, XHCI_CAP_CSZ);
	DUMP_REG(cap, XHCI_CAP_PPC);
	DUMP_REG(cap, XHCI_CAP_PIND);
	DUMP_REG(cap, XHCI_CAP_C);
	DUMP_REG(cap, XHCI_CAP_LTC);
	DUMP_REG(cap, XHCI_CAP_NSS);
	DUMP_REG(cap, XHCI_CAP_PAE);
	DUMP_REG(cap, XHCI_CAP_SPC);
	DUMP_REG(cap, XHCI_CAP_SEC);
	DUMP_REG(cap, XHCI_CAP_CFC);
	DUMP_REG(cap, XHCI_CAP_MAX_PSA_SIZE);
	DUMP_REG(cap, XHCI_CAP_XECP);
	DUMP_REG(cap, XHCI_CAP_DBOFF);
	DUMP_REG(cap, XHCI_CAP_RTSOFF);
	DUMP_REG(cap, XHCI_CAP_U3C);
	DUMP_REG(cap, XHCI_CAP_CMC);
	DUMP_REG(cap, XHCI_CAP_FSC);
	DUMP_REG(cap, XHCI_CAP_CTC);
	DUMP_REG(cap, XHCI_CAP_LEC);
	DUMP_REG(cap, XHCI_CAP_CIC);
}

void xhci_dump_port(xhci_port_regs_t *port)
{
	DUMP_REG(port, XHCI_PORT_CCS);
	DUMP_REG(port, XHCI_PORT_PED);
	DUMP_REG(port, XHCI_PORT_OCA);
	DUMP_REG(port, XHCI_PORT_PR);
	DUMP_REG(port, XHCI_PORT_PLS);
	DUMP_REG(port, XHCI_PORT_PP);
	DUMP_REG(port, XHCI_PORT_PIC);
	DUMP_REG(port, XHCI_PORT_LWS);
	DUMP_REG(port, XHCI_PORT_CSC);
	DUMP_REG(port, XHCI_PORT_PEC);
	DUMP_REG(port, XHCI_PORT_WRC);
	DUMP_REG(port, XHCI_PORT_OCC);
	DUMP_REG(port, XHCI_PORT_PRC);
	DUMP_REG(port, XHCI_PORT_PLC);
	DUMP_REG(port, XHCI_PORT_CEC);
	DUMP_REG(port, XHCI_PORT_CAS);
	DUMP_REG(port, XHCI_PORT_WCE);
	DUMP_REG(port, XHCI_PORT_WDE);
	DUMP_REG(port, XHCI_PORT_WOE);
	DUMP_REG(port, XHCI_PORT_DR);
	DUMP_REG(port, XHCI_PORT_WPR);
}

void xhci_dump_state(xhci_hc_t *hc)
{
	usb_log_debug2("Operational registers:");

	DUMP_REG(hc->op_regs, XHCI_OP_RS);
	DUMP_REG(hc->op_regs, XHCI_OP_HCRST);
	DUMP_REG(hc->op_regs, XHCI_OP_INTE);
	DUMP_REG(hc->op_regs, XHCI_OP_HSEE);
	DUMP_REG(hc->op_regs, XHCI_OP_LHCRST);
	DUMP_REG(hc->op_regs, XHCI_OP_CSS);
	DUMP_REG(hc->op_regs, XHCI_OP_CRS);
	DUMP_REG(hc->op_regs, XHCI_OP_EWE);
	DUMP_REG(hc->op_regs, XHCI_OP_EU3S);
	DUMP_REG(hc->op_regs, XHCI_OP_CME);
	DUMP_REG(hc->op_regs, XHCI_OP_HCH);
	DUMP_REG(hc->op_regs, XHCI_OP_HSE);
	DUMP_REG(hc->op_regs, XHCI_OP_EINT);
	DUMP_REG(hc->op_regs, XHCI_OP_PCD);
	DUMP_REG(hc->op_regs, XHCI_OP_SSS);
	DUMP_REG(hc->op_regs, XHCI_OP_RSS);
	DUMP_REG(hc->op_regs, XHCI_OP_SRE);
	DUMP_REG(hc->op_regs, XHCI_OP_CNR);
	DUMP_REG(hc->op_regs, XHCI_OP_HCE);
	DUMP_REG(hc->op_regs, XHCI_OP_PAGESIZE);
	DUMP_REG(hc->op_regs, XHCI_OP_NOTIFICATION);
	DUMP_REG(hc->op_regs, XHCI_OP_RCS);
	DUMP_REG(hc->op_regs, XHCI_OP_CS);
	DUMP_REG(hc->op_regs, XHCI_OP_CA);
	DUMP_REG(hc->op_regs, XHCI_OP_CRR);
	DUMP_REG(hc->op_regs, XHCI_OP_CRCR_LO);
	DUMP_REG(hc->op_regs, XHCI_OP_CRCR_HI);
	DUMP_REG(hc->op_regs, XHCI_OP_DCBAAP_LO);
	DUMP_REG(hc->op_regs, XHCI_OP_DCBAAP_HI);
	DUMP_REG(hc->rt_regs, XHCI_RT_MFINDEX);

	usb_log_debug2("Interrupter 0 state:");
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_IP);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_IE);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_IMI);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_IMC);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_ERSTSZ);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_ERSTBA_LO);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_ERSTBA_HI);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_ERDP_LO);
	DUMP_REG(&hc->rt_regs->ir[0], XHCI_INTR_ERDP_HI);
}

void xhci_dump_ports(xhci_hc_t *hc)
{
	const size_t num_ports = XHCI_REG_RD(hc->cap_regs, XHCI_CAP_MAX_PORTS);
	for (size_t i = 0; i < num_ports; i++) {
		usb_log_debug2("Port %zu state:", i);

		xhci_dump_port(&hc->op_regs->portrs[i]);
	}
}

static const char *trb_types [] = {
	[0] = "<empty>",
#define TRB(t) [XHCI_TRB_TYPE_##t] = #t
	TRB(NORMAL),
	TRB(SETUP_STAGE),
	TRB(DATA_STAGE),
	TRB(STATUS_STAGE),
	TRB(ISOCH),
	TRB(LINK),
	TRB(EVENT_DATA),
	TRB(NO_OP),
	TRB(ENABLE_SLOT_CMD),
	TRB(DISABLE_SLOT_CMD),
	TRB(ADDRESS_DEVICE_CMD),
	TRB(CONFIGURE_ENDPOINT_CMD),
	TRB(EVALUATE_CONTEXT_CMD),
	TRB(RESET_ENDPOINT_CMD),
	TRB(STOP_ENDPOINT_CMD),
	TRB(SET_TR_DEQUEUE_POINTER_CMD),
	TRB(RESET_DEVICE_CMD),
	TRB(FORCE_EVENT_CMD),
	TRB(NEGOTIATE_BANDWIDTH_CMD),
	TRB(SET_LATENCY_TOLERANCE_VALUE_CMD),
	TRB(GET_PORT_BANDWIDTH_CMD),
	TRB(FORCE_HEADER_CMD),
	TRB(NO_OP_CMD),
	TRB(TRANSFER_EVENT),
	TRB(COMMAND_COMPLETION_EVENT),
	TRB(PORT_STATUS_CHANGE_EVENT),
	TRB(BANDWIDTH_REQUEST_EVENT),
	TRB(DOORBELL_EVENT),
	TRB(HOST_CONTROLLER_EVENT),
	TRB(DEVICE_NOTIFICATION_EVENT),
	TRB(MFINDEX_WRAP_EVENT),
#undef TRB
	[XHCI_TRB_TYPE_MAX] = NULL,
};

const char *xhci_trb_str_type(unsigned type)
{
	static char type_buf [20];

	if (type < XHCI_TRB_TYPE_MAX && trb_types[type] != NULL)
		return trb_types[type];

	snprintf(type_buf, sizeof(type_buf), "<unknown (%u)>", type);
	return type_buf;
}

void xhci_dump_trb(xhci_trb_t *trb)
{
	usb_log_debug2("TRB(%p): type %s, cycle %u", trb, xhci_trb_str_type(TRB_TYPE(*trb)), TRB_CYCLE(*trb));
}

/**
 * @}
 */
