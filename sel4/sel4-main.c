/**
 * a calculator system program running on sel4
 * @author Tobias Weber
 * @date apr-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * References:
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/mapping/mapping.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/untyped/untyped.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/threads/threads.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/interrupts/interrupts.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/dynamic-2/dynamic-2.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/libsel4tutorials/src/alloc.c
 *   - https://docs.sel4.systems/projects/sel4/api-doc.html
 */

#include "defines.h"
#include "string.h"
#include "shell.h"

#include <sel4/sel4.h>
#include <sel4platsupport/bootinfo.h>


struct Keyboard
{
	seL4_SlotPos keyb_slot;
	seL4_SlotPos irq_slot;
	seL4_SlotPos irq_notify;
};


// some (arbitrary) badge number for the thread endpoint
#define CALCTHREAD_BADGE 1234


/**
 * print slot usage
 * @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/untyped/untyped.md
 */
void print_slots(seL4_SlotPos start, seL4_SlotPos end, const seL4_UntypedDesc* list)
{
	printf("\nUntyped capability slots:\n");
	printf("Slot       Size             Physical Address      Device\n");

	for(seL4_SlotPos cur_slot=start; cur_slot<end; ++cur_slot)
	{
		const seL4_UntypedDesc *descr = list + (cur_slot-start);

		word_t size = (1 << descr->sizeBits);
		word_t addr_start = descr->paddr;
		word_t addr_end = addr_start + size;
		u8 is_dev = descr->isDevice;

		printf("0x%-8lx %-16ld 0x%016lx %4d\n",
			cur_slot, size, addr_start, is_dev);
	}

	printf("\n");
}


/**
 * find a free untyped slot
 * @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/untyped/untyped.md
 */
seL4_SlotPos find_untyped(seL4_SlotPos untyped_start, seL4_SlotPos untyped_end,
	const seL4_UntypedDesc* untyped_list, word_t needed_size)
{
	for(seL4_SlotPos cur_slot=untyped_start; cur_slot<untyped_end; ++cur_slot)
	{
		const seL4_UntypedDesc *cur_descr = untyped_list + (cur_slot-untyped_start);
		if(cur_descr->isDevice)
			continue;

		word_t cur_size = (1 << cur_descr->sizeBits);
		if(cur_size < needed_size)
			continue;

		return cur_slot;
	}

	return 0;
}


/**
 * find the capability slot for a device memory region which has the given address
 * @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/untyped/untyped.md
 */
seL4_SlotPos find_devicemem(seL4_SlotPos untyped_start, seL4_SlotPos untyped_end,
	const seL4_UntypedDesc* untyped_list, word_t addr)
{
	for(seL4_SlotPos cur_slot=untyped_start; cur_slot<untyped_end; ++cur_slot)
	{
		const seL4_UntypedDesc *cur_descr = untyped_list + (cur_slot-untyped_start);
		if(!cur_descr->isDevice)
			continue;

		word_t size = (1 << cur_descr->sizeBits);
		word_t addr_start = cur_descr->paddr;
		word_t addr_end = addr_start + size;

		if(addr >= addr_start && addr < addr_end)
			return cur_slot;
	}

	return 0;
}


/**
 * map page directory pointer table, page directory and page table
 * @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/mapping/mapping.md
 */
void map_pagetables(seL4_SlotPos untyped_start, seL4_SlotPos untyped_end,
	const seL4_UntypedDesc* untyped_list, seL4_SlotPos* cur_slot,
	word_t virt_addr)
{
	const seL4_SlotPos cnode = seL4_CapInitThreadCNode;
	const seL4_SlotPos vspace = seL4_CapInitThreadVSpace;
	seL4_X86_VMAttributes vmattr = seL4_X86_Default_VMAttributes;

	seL4_SlotPos table_slot = find_untyped(untyped_start, untyped_end,
		untyped_list, PAGE_SIZE*1024);
	if(table_slot < untyped_start)
		printf("Error: No large enough untyped slot found!\n");
	printf("Loading tables into untyped slot 0x%lx.\n", table_slot);

	// load three levels of page tables
	const seL4_ArchObjectType pagetable_objs[] =
	{
		seL4_X86_PDPTObject,
		seL4_X86_PageDirectoryObject,
		seL4_X86_PageTableObject
	};

	seL4_Error (*pagetable_map[])(word_t, seL4_CPtr, word_t, seL4_X86_VMAttributes) =
	{
		&seL4_X86_PDPT_Map,
		&seL4_X86_PageDirectory_Map,
		&seL4_X86_PageTable_Map
	};

	seL4_SlotPos pagetable_slots[] = { 0, 0, 0 };

	for(i16 level=0; level<sizeof(pagetable_objs)/sizeof(pagetable_objs[0]); ++level)
	{
		pagetable_slots[level] = (*cur_slot)++;
		seL4_Untyped_Retype(table_slot, pagetable_objs[level], 0, cnode,
			0, 0, pagetable_slots[level], 1);
		if((*pagetable_map[level])(pagetable_slots[level],
			vspace, virt_addr, vmattr) != seL4_NoError)
		{
			printf("Error mapping page table level %d!\n", level);
			break;
		}
	}
}


/**
 * map a page into a given virtual address
 * @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/mapping/mapping.md
 */
seL4_SlotPos map_page(seL4_SlotPos untyped_start, seL4_SlotPos untyped_end,
	const seL4_UntypedDesc* untyped_list, seL4_SlotPos* cur_slot,
	word_t virt_addr)
{
	const seL4_SlotPos cnode = seL4_CapInitThreadCNode;
	const seL4_SlotPos vspace = seL4_CapInitThreadVSpace;
	seL4_X86_VMAttributes vmattr = seL4_X86_Default_VMAttributes;

	seL4_SlotPos base_slot = find_untyped(untyped_start, untyped_end, untyped_list, PAGE_SIZE);
	printf("Using device memory slot 0x%lx.\n", base_slot);

	seL4_SlotPos page_slot = (*cur_slot)++;
	seL4_Untyped_Retype(base_slot, PAGE_TYPE, 0, cnode, 0, 0, page_slot, 1);

	if(seL4_X86_Page_Map(page_slot, vspace, virt_addr, seL4_AllRights, vmattr)
		!= seL4_NoError)
	{
		printf("Error mapping page!\n");
	}

	seL4_X86_Page_GetAddress_t addr_info = seL4_X86_Page_GetAddress(page_slot);
	printf("Mapped virtual address: 0x%lx -> physical address: 0x%lx.\n",
		virt_addr, addr_info.paddr);

	return page_slot;
}


/**
 * map a given physical address into a given virtual address
 * @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/mapping/mapping.md
 */
seL4_SlotPos map_page_phys(seL4_SlotPos untyped_start, seL4_SlotPos untyped_end,
	const seL4_UntypedDesc* untyped_list, seL4_SlotPos* cur_slot,
	word_t virt_addr, word_t phys_addr)
{
	const seL4_SlotPos cnode = seL4_CapInitThreadCNode;
	const seL4_SlotPos vspace = seL4_CapInitThreadVSpace;
	seL4_X86_VMAttributes vmattr = seL4_X86_Default_VMAttributes;

	seL4_SlotPos base_slot = find_devicemem(untyped_start, untyped_end,
		untyped_list, (word_t)phys_addr);
	printf("Using device memory slot 0x%lx.\n", base_slot);

	seL4_SlotPos page_slot = 0;
	// iterate all page frames till we're at the correct one
	// TODO: find more efficient way to specify an offset
	for(word_t i=0; i<=phys_addr/PAGE_SIZE; ++i)
	{
		seL4_SlotPos frame_slot = (*cur_slot)++;
		seL4_Untyped_Retype(base_slot, PAGE_TYPE, 0, cnode, 0, 0, frame_slot, 1);
		page_slot = frame_slot;
	}
	//seL4_SlotPos frame_slot = (*cur_slot)++;
	//seL4_Untyped_Retype(base_slot, seL4_X86_LargePageObject, 0, cnode, 0, 0, frame_slot, 1);

	if(seL4_X86_Page_Map(page_slot, vspace, virt_addr, seL4_ReadWrite, vmattr)
		!= seL4_NoError)
	{
		printf("Error mapping page!\n");
	}

	seL4_X86_Page_GetAddress_t addr_info = seL4_X86_Page_GetAddress(page_slot);
	printf("Mapped virtual address: 0x%lx -> physical address: 0x%lx.\n",
		virt_addr, addr_info.paddr);

	return page_slot;
}


/**
 * finds a free capability slot and retypes it
 * @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/untyped/untyped.md
 */
seL4_SlotPos get_slot(word_t obj, word_t obj_size,
	seL4_SlotPos untyped_start, seL4_SlotPos untyped_end, const seL4_UntypedDesc* untyped_list,
	seL4_SlotPos* cur_slot, seL4_SlotPos cnode)
{
	seL4_SlotPos slot = find_untyped(untyped_start, untyped_end, untyped_list, obj_size);
	seL4_SlotPos offs = (*cur_slot)++;
	seL4_Untyped_Retype(slot, obj, 0, cnode, 0, 0, offs, 1);

	return offs;
}


i64 main()
{
	printf("--------------------------------------------------------------------------------\n");

	// ------------------------------------------------------------------------
	// initial thread and boot infos
	// ------------------------------------------------------------------------
	const seL4_SlotPos this_cnode = seL4_CapInitThreadCNode;
	const seL4_SlotPos this_vspace = seL4_CapInitThreadVSpace;
	const seL4_SlotPos this_tcb = seL4_CapInitThreadTCB;
	const seL4_SlotPos this_ipcbuf = seL4_CapInitThreadIPCBuffer;
	const seL4_SlotPos this_irqctrl = seL4_CapIRQControl;
	const seL4_SlotPos this_ioctrl = seL4_CapIOPortControl;

	const seL4_BootInfo *bootinfo = platsupport_get_bootinfo();
	const seL4_IPCBuffer *this_ipcbuffer = bootinfo->ipcBuffer;

	const seL4_SlotRegion *empty_region = &bootinfo->empty;
	const seL4_SlotPos empty_start = empty_region->start;
	const seL4_SlotPos empty_end = empty_region->end;
	printf("Empty CNodes in region: [%ld .. %ld[.\n", empty_start, empty_end);

	const seL4_UntypedDesc* untyped_list = bootinfo->untypedList;
	const seL4_SlotRegion *untyped_region = &bootinfo->untyped;
	const seL4_SlotPos untyped_start = untyped_region->start;
	const seL4_SlotPos untyped_end = untyped_region->end;
	printf("Untyped CNodes in region: [%ld .. %ld[.\n", untyped_start, untyped_end);

	seL4_SlotPos cur_slot = empty_start;
	seL4_SlotPos cur_untyped_slot = untyped_start;

#if SERIAL_DEBUG != 0
	print_slots(untyped_start, untyped_end, untyped_list);
#endif
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// (arbitrary) virtual addresses to map page tables, video ram and the TCB stack into
	// ------------------------------------------------------------------------
	word_t virt_addr_tables = 0x8000000000;
	word_t virt_addr_char = 0x8000001000;
	word_t virt_addr_tcb_stack = 0x8000002000;
	word_t virt_addr_tcb_tls = 0x8000003000;
	word_t virt_addr_tcb_ipcbuf = 0x8000004000;
	word_t virt_addr_tcb_tlsipc = virt_addr_tcb_tls + 0x10;

	// map the page tables
	map_pagetables(untyped_start, untyped_end, untyped_list, &cur_slot, virt_addr_tables);

	// find page whose frame contains the vga memory
	seL4_SlotPos page_slot = map_page_phys(untyped_start, untyped_end,
		untyped_list, &cur_slot, virt_addr_char, CHAROUT_PHYS);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// start shell thread
	// @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/threads/threads.md
	// ------------------------------------------------------------------------
	// create a page frame for the thread's stack
	seL4_SlotPos page_slot_tcb_stack = map_page(untyped_start, untyped_end,
		untyped_list, &cur_slot, virt_addr_tcb_stack);

	seL4_SlotPos page_slot_tcb_tls = map_page(untyped_start, untyped_end,
		untyped_list, &cur_slot, virt_addr_tcb_tls);

	seL4_SlotPos page_slot_tcb_ipcbuf = map_page(untyped_start, untyped_end,
		untyped_list, &cur_slot, virt_addr_tcb_ipcbuf);

	seL4_SlotPos tcb = get_slot(seL4_TCBObject, 1<<seL4_TCBBits,
		untyped_start, untyped_end, untyped_list, &cur_slot, this_cnode);

	// the child thread uses the main thread's cnode and vspace
	if(seL4_TCB_SetSpace(tcb, 0, this_cnode, 0, this_vspace, 0) != seL4_NoError)
		printf("Error: Cannot set TCB space!\n");

	// set up thread local storage
	if(seL4_TCB_SetTLSBase(tcb, virt_addr_tcb_tlsipc) != seL4_NoError)
		printf("Error: Cannot set TCB IPC buffer!\n");

	// set up the ipc buffer
	if(seL4_TCB_SetIPCBuffer(tcb, virt_addr_tcb_ipcbuf, page_slot_tcb_ipcbuf) != seL4_NoError)
		printf("Error: Cannot set TCB IPC buffer!\n");

	*(const seL4_IPCBuffer**)virt_addr_tcb_tls = this_ipcbuffer /*__sel4_ipc_buffer*/;

	// doesn't seem to get scheduled otherwise...
	if(seL4_TCB_SetPriority(tcb, this_tcb, seL4_MaxPrio) != seL4_NoError)
		printf("Error: Cannot set TCB priority!\n");

	// create semaphores for thread signalling
	seL4_SlotPos tcb_startnotify = get_slot(seL4_NotificationObject, 1<<seL4_NotificationBits,
		untyped_start, untyped_end, untyped_list, &cur_slot, this_cnode);
	seL4_SlotPos tcb_endpoint = get_slot(seL4_EndpointObject, 1<<seL4_EndpointBits,
		untyped_start, untyped_end, untyped_list, &cur_slot, this_cnode);
	seL4_TCB_BindNotification(this_tcb, tcb_startnotify);

	// get badged versions of these objects
	word_t tcb_badge = CALCTHREAD_BADGE;
	seL4_SlotPos tcb_startnotify2 = cur_slot++;
	if(seL4_CNode_Mint(this_cnode, tcb_startnotify2, seL4_WordBits, this_cnode,
		tcb_startnotify, seL4_WordBits, seL4_AllRights, tcb_badge) != seL4_NoError)
		printf("Error: Minting of start notifier failed.");

	seL4_SlotPos tcb_endpoint2 = cur_slot++;
	if(seL4_CNode_Mint(this_cnode, tcb_endpoint2, seL4_WordBits, this_cnode,
		tcb_endpoint, seL4_WordBits, seL4_AllRights, tcb_badge) != seL4_NoError)
		printf("Error: Minting of thread endpoint failed.");

	seL4_UserContext tcb_context;
	i32 num_regs = sizeof(tcb_context)/sizeof(tcb_context.rax);
	seL4_TCB_ReadRegisters(tcb, 0, 0, num_regs, &tcb_context);

	// pass instruction pointer, stack pointer and arguments in registers
	// according to sysv calling convention
	tcb_context.rip = (word_t)&run_calc_shell;  // entry point
	tcb_context.rsp = (word_t)(virt_addr_tcb_stack + PAGE_SIZE);  // stack
	tcb_context.rbp = (word_t)(virt_addr_tcb_stack + PAGE_SIZE);  // stack
	tcb_context.rdi = (word_t)tcb_startnotify2; // arg 1: start notification
	tcb_context.rsi = (word_t)virt_addr_char;   // arg 2: vga ram
	tcb_context.rdx = (word_t)tcb_endpoint;     // arg 3: ipc endpoint

	printf("rip = 0x%lx, rsp = 0x%lx, rflags = 0x%lx, rdi = 0x%lx, rsi = 0x%lx, rdx = 0x%lx.\n",
		tcb_context.rip, tcb_context.rsp, tcb_context.rflags,
		tcb_context.rdi, tcb_context.rsi, tcb_context.rdx);

	// write registers and start thread
	if(seL4_TCB_WriteRegisters(tcb, 1, 0, num_regs, &tcb_context) != seL4_NoError)
		printf("Error writing TCB registers!\n");

	printf("Waiting for thread to start...\n");
	word_t start_badge;
	seL4_Wait(tcb_startnotify, &start_badge);
	printf("Thread started, badge: %ld.\n", start_badge);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// keyboard interrupt service routine
	// @see https://github.com/seL4/sel4-tutorials/blob/master/tutorials/interrupts/interrupts.md
	// ------------------------------------------------------------------------
	struct Keyboard keyb;

	// keyboard interrupt
	keyb.keyb_slot = cur_slot++;
	if(seL4_X86_IOPortControl_Issue(this_ioctrl, KEYB_DATA_PORT, KEYB_DATA_PORT,
		this_cnode, keyb.keyb_slot, seL4_WordBits) != seL4_NoError)
		printf("Error getting keyboard IO control!\n");

	keyb.irq_slot = cur_slot++;
	//seL4_IRQControl_Get(this_irqctrl, KEYB_IRQ, this_cnode, keyb.irq_slot, seL4_WordBits);
	if(seL4_IRQControl_GetIOAPIC(this_irqctrl, this_cnode, keyb.irq_slot,
		seL4_WordBits, KEYB_PIC, KEYB_IRQ, 0, 1, KEYB_INT) != seL4_NoError)
		printf("Error getting keyboard interrupt control!\n");

	keyb.irq_notify = get_slot(seL4_NotificationObject, 1<<seL4_NotificationBits,
		untyped_start, untyped_end, untyped_list, &cur_slot, this_cnode);
	if(seL4_IRQHandler_SetNotification(keyb.irq_slot, keyb.irq_notify) != seL4_NoError)
		printf("Error setting keyboard interrupt notification!\n");

	while(1)
	{
		seL4_Wait(keyb.irq_notify, 0);
		seL4_X86_IOPort_In8_t key = seL4_X86_IOPort_In8(keyb.keyb_slot, KEYB_DATA_PORT);

		if(key.error != seL4_NoError)
		{
			printf("Error reading keyboard port!\n");
		}
		else
		{
			printf("Key code: 0x%x.\n", key.result);
			seL4_IRQHandler_Ack(keyb.irq_slot);

			// save the key code in the message register
			// and send it to the thread
			seL4_SetMR(0, key.result);
			seL4_Call(tcb_endpoint2, seL4_MessageInfo_new(0,0,0,1));
		}
	}
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// end program
	seL4_TCB_Suspend(tcb);
	seL4_CNode_Revoke(this_cnode, page_slot_tcb_stack, seL4_WordBits);
	seL4_CNode_Revoke(this_cnode, page_slot, seL4_WordBits);

	printf("--------------------------------------------------------------------------------\n");
	printf("Main thread has ended.\n");
	while(1) seL4_Yield();
	// ------------------------------------------------------------------------

	return 0;
}
