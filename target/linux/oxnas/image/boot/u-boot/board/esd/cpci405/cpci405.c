/*
 * (C) Copyright 2001-2003
 * Stefan Roese, esd gmbh germany, stefan.roese@esd-electronics.com
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/processor.h>
#include <command.h>
#include <malloc.h>
#include <net.h>

/* ------------------------------------------------------------------------- */
extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);       /*cmd_boot.c*/
#if 0
#define FPGA_DEBUG
#endif

/* fpga configuration data - generated by bin2cc */
const unsigned char fpgadata[] =
{
#ifdef CONFIG_CPCI405_VER2
# ifdef CONFIG_CPCI405AB
#  include "fpgadata_cpci405ab.c"
# else
#  include "fpgadata_cpci4052.c"
# endif
#else
# include "fpgadata_cpci405.c"
#endif
};

/*
 * include common fpga code (for esd boards)
 */
#include "../common/fpga.c"


#include "../common/auto_update.h"

#ifdef CONFIG_CPCI405AB
au_image_t au_image[] = {
	{"cpci405ab/preinst.img", 0, -1, AU_SCRIPT},
	{"cpci405ab/pImage", 0xffc00000, 0x000c0000, AU_NOR},
	{"cpci405ab/pImage.initrd", 0xffcc0000, 0x00300000, AU_NOR},
	{"cpci405ab/u-boot.img", 0xfffc0000, 0x00040000, AU_FIRMWARE},
	{"cpci405ab/postinst.img", 0, 0, AU_SCRIPT},
};
#else
#ifdef CONFIG_CPCI405_VER2
au_image_t au_image[] = {
	{"cpci4052/preinst.img", 0, -1, AU_SCRIPT},
	{"cpci4052/pImage", 0xffc00000, 0x000c0000, AU_NOR},
	{"cpci4052/pImage.initrd", 0xffcc0000, 0x00300000, AU_NOR},
	{"cpci4052/u-boot.img", 0xfffc0000, 0x00040000, AU_FIRMWARE},
	{"cpci4052/postinst.img", 0, 0, AU_SCRIPT},
};
#else
au_image_t au_image[] = {
	{"cpci405/preinst.img", 0, -1, AU_SCRIPT},
	{"cpci405/pImage", 0xffc00000, 0x000c0000, AU_NOR},
	{"cpci405/pImage.initrd", 0xffcc0000, 0x00310000, AU_NOR},
	{"cpci405/u-boot.img", 0xfffd0000, 0x00030000, AU_FIRMWARE},
	{"cpci405/postinst.img", 0, 0, AU_SCRIPT},
};
#endif
#endif

int N_AU_IMAGES = (sizeof(au_image) / sizeof(au_image[0]));


/* Prototypes */
int cpci405_version(void);
int gunzip(void *, int, unsigned char *, unsigned long *);
void lxt971_no_sleep(void);


int board_early_init_f (void)
{
#ifndef CONFIG_CPCI405_VER2
	int index, len, i;
	int status;
#endif

#ifdef FPGA_DEBUG
	DECLARE_GLOBAL_DATA_PTR;

	/* set up serial port with default baudrate */
	(void) get_clocks ();
	gd->baudrate = CONFIG_BAUDRATE;
	serial_init ();
	console_init_f();
#endif

	/*
	 * First pull fpga-prg pin low, to disable fpga logic (on version 2 board)
	 */
	out32(GPIO0_ODR, 0x00000000);        /* no open drain pins      */
	out32(GPIO0_TCR, CFG_FPGA_PRG);      /* setup for output        */
	out32(GPIO0_OR,  CFG_FPGA_PRG);      /* set output pins to high */
	out32(GPIO0_OR, 0);                  /* pull prg low            */

	/*
	 * Boot onboard FPGA
	 */
#ifndef CONFIG_CPCI405_VER2
	if (cpci405_version() == 1) {
		status = fpga_boot((unsigned char *)fpgadata, sizeof(fpgadata));
		if (status != 0) {
			/* booting FPGA failed */
#ifndef FPGA_DEBUG
			DECLARE_GLOBAL_DATA_PTR;

			/* set up serial port with default baudrate */
			(void) get_clocks ();
			gd->baudrate = CONFIG_BAUDRATE;
			serial_init ();
			console_init_f();
#endif
			printf("\nFPGA: Booting failed ");
			switch (status) {
			case ERROR_FPGA_PRG_INIT_LOW:
				printf("(Timeout: INIT not low after asserting PROGRAM*)\n ");
				break;
			case ERROR_FPGA_PRG_INIT_HIGH:
				printf("(Timeout: INIT not high after deasserting PROGRAM*)\n ");
				break;
			case ERROR_FPGA_PRG_DONE:
				printf("(Timeout: DONE not high after programming FPGA)\n ");
				break;
			}

			/* display infos on fpgaimage */
			index = 15;
			for (i=0; i<4; i++) {
				len = fpgadata[index];
				printf("FPGA: %s\n", &(fpgadata[index+1]));
				index += len+3;
			}
			putc ('\n');
			/* delayed reboot */
			for (i=20; i>0; i--) {
				printf("Rebooting in %2d seconds \r",i);
				for (index=0;index<1000;index++)
					udelay(1000);
			}
			putc ('\n');
			do_reset(NULL, 0, 0, NULL);
		}
	}
#endif /* !CONFIG_CPCI405_VER2 */

	/*
	 * IRQ 0-15  405GP internally generated; active high; level sensitive
	 * IRQ 16    405GP internally generated; active low; level sensitive
	 * IRQ 17-24 RESERVED
	 * IRQ 25 (EXT IRQ 0) CAN0; active low; level sensitive
	 * IRQ 26 (EXT IRQ 1) CAN1 (+FPGA on CPCI4052) ; active low; level sensitive
	 * IRQ 27 (EXT IRQ 2) PCI SLOT 0; active low; level sensitive
	 * IRQ 28 (EXT IRQ 3) PCI SLOT 1; active low; level sensitive
	 * IRQ 29 (EXT IRQ 4) PCI SLOT 2; active low; level sensitive
	 * IRQ 30 (EXT IRQ 5) PCI SLOT 3; active low; level sensitive
	 * IRQ 31 (EXT IRQ 6) COMPACT FLASH; active high; level sensitive
	 */
	mtdcr(uicsr, 0xFFFFFFFF);       /* clear all ints */
	mtdcr(uicer, 0x00000000);       /* disable all ints */
	mtdcr(uiccr, 0x00000000);       /* set all to be non-critical*/
	if (cpci405_version() == 3) {
		mtdcr(uicpr, 0xFFFFFF99);       /* set int polarities */
	} else {
		mtdcr(uicpr, 0xFFFFFF81);       /* set int polarities */
	}
	mtdcr(uictr, 0x10000000);       /* set int trigger levels */
	mtdcr(uicvcr, 0x00000001);      /* set vect base=0,INT0 highest priority*/
	mtdcr(uicsr, 0xFFFFFFFF);       /* clear all ints */

	return 0;
}


/* ------------------------------------------------------------------------- */

int ctermm2(void)
{
#ifdef CONFIG_CPCI405_VER2
	return 0;                       /* no, board is cpci405 */
#else
	if ((*(unsigned char *)0xf0000400 == 0x00) &&
	    (*(unsigned char *)0xf0000401 == 0x01))
		return 0;               /* no, board is cpci405 */
	else
		return -1;              /* yes, board is cterm-m2 */
#endif
}


int cpci405_host(void)
{
	if (mfdcr(strap) & PSR_PCI_ARBIT_EN)
		return -1;              /* yes, board is cpci405 host */
	else
		return 0;               /* no, board is cpci405 adapter */
}


int cpci405_version(void)
{
	unsigned long cntrl0Reg;
	unsigned long value;

	/*
	 * Setup GPIO pins (CS2/GPIO11 and CS3/GPIO12 as GPIO)
	 */
	cntrl0Reg = mfdcr(cntrl0);
	mtdcr(cntrl0, cntrl0Reg | 0x03000000);
	out32(GPIO0_ODR, in32(GPIO0_ODR) & ~0x00180000);
	out32(GPIO0_TCR, in32(GPIO0_TCR) & ~0x00180000);
	udelay(1000);                   /* wait some time before reading input */
	value = in32(GPIO0_IR) & 0x00180000;       /* get config bits */

	/*
	 * Restore GPIO settings
	 */
	mtdcr(cntrl0, cntrl0Reg);

	switch (value) {
	case 0x00180000:
		/* CS2==1 && CS3==1 -> version 1 */
		return 1;
	case 0x00080000:
		/* CS2==0 && CS3==1 -> version 2 */
		return 2;
	case 0x00100000:
		/* CS2==1 && CS3==0 -> version 3 */
		return 3;
	case 0x00000000:
		/* CS2==0 && CS3==0 -> version 4 */
		return 4;
	default:
		/* should not be reached! */
		return 2;
	}
}


int misc_init_f (void)
{
	return 0;  /* dummy implementation */
}


int misc_init_r (void)
{
	DECLARE_GLOBAL_DATA_PTR;
	unsigned long cntrl0Reg;

	/* adjust flash start and offset */
	gd->bd->bi_flashstart = 0 - gd->bd->bi_flashsize;
	gd->bd->bi_flashoffset = 0;

#ifdef CONFIG_CPCI405_VER2
	{
	unsigned char *dst;
	ulong len = sizeof(fpgadata);
	int status;
	int index;
	int i;

	/*
	 * On CPCI-405 version 2 the environment is saved in eeprom!
	 * FPGA can be gzip compressed (malloc) and booted this late.
	 */

	if (cpci405_version() >= 2) {
		/*
		 * Setup GPIO pins (CS6+CS7 as GPIO)
		 */
		cntrl0Reg = mfdcr(cntrl0);
		mtdcr(cntrl0, cntrl0Reg | 0x00300000);

		dst = malloc(CFG_FPGA_MAX_SIZE);
		if (gunzip (dst, CFG_FPGA_MAX_SIZE, (uchar *)fpgadata, &len) != 0) {
			printf ("GUNZIP ERROR - must RESET board to recover\n");
			do_reset (NULL, 0, 0, NULL);
		}

		status = fpga_boot(dst, len);
		if (status != 0) {
			printf("\nFPGA: Booting failed ");
			switch (status) {
			case ERROR_FPGA_PRG_INIT_LOW:
				printf("(Timeout: INIT not low after asserting PROGRAM*)\n ");
				break;
			case ERROR_FPGA_PRG_INIT_HIGH:
				printf("(Timeout: INIT not high after deasserting PROGRAM*)\n ");
				break;
			case ERROR_FPGA_PRG_DONE:
				printf("(Timeout: DONE not high after programming FPGA)\n ");
				break;
			}

			/* display infos on fpgaimage */
			index = 15;
			for (i=0; i<4; i++) {
				len = dst[index];
				printf("FPGA: %s\n", &(dst[index+1]));
				index += len+3;
			}
			putc ('\n');
			/* delayed reboot */
			for (i=20; i>0; i--) {
				printf("Rebooting in %2d seconds \r",i);
				for (index=0;index<1000;index++)
					udelay(1000);
			}
			putc ('\n');
			do_reset(NULL, 0, 0, NULL);
		}

		/* restore gpio/cs settings */
		mtdcr(cntrl0, cntrl0Reg);

		puts("FPGA:  ");

		/* display infos on fpgaimage */
		index = 15;
		for (i=0; i<4; i++) {
			len = dst[index];
			printf("%s ", &(dst[index+1]));
			index += len+3;
		}
		putc ('\n');

		free(dst);

		/*
		 * Reset FPGA via FPGA_DATA pin
		 */
		SET_FPGA(FPGA_PRG | FPGA_CLK);
		udelay(1000); /* wait 1ms */
		SET_FPGA(FPGA_PRG | FPGA_CLK | FPGA_DATA);
		udelay(1000); /* wait 1ms */

		if (cpci405_version() == 3) {
			volatile unsigned short *fpga_mode = (unsigned short *)CFG_FPGA_BASE_ADDR;
			volatile unsigned char *leds = (unsigned char *)CFG_LED_ADDR;

			/*
			 * Enable outputs in fpga on version 3 board
			 */
			*fpga_mode |= CFG_FPGA_MODE_ENABLE_OUTPUT;

			/*
			 * Set outputs to 0
			 */
			*leds = 0x00;

			/*
			 * Reset external DUART
			 */
			*fpga_mode |= CFG_FPGA_MODE_DUART_RESET;
			udelay(100);
			*fpga_mode &= ~(CFG_FPGA_MODE_DUART_RESET);
		}
	}
	else {
		puts("\n*** U-Boot Version does not match Board Version!\n");
		puts("*** CPCI-405 Version 1.x detected!\n");
		puts("*** Please use correct U-Boot version (CPCI405 instead of CPCI4052)!\n\n");
	}
	}

#else /* CONFIG_CPCI405_VER2 */

#if 0 /* test-only: code-plug now not relavant for ip-address any more */
	/*
	 * Generate last byte of ip-addr from code-plug @ 0xf0000400
	 */
	if (ctermm2()) {
		char str[32];
		unsigned char ipbyte = *(unsigned char *)0xf0000400;

		/*
		 * Only overwrite ip-addr with allowed values
		 */
		if ((ipbyte != 0x00) && (ipbyte != 0xff)) {
			bd->bi_ip_addr = (bd->bi_ip_addr & 0xffffff00) | ipbyte;
			sprintf(str, "%ld.%ld.%ld.%ld",
				(bd->bi_ip_addr & 0xff000000) >> 24,
				(bd->bi_ip_addr & 0x00ff0000) >> 16,
				(bd->bi_ip_addr & 0x0000ff00) >> 8,
				(bd->bi_ip_addr & 0x000000ff));
			setenv("ipaddr", str);
		}
	}
#endif

	if (cpci405_version() >= 2) {
		puts("\n*** U-Boot Version does not match Board Version!\n");
		puts("*** CPCI-405 Board Version 2.x detected!\n");
		puts("*** Please use correct U-Boot version (CPCI4052 instead of CPCI405)!\n\n");
	}

#endif /* CONFIG_CPCI405_VER2 */

	/*
	 * Select cts (and not dsr) on uart1
	 */
	cntrl0Reg = mfdcr(cntrl0);
	mtdcr(cntrl0, cntrl0Reg | 0x00001000);

	return (0);
}


/*
 * Check Board Identity:
 */

int checkboard (void)
{
#ifndef CONFIG_CPCI405_VER2
	int index;
	int len;
#endif
	unsigned char str[64];
	int i = getenv_r ("serial#", str, sizeof(str));
	unsigned short ver;

	puts ("Board: ");

	if (i == -1) {
		puts ("### No HW ID - assuming CPCI405");
	} else {
		puts(str);
	}

	ver = cpci405_version();
	printf(" (Ver %d.x, ", ver);

#if 0 /* test-only */
	if (ver >= 2) {
		volatile u16 *fpga_status = (u16 *)CFG_FPGA_BASE_ADDR + 1;

		if (*fpga_status & CFG_FPGA_STATUS_FLASH) {
			puts ("FLASH Bank B, ");
		} else {
			puts ("FLASH Bank A, ");
		}
	}
#endif

	if (ctermm2()) {
		unsigned char str[4];

		/*
		 * Read board-id and save in env-variable
		 */
		sprintf(str, "%d", *(unsigned char *)0xf0000400);
		setenv("boardid", str);
		printf("CTERM-M2 - Id=%s)", str);
	} else {
		if (cpci405_host()) {
			puts ("PCI Host Version)");
		} else {
			puts ("PCI Adapter Version)");
		}
	}

#ifndef CONFIG_CPCI405_VER2
	puts ("\nFPGA:  ");

	/* display infos on fpgaimage */
	index = 15;
	for (i=0; i<4; i++) {
		len = fpgadata[index];
		printf("%s ", &(fpgadata[index+1]));
		index += len+3;
	}
#endif

	putc ('\n');

	/*
	 * Disable sleep mode in LXT971
	 */
	lxt971_no_sleep();

	return 0;
}

/* ------------------------------------------------------------------------- */

long int initdram (int board_type)
{
	unsigned long val;

	mtdcr(memcfga, mem_mb0cf);
	val = mfdcr(memcfgd);

#if 0
	printf("\nmb0cf=%x\n", val); /* test-only */
	printf("strap=%x\n", mfdcr(strap)); /* test-only */
#endif

	return (4*1024*1024 << ((val & 0x000e0000) >> 17));
}

/* ------------------------------------------------------------------------- */

int testdram (void)
{
	/* TODO: XXX XXX XXX */
	printf ("test: 16 MB - ok\n");

	return (0);
}

/* ------------------------------------------------------------------------- */

#ifdef CONFIG_CPCI405_VER2
#ifdef CONFIG_IDE_RESET

void ide_set_reset(int on)
{
	volatile unsigned short *fpga_mode = (unsigned short *)CFG_FPGA_BASE_ADDR;

	/*
	 * Assert or deassert CompactFlash Reset Pin
	 */
	if (on) {		/* assert RESET */
		*fpga_mode &= ~(CFG_FPGA_MODE_CF_RESET);
	} else {		/* release RESET */
		*fpga_mode |= CFG_FPGA_MODE_CF_RESET;
	}
}

#endif /* CONFIG_IDE_RESET */
#endif /* CONFIG_CPCI405_VER2 */


#ifdef CONFIG_CPCI405AB

#define ONE_WIRE_CLEAR   (*(volatile unsigned short *)(CFG_FPGA_BASE_ADDR + CFG_FPGA_MODE) \
			  |= CFG_FPGA_MODE_1WIRE_DIR)
#define ONE_WIRE_SET     (*(volatile unsigned short *)(CFG_FPGA_BASE_ADDR + CFG_FPGA_MODE) \
			  &= ~CFG_FPGA_MODE_1WIRE_DIR)
#define ONE_WIRE_GET     (*(volatile unsigned short *)(CFG_FPGA_BASE_ADDR + CFG_FPGA_STATUS) \
			  & CFG_FPGA_MODE_1WIRE)

/*
 * Generate a 1-wire reset, return 1 if no presence detect was found,
 * return 0 otherwise.
 * (NOTE: Does not handle alarm presence from DS2404/DS1994)
 */
int OWTouchReset(void)
{
	int result;

	ONE_WIRE_CLEAR;
	udelay(480);
	ONE_WIRE_SET;
	udelay(70);

	result = ONE_WIRE_GET;

	udelay(410);
	return result;
}


/*
 * Send 1 a 1-wire write bit.
 * Provide 10us recovery time.
 */
void OWWriteBit(int bit)
{
	if (bit) {
		/*
		 * write '1' bit
		 */
		ONE_WIRE_CLEAR;
		udelay(6);
		ONE_WIRE_SET;
		udelay(64);
	} else {
		/*
		 * write '0' bit
		 */
		ONE_WIRE_CLEAR;
		udelay(60);
		ONE_WIRE_SET;
		udelay(10);
	}
}


/*
 * Read a bit from the 1-wire bus and return it.
 * Provide 10us recovery time.
 */
int OWReadBit(void)
{
	int result;

	ONE_WIRE_CLEAR;
	udelay(6);
	ONE_WIRE_SET;
	udelay(9);

	result = ONE_WIRE_GET;

	udelay(55);
	return result;
}


void OWWriteByte(int data)
{
	int loop;

	for (loop=0; loop<8; loop++) {
		OWWriteBit(data & 0x01);
		data >>= 1;
	}
}


int OWReadByte(void)
{
	int loop, result = 0;

	for (loop=0; loop<8; loop++) {
		result >>= 1;
		if (OWReadBit()) {
			result |= 0x80;
		}
	}

	return result;
}


int do_onewire(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	volatile unsigned short val;
	int result;
	int i;
	unsigned char ow_id[6];
	unsigned char str[32];
	unsigned char ow_crc;

	/*
	 * Clear 1-wire bit (open drain with pull-up)
	 */
	val = *(volatile unsigned short *)0xf0400000;
	val &= ~0x1000; /* clear 1-wire bit */
	*(volatile unsigned short *)0xf0400000 = val;

	result = OWTouchReset();
	if (result != 0) {
		puts("No 1-wire device detected!\n");
	}

	OWWriteByte(0x33); /* send read rom command */
	OWReadByte(); /* skip family code ( == 0x01) */
	for (i=0; i<6; i++) {
		ow_id[i] = OWReadByte();
	}
	ow_crc = OWReadByte(); /* read crc */

	sprintf(str, "%08X%04X", *(unsigned int *)&ow_id[0], *(unsigned short *)&ow_id[4]);
	printf("Setting environment variable 'ow_id' to %s\n", str);
	setenv("ow_id", str);

	return 0;
}
U_BOOT_CMD(
	onewire,	1,	1,	do_onewire,
	"onewire - Read 1-write ID\n",
	NULL
	);


#define CFG_I2C_EEPROM_ADDR_2	0x51	/* EEPROM CAT28WC32		*/
#define CFG_ENV_SIZE_2	0x800	/* 2048 bytes may be used for env vars*/

/*
 * Write backplane ip-address...
 */
int do_get_bpip(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	DECLARE_GLOBAL_DATA_PTR;

	bd_t *bd = gd->bd;
	char *buf;
	ulong crc;
	char str[32];
	char *ptr;
	IPaddr_t ipaddr;

	buf = malloc(CFG_ENV_SIZE_2);
	if (eeprom_read(CFG_I2C_EEPROM_ADDR_2, 0, buf, CFG_ENV_SIZE_2)) {
		puts("\nError reading backplane EEPROM!\n");
	} else {
		crc = crc32(0, buf+4, CFG_ENV_SIZE_2-4);
		if (crc != *(ulong *)buf) {
			printf("ERROR: crc mismatch %08lx %08lx\n", crc, *(ulong *)buf);
			return -1;
		}

		/*
		 * Find bp_ip
		 */
		ptr = strstr(buf+4, "bp_ip=");
		if (ptr == NULL) {
			printf("ERROR: bp_ip not found!\n");
			return -1;
		}
		ptr += 6;
		ipaddr = string_to_ip(ptr);

		/*
		 * Update whole ip-addr
		 */
		bd->bi_ip_addr = ipaddr;
		sprintf(str, "%ld.%ld.%ld.%ld",
			(bd->bi_ip_addr & 0xff000000) >> 24,
			(bd->bi_ip_addr & 0x00ff0000) >> 16,
			(bd->bi_ip_addr & 0x0000ff00) >> 8,
			(bd->bi_ip_addr & 0x000000ff));
		setenv("ipaddr", str);
		printf("Updated ip_addr from bp_eeprom to %s!\n", str);
	}

	free(buf);

	return 0;
}
U_BOOT_CMD(
	getbpip,	1,	1,	do_get_bpip,
	"getbpip - Update IP-Address with Backplane IP-Address\n",
	NULL
	);

/*
 * Set and print backplane ip...
 */
int do_set_bpip(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *buf;
	unsigned char str[32];
	ulong crc;

	if (argc < 2) {
		puts("ERROR!\n");
		return -1;
	}

	printf("Setting bp_ip to %s\n", argv[1]);
	buf = malloc(CFG_ENV_SIZE_2);
	memset(buf, 0, CFG_ENV_SIZE_2);
	sprintf(str, "bp_ip=%s", argv[1]);
	strcpy(buf+4, str);
	crc = crc32(0, buf+4, CFG_ENV_SIZE_2-4);
	*(ulong *)buf = crc;

	if (eeprom_write(CFG_I2C_EEPROM_ADDR_2, 0, buf, CFG_ENV_SIZE_2)) {
		puts("\nError writing backplane EEPROM!\n");
	}

	free(buf);

	return 0;
}
U_BOOT_CMD(
	setbpip,	2,	1,	do_set_bpip,
	"setbpip - Write Backplane IP-Address\n",
	NULL
	);

#endif /* CONFIG_CPCI405AB */
