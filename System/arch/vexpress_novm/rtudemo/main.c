/***********************************************************************************/
/* Copyright (c) 2013, Wictor Lund. All rights reserved.			   */
/* Copyright (c) 2013, Åbo Akademi University. All rights reserved.		   */
/* 										   */
/* Redistribution and use in source and binary forms, with or without		   */
/* modification, are permitted provided that the following conditions are met:	   */
/*      * Redistributions of source code must retain the above copyright	   */
/*        notice, this list of conditions and the following disclaimer.		   */
/*      * Redistributions in binary form must reproduce the above copyright	   */
/*        notice, this list of conditions and the following disclaimer in the	   */
/*        documentation and/or other materials provided with the distribution.	   */
/*      * Neither the name of the Åbo Akademi University nor the		   */
/*        names of its contributors may be used to endorse or promote products	   */
/*        derived from this software without specific prior written permission.	   */
/* 										   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND */
/* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED   */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE	   */
/* DISCLAIMED. IN NO EVENT SHALL ÅBO AKADEMI UNIVERSITY BE LIABLE FOR ANY	   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES	   */
/* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;	   */
/* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND	   */
/* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT	   */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS   */
/* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 		   */
/***********************************************************************************/

#include <FreeRTOS.h>

#define MAIN MAIN
#define SYSTEM_MODULE MAIN

#include <task.h>
#include <queue.h>

#include <System/types.h>
#include <System/linker.h>
#include <System/applications.h>
#include <System/migrator.h>
#include <System/system.h>
#include <System/task_manager.h>
#include <System/pl011.h>
#include <System/elf.h>

#include <stdarg.h>

#define ADD_CARRIGE_RETURN

static int putchar_uart(int c)
{
	xUARTSendCharacter(0,c,0);
	xUARTSendCharacter(1,c,0);
	xUARTSendCharacter(2,c,0);
	xUARTSendCharacter(3,c,0);
#ifdef ADD_CARRIGE_RETURN
	if (c == '\n')
		xUARTSendCharacter(portCORE_ID(),'\r',0);
#endif
	return 0;
}

static void printchar_uart(char **str, int c)
{
//	extern int putchar(int c);
	
	if (str) {
		**str = c;
		++(*str);
	}
	else putchar_uart(c);
}

#define PAD_RIGHT 1
#define PAD_ZERO 2

static int prints_uart(char **out, const char *string, int width, int pad)
{
	register int pc = 0, padchar = ' ';

	if (width > 0) {
		register int len = 0;
		register const char *ptr;
		for (ptr = string; *ptr; ++ptr) ++len;
		if (len >= width) width = 0;
		else width -= len;
		if (pad & PAD_ZERO) padchar = '0';
	}
	if (!(pad & PAD_RIGHT)) {
		for ( ; width > 0; --width) {
			printchar_uart (out, padchar);
			++pc;
		}
	}
	for ( ; *string ; ++string) {
		printchar_uart (out, *string);
		++pc;
	}
	for ( ; width > 0; --width) {
		printchar_uart (out, padchar);
		++pc;
	}

	return pc;
}

/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 12

static int printi_uart(char **out, int i, int b, int sg, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	register char *s;
	register int t, neg = 0, pc = 0;
	register unsigned int u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints_uart (out, print_buf, width, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN-1;
	*s = '\0';

	while (u) {
		t = u % b;
		if( t >= 10 )
			t += letbase - '0' - 10;
		*--s = t + '0';
		u /= b;
	}

	if (neg) {
		if( width && (pad & PAD_ZERO) ) {
			printchar_uart(out, '-');
			++pc;
			--width;
		}
		else {
			*--s = '-';
		}
	}

	return pc + prints_uart(out, s, width, pad);
}

static int print_uart(char **out, const char *format, va_list args )
{
	register int width, pad;
	register int pc = 0;
	char scr[2];

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = pad = 0;
			if (*format == '\0') break;
			if (*format == '%') goto out;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			for ( ; *format >= '0' && *format <= '9'; ++format) {
				width *= 10;
				width += *format - '0';
			}
			if (*format == 'l') ++format;
			if( *format == 's' ) {
				register char *s = (char *)va_arg( args, int );
				pc += prints_uart (out, s?s:"(null)", width, pad);
				continue;
			}
			if( *format == 'd' || *format == 'i' ) {
				pc += printi_uart (out, va_arg( args, int ), 10, 1, width, pad, 'a');
				continue;
			}
			if( *format == 'x' ) {
				pc += printi_uart (out, va_arg( args, int ), 16, 0, width, pad, 'a');
				continue;
			}
			if( *format == 'X' ) {
				pc += printi_uart (out, va_arg( args, int ), 16, 0, width, pad, 'A');
				continue;
			}
			if( *format == 'u' ) {
				pc += printi_uart (out, va_arg( args, int ), 10, 0, width, pad, 'a');
				continue;
			}
			if( *format == 'c' ) {
				/* char are converted to int then pushed on the stack */
				scr[0] = (char)va_arg( args, int );
				scr[1] = '\0';
				pc += prints_uart (out, scr, width, pad);
				continue;
			}
		}
		else {
		out:
			printchar_uart (out, *format);
			++pc;
		}
	}
	if (out) **out = '\0';
	va_end( args );
	return pc;
}

int puts_uart(const char *s)
{
	while (*s != '\0')
		putchar_uart(*s++);
	return 0;
}

int printf_uart(const char *format, ...)
{
        va_list args;
        
        va_start( args, format );
        return print_uart( 0, format, args );
}

#if 0
int sprintf_uart(char *out, const char *format, ...)
{
        va_list args;
        
        va_start( args, format );
        return print( &out, format, args );
}
#endif
#ifdef TEST_PRINTF
int main(void)
{
	char *ptr = "Hello world!";
	char *np = 0;
	int i = 5;
	unsigned int bs = sizeof(int)*8;
	int mi;
	char buf[80];

	mi = (1 << (bs-1)) + 1;
	printf_uart("%s\n", ptr);
	printf_uart("printf test\n");
	printf("%s is null pointer\n", np);
	printf("%d = 5\n", i);
	printf("%d = - max int\n", mi);
	printf("char %c = 'a'\n", 'a');
	printf("hex %x = ff\n", 0xff);
	printf("hex %02x = 00\n", 0);
	printf("signed %d = unsigned %u = hex %x\n", -3, -3, -3);
	printf("%d %s(s)%", 0, "message");
	printf("\n");
	printf("%d %s(s) with %%\n", 0, "message");
	sprintf_uart(buf, "justif: \"%-10s\"\n", "left"); printf("%s", buf);
	sprintf_uart(buf, "justif: \"%10s\"\n", "right"); printf("%s", buf);
	sprintf_uart(buf, " 3: %04d zero padded\n", 3); printf("%s", buf);
	sprintf_uart(buf, " 3: %-4d left justif.\n", 3); printf("%s", buf);
	sprintf_uart(buf, " 3: %4d right justif.\n", 3); printf("%s", buf);
	sprintf_uart(buf, "-3: %04d zero padded\n", -3); printf("%s", buf);
	sprintf_uart(buf, "-3: %-4d left justif.\n", -3); printf("%s", buf);
	sprintf_uart(buf, "-3: %4d right justif.\n", -3); printf("%s", buf);

	return 0;
}

/*
 * if you compile this file with
 *   gcc -Wall $(YOUR_C_OPTIONS) -DTEST_PRINTF -c printf.c
 * you will get a normal warning:
 *   printf.c:214: warning: spurious trailing `%' in format
 * this line is testing an invalid % at the end of the format string.
 *
 * this should display (on 32bit int machine) :
 *
 * Hello world!
 * printf test
gt * (null) is null pointer
 * 5 = 5
 * -2147483647 = - max int
 * char a = 'a'
 * hex ff = ff
 * hex 00 = 00
 * signed -3 = unsigned 4294967293 = hex fffffffd
 * 0 message(s)
 * 0 message(s) with %
 * justif: "left      "
 * justif: "     right"
 *  3: 0003 zero padded
 *  3: 3    left justif.
 *  3:    3 right justif.
 * -3: -003 zero padded
 * -3: -3   left justif.
 * -3:   -3 right justif.
 */

#endif
# if 0
/*lint +e956 */
    [VE_UART0] = 0x1c090000,
    [VE_UART1] = 0x1c0a0000,
    [VE_UART2] = 0x1c0b0000,
    [VE_UART3] = 0x1c0c0000,


/* UART */
static volatile unsigned int * const UART0DR = (unsigned int *) UARTDR;
static volatile unsigned int * const UART0FR = (unsigned int *) UARTFR;
static void uart_putc(const char c)
{
	// Wait for UART to become ready to transmit.
	while ((*UART0FR) & (1 << 5)) { }
	*UART0DR = c; /* Transmit char */
}
/*
static void uart_puthex(uint64_t n)
{
	const char *hexdigits = "0123456789ABCDEF";

	uart_putc('0');
	uart_putc('x');
	for (int i = 60; i >= 0; i -= 4){
		uart_putc(hexdigits[(n >> i) & 0xf]);
		if (i == 32)
			uart_putc(' ');
	}
}
*/
static void uart_puts(const char *s) {
	for (int i = 0; s[i] != '\0'; i ++)
		uart_putc((unsigned char)s[i]);
}
#endif

int migrator_loop()
{
	task_register_cons *trc;

	while (1) {
		vTaskDelay(1000/portTICK_RATE_MS);

		if ((trc = task_find("rtuapp"))) {
			if (!task_wait_for_checkpoint(trc, cp_req_rtu)) {
				ERROR_MSG("%s: Failed to reach rtu checkpoint for task \"%s\"\n",
					  __func__, trc->name);
				return 0;
			}

			Elf32_Ehdr *new_sw = (Elf32_Ehdr *)&_rtuappv2_elf_start;

			INFO_MSG("Starting runtime update.\n");
			if (!migrator_runtime_update(trc, new_sw)) {
				ERROR_MSG("Runtime updating failed.\n");
				return 0;
			}
			INFO_MSG("Runtime update complete. (-> v2)\n");
		}

		vTaskDelay(1000/portTICK_RATE_MS);

		if ((trc = task_find("rtuapp"))) {
			if (!task_wait_for_checkpoint(trc, cp_req_rtu)) {
				ERROR_MSG("%s: Failed to reach rtu checkpoint for task \"%s\"\n",
					  __func__, trc->name);
				return 0;
			}

			Elf32_Ehdr *new_sw = (Elf32_Ehdr *)&_rtuappv1_elf_start;

			INFO_MSG("Starting runtime update.\n");
			if (!migrator_runtime_update(trc, new_sw)) {
				ERROR_MSG("Runtime updating failed.\n");
				return 0;
			}
			INFO_MSG("Runtime update complete. (-> v1)\n");
		}
	}
}
 
int main()
{
	Elf32_Ehdr *simple_elfh = APPLICATION_ELF(simple);
	Elf32_Ehdr *writer_elfh = APPLICATION_ELF(writer);
	Elf32_Ehdr *reader_elfh = APPLICATION_ELF(reader);
	Elf32_Ehdr *rtuapp_elfh = APPLICATION_ELF(rtuappv1);
    //printf_uart("printf test");
//printf_uart("%s", "printf test");
	//Elf32_Ehdr *sys_elfh = SYSTEM_ELF;
        //INFO_MSG("System ELF magic checks out @ 0x%lx\n", (u_int32_t)sys_elfh);
	//INFO_MSG("Starting scheduler\n");
	//if (check_elf_magic(sys_elfh))
		//INFO_MSG("System ELF magic checks out @ 0x%lx\n", (u_int32_t)sys_elfh);
	//else {
		//ERROR_MSG("Wrong System ELF magic @ 0x%lx\n", (u_int32_t)sys_elfh);	
	//	goto exit;
	//}

	/*
	 * Registering tasks
	 */

	task_register_cons *simplec = task_register("simple", simple_elfh);
	task_register_cons *readerc = task_register("reader", reader_elfh);
	task_register_cons *writerc = task_register("writer", writer_elfh);
	task_register_cons *rtuappc = task_register("rtuapp", rtuapp_elfh);

	if (!task_alloc(simplec)) {
		ERROR_MSG("Could not alloc memory for task \"simple\"\n");
		goto exit;
	}
	if (!task_alloc(readerc)) {
		ERROR_MSG("Could not alloc memory for task \"reader\"\n");
		goto exit;
	}
	if (!task_alloc(writerc)) {
		ERROR_MSG("Could not alloc memory for task \"writer\"\n");
		goto exit;
	}
	if (!task_alloc(rtuappc)) {
		ERROR_MSG("Could not alloc memory for task \"rtuapp\"\n");
		goto exit;
	}

	/*
	 * Linking tasks
	 */

	if (!task_link(simplec)) {
		ERROR_MSG("Could not link \"simple\" task\n");
		goto exit;
	}
	if (!task_link(readerc)) {
		ERROR_MSG("Could not link \"reader\" task\n");
		goto exit;
	}
	if (!task_link(writerc)) {
		ERROR_MSG("Could not link \"writer\" task\n");
		goto exit;
	}
	if (!task_link(rtuappc)) {
		ERROR_MSG("Could not link \"rtuapp\" task\n");
		goto exit;
	}

	/*
	 * Starting tasks
	 */
	
	if (!task_start(simplec)) {
		ERROR_MSG("Could not start \"simple\" task\n");
		goto exit;
	}

	if (!task_start(readerc)) {
		ERROR_MSG("Could not start \"reader\" task\n");
		goto exit;
	}

	if (!task_start(writerc)) {
		ERROR_MSG("Could not start \"writer\" task\n");
		goto exit;
	}

	if (!task_start(rtuappc)) {
		ERROR_MSG("Could not start \"rtuapp\" task\n");
		goto exit;
	}

	/*
	 * Create migration task.
	 */
    /* anshul commenting below for the time being */
	/* if (!migrator_start()) {
		ERROR_MSG("Could not start migrator.\n");
		goto exit;
	} */
	
	INFO_MSG("Starting scheduler\n");
	vTaskStartScheduler();

exit:
	INFO_MSG("Going into infinite loop...\n");
	while(1)
		;
}

void vApplicationMallocFailedHook( void )
{
	__asm volatile (" smc #0 ");
}

void vApplicationIdleHook( void )
{

}
