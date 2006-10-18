/*
 * Copyright (C) 2005 Ondrej Palkovsky
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

/** @addtogroup mips32	
 * @{
 */
/** @file
 */

#include <arch/drivers/arc.h>
#include <arch/mm/page.h>
#include <print.h>
#include <arch.h>
#include <arch/byteorder.h>
#include <arch/mm/frame.h>
#include <mm/frame.h>
#include <interrupt.h>
#include <align.h>
#include <console/console.h>
#include <console/kconsole.h>
#include <console/cmd.h>
#include <mm/slab.h>

/* This is a good joke, SGI HAS different types than NT bioses... */
/* Here is the SGI type */
static char *basetypes[] = {
	"ExceptionBlock",
	"SystemParameterBlock",
	"FreeContiguous",
	"FreeMemory",
	"BadMemory",
	"LoadedProgram",
	"FirmwareTemporary",
	"FirmwarePermanent"
};

static char *ctypes[] = {
	"ARC_type",
	"CPU_type",
	"FPU_type",
	"PrimaryICache",
	"PrimaryDCache",
	"SecondaryICache",
	"SecondaryDCache",
	"SecondaryCache",
	"Memory",
	"EISAAdapter",
	"TCAdapter",
	"SCSIAdapter",
	"DTIAdapter",
	"MultiFunctionAdapter",
	"DiskController",
	"TapeController",
	"CDROMController",
	"WORMController",
	"SerialController",
	"NetworkController",
	"DisplayController",
	"ParallelController",
	"PointerController",
	"KeyboardController",
	"AudioController",
	"OtherController",
	"DiskPeripheral",
	"FloppyDiskPeripheral",
	"TapePeripheral",
	"ModemPeripheral",
	"MonitorPeripheral",
	"PrinterPeripheral",
	"PointerPeripheral",
	"KeyboardPeripheral",
	"TerminalPeripheral",
	"OtherPeripheral",
	"LinePeripheral",
	"NetworkPeripheral"
	"OtherPeripheral",
	"XTalkAdapter",
	"PCIAdapter",
	"GIOAdapter",
	"TPUAdapter",
	"Anonymous"
};

static arc_sbp *sbp = (arc_sbp *)PA2KA(0x1000);
static arc_func_vector_t *arc_entry; 


static void arc_putchar(char ch);

/** Return true if ARC is available */
int arc_enabled(void)
{
	return sbp != NULL;
}


/** Print configuration data that ARC reports about component */
static void arc_print_confdata(arc_component *c)
{
	cm_resource_list *configdata;
	int i;

	if (!c->configdatasize)
		return; /* No configuration data */

	configdata = malloc(c->configdatasize, 0);

	if (arc_entry->getconfigurationdata(configdata, c)) {
		free(configdata);
		return;
	}
	/* Does not seem to return meaningful data, don't use now */
	free(configdata);
	return;
	
	for (i=0; i < configdata->count; i++) {
		switch (configdata->descr[i].type) {
		case CmResourceTypePort:
			printf("Port: %p-size:%d ",
			       (uintptr_t)configdata->descr[i].u.port.start,
			       configdata->descr[i].u.port.length);
			break;
		case CmResourceTypeInterrupt:
			printf("Irq: level(%d) vector(%d) ",
			       configdata->descr[i].u.interrupt.level,
			       configdata->descr[i].u.interrupt.vector);
			break;
		case CmResourceTypeMemory:
			printf("Memory: %p-size:%d ",
			       (uintptr_t)configdata->descr[i].u.port.start,
			       configdata->descr[i].u.port.length);
			break;
		default:
			break;
		}
	}

	free(configdata);
}

/** Print information about component */
static void arc_print_component(arc_component *c)
{
	int i;

	printf("%s: ",ctypes[c->type]);
	for (i=0;i < c->identifier_len;i++)
		printf("%c",c->identifier[i]);

	printf(" ");
	arc_print_confdata(c);
	printf("\n");
}

/**
 * Read from ARC bios configuration data and print it
 */
static int cmd_arc_print_devices(cmd_arg_t *argv)
{
	arc_component *c,*next;

	c = arc_entry->getchild(NULL);
	while (c) {
		arc_print_component(c);
		next = arc_entry->getchild(c);
		while (!next) {
			next = arc_entry->getpeer(c);
			if (!next)
				c = arc_entry->getparent(c);
			if (!c)
				return 0;
		}
		c = next;
	}
	return 1;
}
static cmd_info_t devlist_info = {
	.name = "arcdevlist",
	.description = "Print arc device list",
	.func = cmd_arc_print_devices,
	.argc = 0
};


/** Read from arc bios memory map and print it
 *
 */
static int cmd_arc_print_memmap(cmd_arg_t *argv)
{
	arc_memdescriptor_t *desc;

	printf("Memory map:\n");

	desc = arc_entry->getmemorydescriptor(NULL);
	while (desc) {
		printf("%s: %d(%p) (size: %dKB)\n",basetypes[desc->type],
		       desc->basepage * ARC_FRAME,
		       desc->basepage * ARC_FRAME,
		       desc->basecount*ARC_FRAME/1024);
		desc = arc_entry->getmemorydescriptor(desc);
	}
	return 1;
}
static cmd_info_t memmap_info = {
	.name = "arcmemmap",
	.description = "Print arc memory map",
	.func = cmd_arc_print_memmap,
	.argc = 0
};

/** Print charactor to console */
static void arc_putchar(char ch)
{
	uint32_t cnt;
	ipl_t ipl;

	/* TODO: Should be spinlock? */
	ipl = interrupts_disable();
	arc_entry->write(1, &ch, 1, &cnt);
	interrupts_restore(ipl);
	
}

static int cmd_reboot(cmd_arg_t *argv)
{
	arc_entry->reboot();
	return 0;
}
static cmd_info_t reboot_info = {
	.name = "reboot",
	.description = "Reboot computer",
	.func = cmd_reboot,
	.argc = 0
};

/** Initialize ARC structure
 *
 * @return 0 - ARC OK, -1 - ARC does not exist
 */
int arc_init(void)
{
	if (sbp->signature != ARC_MAGIC) {
		sbp = NULL;
		return -1;
	}
	arc_entry = sbp->firmwarevector;

	arc_putchar('A');
	arc_putchar('R');
	arc_putchar('C');
	arc_putchar('\n');

	/* Add command for resetting the computer */
	cmd_initialize(&reboot_info);
	cmd_register(&reboot_info);
	cmd_initialize(&memmap_info);
	cmd_register(&memmap_info);
	cmd_initialize(&devlist_info);
	cmd_register(&devlist_info);

	return 0;
}

static bool kbd_polling_enabled;
static chardev_t console;

/** Try to get character, return character or -1 if not available */
static void arc_keyboard_poll(void)
{
	char ch;
	uint32_t count;
	long result;
	
	if (! kbd_polling_enabled)
		return;

	if (arc_entry->getreadstatus(0))
		return;
	result = arc_entry->read(0, &ch, 1, &count);
	if (result || count!=1) {
		return;
	}
	if (ch == '\r')
		ch = '\n';
	if (ch == 0x7f)
		ch = '\b';
	
	chardev_push_character(&console, ch);
}

static char arc_read(chardev_t *dev)
{
	char ch;
	uint32_t count;
	long result;

	result = arc_entry->read(0, &ch, 1, &count);
	if (result || count!=1) {
		printf("Error reading from ARC keyboard.\n");
		cpu_halt();
	}
	if (ch == '\r')
		return '\n';
	if (ch == 0x7f)
		return '\b';
	return ch;
}

static void arc_write(chardev_t *dev, const char ch)
{
	arc_putchar(ch);
}

static void arc_enable(chardev_t *dev)
{
	kbd_polling_enabled = true;
}

static void arc_disable(chardev_t *dev)
{
	kbd_polling_enabled = false;
}

static chardev_operations_t arc_ops = {
	.resume = arc_enable,
	.suspend = arc_disable,
	.write = arc_write,
	.read = arc_read
};

void arc_console(void)
{
	kbd_polling_enabled = true;
	
	chardev_initialize("arc_console", &console, &arc_ops);
	timer_fnc = &arc_keyboard_poll;
	stdin = &console;
	stdout = &console;
}

/* Initialize frame zones from ARC firmware. 
 * In the future we may use even the FirmwareTemporary regions,
 * currently we use the FreeMemory (what about the LoadedProgram?)
 */
void arc_frame_init(void)
{
	arc_memdescriptor_t *desc;
	int total = 0;
	uintptr_t base;
	size_t basesize;

	desc = arc_entry->getmemorydescriptor(NULL);
	while (desc) {
		if (desc->type == FreeMemory ||
		    desc->type == FreeContiguous) {
			base = desc->basepage*ARC_FRAME;
			basesize = desc->basecount*ARC_FRAME;

			if (base % FRAME_SIZE ) {
				basesize -= FRAME_SIZE - (base % FRAME_SIZE);
				base = ALIGN_UP(base, FRAME_SIZE);
			}
			basesize = ALIGN_DOWN(basesize, FRAME_SIZE);

			total += basesize;
			
			zone_create(ADDR2PFN(base), SIZE2FRAMES(basesize),
				    ADDR2PFN(base), 0);
		}
		desc = arc_entry->getmemorydescriptor(desc);
	}

	config.memory_size = total;
}

/** @}
 */
