/* Linker script to configure memory regions. */
MEMORY
{
	FLASH (rx)	: ORIGIN = 0x08000000, LENGTH = 256K
	SRAM1 (xrw)	: ORIGIN = 0x20000000, LENGTH = 64K

}

/* Library configurations */
GROUP(libgcc.a libc.a libm.a libnosys.a)

/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions FLASH and RAM.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __exidx_start
 *   __exidx_end
 *   __copy_table_start__
 *   __copy_table_end__
 *   __zero_table_start__
 *   __zero_table_end__
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 *   __Vectors_End
 *   __Vectors_Size
 */
ENTRY(Reset_Handler)

SECTIONS
{
	.text :
	{
		KEEP(*(.vectors))
		__Vectors_End = .;
		__Vectors_Size = __Vectors_End - __Vectors;
		__end__ = .;

		*(.text*)

		KEEP(*(.init))
		KEEP(*(.fini))


		*(.rodata*)

		KEEP(*(.eh_frame*))
	} > FLASH

	__etext = .;

	.data : AT (__etext)
	{
		__data_start__ = .;
		*(vtable)
		*(.data*)

		/* All data end */
		__data_end__ = .;

	} > SRAM1

	.bss :
	{
		. = ALIGN(4);
		__bss_start__ = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end__ = .;
	} > SRAM1

	.heap (COPY):
	{
		__HeapBase = .;
		__end__ = .;
		end = __end__;
		KEEP(*(.heap*))
		__HeapLimit = .;
	} > SRAM1

	/* .stack_dummy section doesn't contains any symbols. It is only
	 * used for linker to calculate size of stack sections, and assign
	 * values to stack symbols later */
	.stack_dummy (COPY):
	{
		KEEP(*(.stack*))
	} > SRAM1

	/* Set stack top to end of RAM, and stack limit move down by
	 * size of stack_dummy section */
	__StackTop = ORIGIN(SRAM1) + LENGTH(SRAM1);
	__StackLimit = __StackTop - SIZEOF(.stack_dummy);
	PROVIDE(__stack = __StackTop);

	/* Check if data + heap + stack exceeds RAM limit */
	ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
	
}
