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
 * @brief The host controller data bookkeeping.
 */

#include <errno.h>
#include <str_error.h>
#include <usb/debug.h>
#include <usb/host/ddf_helpers.h>
#include <usb/host/utils/malloc32.h>
#include "debug.h"
#include "hc.h"
#include "hw_struct/trb.h"

static const irq_cmd_t irq_commands[] = {
	{
		.cmd = CMD_PIO_READ_32,
		.dstarg = 1,
		.addr = NULL
	},
	{
		.cmd = CMD_AND,
		.srcarg = 1,
		.dstarg = 2,
		.value = 0
	},
	{
		.cmd = CMD_PREDICATE,
		.srcarg = 2,
		.value = 2
	},
	{
		.cmd = CMD_PIO_WRITE_A_32,
		.srcarg = 1,
		.addr = NULL
	},
	{
		.cmd = CMD_ACCEPT
	}
};

/**
 * Generates code to accept interrupts. The xHCI is designed primarily for
 * MSI/MSI-X, but we use PCI Interrupt Pin. In this mode, all the Interrupters
 * (except 0) are disabled.
 */
static int hc_gen_irq_code(irq_code_t *code, const hw_res_list_parsed_t *hw_res)
{
	int err;

	assert(code);
	assert(hw_res);

	if (hw_res->irqs.count != 1 || hw_res->mem_ranges.count != 1) {
		usb_log_info("Unexpected HW resources to enable interrupts.");
		return EINVAL;
	}

	addr_range_t mmio_range = hw_res->mem_ranges.ranges[0];

	if (RNGSZ(mmio_range) < sizeof(xhci_cap_regs_t))
		return EOVERFLOW;


	xhci_cap_regs_t *cap_regs = NULL;
	if ((err = pio_enable_range(&mmio_range, (void **)&cap_regs)))
		return EIO;

	code->ranges = malloc(sizeof(irq_pio_range_t));
	if (code->ranges == NULL)
		return ENOMEM;

	code->cmds = malloc(sizeof(irq_commands));
	if (code->cmds == NULL) {
		free(code->ranges);
		return ENOMEM;
	}

	code->rangecount = 1;
	code->ranges[0] = (irq_pio_range_t) {
	    .base = RNGABS(mmio_range),
	    .size = RNGSZ(mmio_range),
	};

	code->cmdcount = ARRAY_SIZE(irq_commands);
	memcpy(code->cmds, irq_commands, sizeof(irq_commands));

	void *intr0_iman = RNGABSPTR(mmio_range) + XHCI_REG_RD(cap_regs, XHCI_CAP_RTSOFF) + offsetof(xhci_rt_regs_t, ir[0]);
	code->cmds[0].addr = intr0_iman;
	code->cmds[3].addr = intr0_iman;
	code->cmds[1].value = host2xhci(32, 1);

	return hw_res->irqs.irqs[0];
}

static int hc_claim(ddf_dev_t *dev)
{
	// TODO: implement handoff: section 4.22.1
	return EOK;
}

static int hc_reset(xhci_hc_t *hc)
{
	/* Stop the HC: set R/S to 0 */
	XHCI_REG_CLR(hc->op_regs, XHCI_OP_RS, 1);

	/* Wait 16 ms until the HC is halted */
	async_usleep(16000);
	assert(XHCI_REG_RD(hc->op_regs, XHCI_OP_HCH));

	/* Reset */
	XHCI_REG_SET(hc->op_regs, XHCI_OP_HCRST, 1);

	/* Wait until the reset is complete */
	while (XHCI_REG_RD(hc->op_regs, XHCI_OP_HCRST))
		async_usleep(1000);

	return EOK;
}

/**
 * Initialize the HC: section 4.2
 */
static int hc_start(xhci_hc_t *hc, bool irq)
{
	int err;

	if ((err = hc_reset(hc)))
		return err;

	while (XHCI_REG_RD(hc->op_regs, XHCI_OP_CNR))
		async_usleep(1000);

	uint64_t dcbaaptr = addr_to_phys(hc->event_ring.erst);
	XHCI_REG_WR(hc->op_regs, XHCI_OP_DCBAAP_LO, LOWER32(dcbaaptr));
	XHCI_REG_WR(hc->op_regs, XHCI_OP_DCBAAP_HI, UPPER32(dcbaaptr));
	XHCI_REG_WR(hc->op_regs, XHCI_OP_MAX_SLOTS_EN, 0);

	uint64_t crptr = xhci_trb_ring_get_dequeue_ptr(&hc->command_ring);
	XHCI_REG_WR(hc->op_regs, XHCI_OP_CRCR_LO, LOWER32(crptr) >> 6);
	XHCI_REG_WR(hc->op_regs, XHCI_OP_CRCR_HI, UPPER32(crptr));

	uint64_t erstptr = addr_to_phys(hc->event_ring.erst);
	xhci_interrupter_regs_t *intr0 = &hc->rt_regs->ir[0];
	XHCI_REG_WR(intr0, XHCI_INTR_ERSTSZ, hc->event_ring.segment_count);
	XHCI_REG_WR(intr0, XHCI_INTR_ERDP_LO, LOWER32(erstptr));
	XHCI_REG_WR(intr0, XHCI_INTR_ERDP_HI, UPPER32(erstptr));
	XHCI_REG_WR(intr0, XHCI_INTR_ERSTBA_LO, LOWER32(erstptr));
	XHCI_REG_WR(intr0, XHCI_INTR_ERSTBA_HI, UPPER32(erstptr));

	// TODO: Setup scratchpad buffers

	if (irq) {
		XHCI_REG_SET(intr0, XHCI_INTR_IE, 1);
		XHCI_REG_SET(hc->op_regs, XHCI_OP_INTE, 1);
	}

	XHCI_REG_SET(hc->op_regs, XHCI_OP_RS, 1);

	return EOK;
}

static int hc_init(hcd_t *hcd, const hw_res_list_parsed_t *hw_res, bool irq)
{
	int err;

	assert(hcd);
	assert(hw_res);
	assert(hcd_get_driver_data(hcd) == NULL);

	/* Initialize the MMIO ranges */
	if (hw_res->mem_ranges.count != 1) {
		usb_log_error("Unexpected MMIO area, bailing out.");
		return EINVAL;
	}

	addr_range_t mmio_range = hw_res->mem_ranges.ranges[0];

	usb_log_debug("MMIO area at %p (size %zu), IRQ %d.\n",
	    RNGABSPTR(mmio_range), RNGSZ(mmio_range), hw_res->irqs.irqs[0]);

	if (RNGSZ(mmio_range) < sizeof(xhci_cap_regs_t))
		return EOVERFLOW;

	void *base;
	if ((err = pio_enable_range(&mmio_range, &base)))
		return err;

	xhci_hc_t *hc = malloc(sizeof(xhci_hc_t));
	if (!hc)
		return ENOMEM;

	hc->cap_regs = (xhci_cap_regs_t *)  base;
	hc->op_regs  = (xhci_op_regs_t *)  (base + XHCI_REG_RD(hc->cap_regs, XHCI_CAP_LENGTH));
	hc->rt_regs  = (xhci_rt_regs_t *)  (base + XHCI_REG_RD(hc->cap_regs, XHCI_CAP_RTSOFF));
	hc->db_arry  = (xhci_doorbell_t *) (base + XHCI_REG_RD(hc->cap_regs, XHCI_CAP_DBOFF));

	usb_log_debug2("Initialized MMIO reg areas:");
	usb_log_debug2("\tCapability regs: %p", hc->cap_regs);
	usb_log_debug2("\tOperational regs: %p", hc->op_regs);
	usb_log_debug2("\tRuntime regs: %p", hc->rt_regs);
	usb_log_debug2("\tDoorbell array base: %p", hc->db_arry);

	xhci_dump_cap_regs(hc->cap_regs);

	hc->ac64 = XHCI_REG_RD(hc->cap_regs, XHCI_CAP_AC64);
	hc->max_slots = XHCI_REG_RD(hc->cap_regs, XHCI_CAP_MAX_SLOTS);

	hc->dcbaa = malloc32((1 + hc->max_slots) * sizeof(xhci_device_ctx_t));
	if (!hc->dcbaa)
		goto err_hc;

	if ((err = xhci_trb_ring_init(&hc->command_ring, hc)))
		goto err_dcbaa;

	if ((err = xhci_event_ring_init(&hc->event_ring, hc)))
		goto err_cmd_ring;

	// TODO: Allocate scratchpad buffers

	hcd_set_implementation(hcd, hc, &xhci_ddf_hc_driver.ops);

	if ((err = hc_start(hc, irq)))
		goto err_event_ring;

	return EOK;

err_event_ring:
	xhci_event_ring_fini(&hc->event_ring);
err_cmd_ring:
	xhci_trb_ring_fini(&hc->command_ring);
err_dcbaa:
	free32(hc->dcbaa);
err_hc:
	free(hc);
	hcd_set_implementation(hcd, NULL, NULL);
	return err;
}

static int hc_status(hcd_t *hcd, uint32_t *status)
{
	xhci_hc_t *hc = hcd_get_driver_data(hcd);
	assert(hc);
	assert(status);

	*status = 0;
	if (hc->op_regs) {
		*status = XHCI_REG_RD(hc->op_regs, XHCI_OP_STATUS);
		XHCI_REG_WR(hc->op_regs, XHCI_OP_STATUS, *status & XHCI_STATUS_ACK_MASK);
	}
	usb_log_debug2("HC(%p): Read status: %x", hc, *status);
	return EOK;
}

static int ring_doorbell(xhci_hc_t *hc, unsigned doorbell, unsigned target)
{
	uint32_t v = host2xhci(32, target & BIT_RRANGE(uint32_t, 7));
	pio_write_32(&hc->db_arry[doorbell], v);
	return EOK;
}

static int send_no_op_command(xhci_hc_t *hc)
{
	xhci_trb_t trb;
	memset(&trb, 0, sizeof(trb));

	trb.control = host2xhci(32, XHCI_TRB_TYPE_NO_OP_CMD << 10);

	xhci_trb_ring_enqueue(&hc->command_ring, &trb);
	ring_doorbell(hc, 0, 0);

	xhci_dump_trb(&trb);
	usb_log_debug2("HC(%p): Sent TRB", hc);
	return EOK;
}

static int hc_schedule(hcd_t *hcd, usb_transfer_batch_t *batch)
{
	xhci_hc_t *hc = hcd_get_driver_data(hcd);
	assert(hc);

	xhci_dump_state(hc);
	send_no_op_command(hc);
	async_usleep(1000);
	xhci_dump_state(hc);

	xhci_dump_trb(hc->event_ring.dequeue_trb);
	return EOK;
}

static void hc_run_event_ring(xhci_hc_t *hc, xhci_event_ring_t *event_ring, xhci_interrupter_regs_t *intr)
{
	int err;
	xhci_trb_t trb;

	err = xhci_event_ring_dequeue(event_ring, &trb);;

	switch (err) {
		case EOK:
			usb_log_debug2("Dequeued from event ring.");
			xhci_dump_trb(&trb);
			break;

		case ENOENT:
			usb_log_debug2("Event ring finished.");
			break;

		default:
			usb_log_warning("Error while accessing event ring: %s", str_error(err));
	}

	/* Update the ERDP to make room inthe ring */
	uint64_t erstptr = addr_to_phys(hc->event_ring.erst);
	XHCI_REG_WR(intr, XHCI_INTR_ERDP_LO, LOWER32(erstptr));
	XHCI_REG_WR(intr, XHCI_INTR_ERDP_HI, UPPER32(erstptr));
}

static void hc_interrupt(hcd_t *hcd, uint32_t status)
{
	xhci_hc_t *hc = hcd_get_driver_data(hcd);
	assert(hc);

	if (status & XHCI_REG_MASK(XHCI_OP_HSE)) {
		usb_log_error("Host controller error occured. Bad things gonna happen...");
	}

	if (status & XHCI_REG_MASK(XHCI_OP_EINT)) {
		usb_log_debug2("Event interrupt.");

		xhci_interrupter_regs_t *intr0 = &hc->rt_regs->ir[0];

		if (XHCI_REG_RD(intr0, XHCI_INTR_IP)) {
			XHCI_REG_SET(intr0, XHCI_INTR_IP, 1);
			hc_run_event_ring(hc, &hc->event_ring, intr0);
		}
	}

	if (status & XHCI_REG_MASK(XHCI_OP_PCD)) {
		usb_log_error("Port change detected. Not implemented yet!");
	}
	
	if (status & XHCI_REG_MASK(XHCI_OP_SRE)) {
		usb_log_error("Save/Restore error occured. WTF, S/R mechanism not implemented!");
	}
}

static void hc_fini(hcd_t *hcd)
{
	xhci_hc_t *hc = hcd_get_driver_data(hcd);
	assert(hc);

	usb_log_info("Finishing");

	xhci_trb_ring_fini(&hc->command_ring);
	xhci_event_ring_fini(&hc->event_ring);

	free(hc);
	hcd_set_implementation(hcd, NULL, NULL);
}

const ddf_hc_driver_t xhci_ddf_hc_driver = {
	.hc_speed = USB_SPEED_SUPER,
	.irq_code_gen = hc_gen_irq_code,
	.claim = hc_claim,
	.init = hc_init,
	.fini = hc_fini,
	.name = "XHCI-PCI",
	.ops = {
		.schedule       = hc_schedule,
		.irq_hook       = hc_interrupt,
		.status_hook    = hc_status,
	}
};



/**
 * @}
 */
