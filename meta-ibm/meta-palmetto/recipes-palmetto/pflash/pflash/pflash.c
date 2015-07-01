#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <byteswap.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <limits.h>
#include <arpa/inet.h>
#include <assert.h>

#include <libflash/libflash.h>
#include <libflash/libffs.h>
#include "progress.h"
#include "io.h"
#include "ast.h"
#include "sfc-ctrl.h"

#define __aligned(x)			__attribute__((aligned(x)))

#define PFLASH_VERSION	"0.8.6"

static bool must_confirm = true;
static bool dummy_run;
static bool need_relock;
static bool bmc_flash;
#ifdef __powerpc__
static bool using_sfc;
#endif

#define FILE_BUF_SIZE	0x10000
static uint8_t file_buf[FILE_BUF_SIZE] __aligned(0x1000);

static struct spi_flash_ctrl	*fl_ctrl;
static struct flash_chip	*fl_chip;
static struct ffs_handle	*ffsh;
static uint32_t			fl_total_size, fl_erase_granule;
static const char		*fl_name;
static int32_t			ffs_index = -1;

static void check_confirm(void)
{
	char yes[8], *p;

	if (!must_confirm)
		return;

	printf("WARNING ! This will modify your %s flash chip content !\n",
	       bmc_flash ? "BMC" : "HOST");
	printf("Enter \"yes\" to confirm:");
	memset(yes, 0, sizeof(yes));
	if (!fgets(yes, 7, stdin))
		exit(1);
	p = strchr(yes, 10);
	if (p)
		*p = 0;
	p = strchr(yes, 13);
	if (p)
		*p = 0;
	if (strcmp(yes, "yes")) {
		printf("Operation cancelled !\n");
		exit(1);
	}
	must_confirm = false;
}

static void print_flash_info(void)
{
	uint32_t i;
	int rc;

	printf("Flash info:\n");
	printf("-----------\n");
	printf("Name          = %s\n", fl_name);
	printf("Total size    = %dMB \n", fl_total_size >> 20);
	printf("Erase granule = %dKB \n", fl_erase_granule >> 10);

	if (bmc_flash)
		return;

	if (!ffsh) {
		rc = ffs_open_flash(fl_chip, 0, 0, &ffsh);
		if (rc) {
			fprintf(stderr, "Error %d opening ffs !\n", rc);
			ffsh = NULL;
		}
	}
	if (!ffsh)
		return;

	printf("\n");
	printf("Partitions:\n");
	printf("-----------\n");

	for (i = 0;; i++) {
		uint32_t start, size, act, end;
		char *name;

		rc = ffs_part_info(ffsh, i, &name, &start, &size, &act);
		if (rc == FFS_ERR_PART_NOT_FOUND)
			break;
		if (rc) {
			fprintf(stderr, "Error %d scanning partitions\n", rc);
			break;
		}
		end = start + size;
		printf("ID=%02d %15s %08x..%08x (actual=%08x)\n",
		       i, name, start, end, act);
		free(name);
	}
}

static void lookup_partition(const char *name)
{
	uint32_t index;
	int rc;

	/* Open libffs if needed */
	if (!ffsh) {
		rc = ffs_open_flash(fl_chip, 0, 0, &ffsh);
		if (rc) {
			fprintf(stderr, "Error %d opening ffs !\n", rc);
			exit(1);
		}
	}

	/* Find partition */
	rc = ffs_lookup_part(ffsh, name, &index);
	if (rc == FFS_ERR_PART_NOT_FOUND) {
		fprintf(stderr, "Partition '%s' not found !\n", name);
		exit(1);
	}
	if (rc) {
		fprintf(stderr, "Error %d looking for partition '%s' !\n",
			rc, name);
		exit(1);
	}
	ffs_index = index;
}

static void erase_chip(void)
{
	int rc;

	printf("About to erase chip !\n");
	check_confirm();

	printf("Erasing... (may take a while !) ");
	fflush(stdout);

	if (dummy_run) {
		printf("skipped (dummy)\n");
		return;
	}

	rc = flash_erase_chip(fl_chip);
	if (rc) {
		fprintf(stderr, "Error %d erasing chip\n", rc);
		exit(1);
	}

	printf("done !\n");
}

static void erase_range(uint32_t start, uint32_t size, bool will_program)
{
	uint32_t done = 0;
	int rc;

	printf("About to erase 0x%08x..0x%08x !\n", start, start + size);
	check_confirm();

	if (dummy_run) {
		printf("skipped (dummy)\n");
		return;
	}

	printf("Erasing...\n");
	progress_init(size >> 8);
	while(size) {
		/* If aligned to 64k and at least 64k, use 64k erase */
		if ((start & 0xffff) == 0 && size >= 0x10000) {
			rc = flash_erase(fl_chip, start, 0x10000);
			if (rc) {
				fprintf(stderr, "Error %d erasing 0x%08x\n",
					rc, start);
				exit(1);
			}
			start += 0x10000;
			size -= 0x10000;
			done += 0x10000;
		} else {
			rc = flash_erase(fl_chip, start, 0x1000);
			if (rc) {
				fprintf(stderr, "Error %d erasing 0x%08x\n",
					rc, start);
				exit(1);
			}
			start += 0x1000;
			size -= 0x1000;
			done += 0x1000;
		}
		progress_tick(done >> 8);
	}
	progress_end();

	/* If this is a flash partition, mark it empty if we aren't
	 * going to program over it as well
	 */
	if (ffsh && ffs_index >= 0 && !will_program) {
		printf("Updating actual size in partition header...\n");
		ffs_update_act_size(ffsh, ffs_index, 0);
	}
}

static void program_file(const char *file, uint32_t start, uint32_t size)
{
	int fd, rc;
	ssize_t len;
	uint32_t actual_size = 0;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		perror("Failed to open file");
		exit(1);
	}
	printf("About to program \"%s\" at 0x%08x..0x%08x !\n",
	       file, start, size);
	check_confirm();

	if (dummy_run) {
		printf("skipped (dummy)\n");
		return;
	}

	printf("Programming & Verifying...\n");
	progress_init(size >> 8);
	while(size) {
		len = read(fd, file_buf, FILE_BUF_SIZE);
		if (len < 0) {
			perror("Error reading file");
			exit(1);
		}
		if (len == 0)
			break;
		if (len > size)
			len = size;
		size -= len;
		actual_size += len;
		rc = flash_write(fl_chip, start, file_buf, len, true);
		if (rc) {
			if (rc == FLASH_ERR_VERIFY_FAILURE)
				fprintf(stderr, "Verification failed for"
					" chunk at 0x%08x\n", start);
			else
				fprintf(stderr, "Flash write error %d for"
					" chunk at 0x%08x\n", rc, start);
			exit(1);
		}
		start += len;
		progress_tick(actual_size >> 8);
	}
	progress_end();
	close(fd);

	/* If this is a flash partition, adjust its size */
	if (ffsh && ffs_index >= 0) {
		printf("Updating actual size in partition header...\n");
		ffs_update_act_size(ffsh, ffs_index, actual_size);
	}
}

static void do_read_file(const char *file, uint32_t start, uint32_t size)
{
	int fd, rc;
	ssize_t len;
	uint32_t done = 0;

	fd = open(file, O_WRONLY | O_TRUNC | O_CREAT, 00666);
	if (fd == -1) {
		perror("Failed to open file");
		exit(1);
	}
	printf("Reading to \"%s\" from 0x%08x..0x%08x !\n",
	       file, start, size);

	progress_init(size >> 8);
	while(size) {
		len = size > FILE_BUF_SIZE ? FILE_BUF_SIZE : size;
		rc = flash_read(fl_chip, start, file_buf, len);
		if (rc) {
			fprintf(stderr, "Flash read error %d for"
				" chunk at 0x%08x\n", rc, start);
			exit(1);
		}
		rc = write(fd, file_buf, len);
		if (rc < 0) {
			perror("Error writing file");
			exit(1);
		}
		start += len;
		size -= len;
		done += len;
		progress_tick(done >> 8);
	}
	progress_end();
	close(fd);
}

static void enable_4B_addresses(void)
{
	int rc;

	printf("Switching to 4-bytes address mode\n");

	rc = flash_force_4b_mode(fl_chip, true);
	if (rc) {
		fprintf(stderr, "Error %d enabling 4b mode\n", rc);
		exit(1);
	}
}

static void disable_4B_addresses(void)
{
	int rc;

	printf("Switching to 3-bytes address mode\n");

	rc = flash_force_4b_mode(fl_chip, false);
	if (rc) {
		fprintf(stderr, "Error %d disabling 4b mode\n", rc);
		exit(1);
	}
}

static void flash_access_cleanup_bmc(void)
{
	if (ffsh)
		ffs_close(ffsh);
	flash_exit(fl_chip);
	ast_sf_close(fl_ctrl);
	close_devs();
}

static void flash_access_setup_bmc(bool use_lpc, bool need_write)
{
	int rc;

	/* Open and map devices */
	open_devs(use_lpc, true);

	/* Create the AST flash controller */
	rc = ast_sf_open(AST_SF_TYPE_BMC, &fl_ctrl);
	if (rc) {
		fprintf(stderr, "Failed to open controller\n");
		exit(1);
	}

	/* Open flash chip */
	rc = flash_init(fl_ctrl, &fl_chip);
	if (rc) {
		fprintf(stderr, "Failed to open flash chip\n");
		exit(1);
	}

	/* Setup cleanup function */
	atexit(flash_access_cleanup_bmc);
}

static void flash_access_cleanup_pnor(void)
{
	/* Re-lock flash */
	if (need_relock)
		set_wrprotect(true);

	if (ffsh)
		ffs_close(ffsh);
	flash_exit(fl_chip);
#ifdef __powerpc__
	if (using_sfc)
		sfc_close(fl_ctrl);
	else
		ast_sf_close(fl_ctrl);
#else
	ast_sf_close(fl_ctrl);
#endif
	close_devs();
}

static void flash_access_setup_pnor(bool use_lpc, bool use_sfc, bool need_write)
{
	int rc;

	/* Open and map devices */
	open_devs(use_lpc, false);

#ifdef __powerpc__
	if (use_sfc) {
		/* Create the SFC flash controller */
		rc = sfc_open(&fl_ctrl);
		if (rc) {
			fprintf(stderr, "Failed to open controller\n");
			exit(1);
		}
		using_sfc = true;
	} else {
#endif			
		/* Create the AST flash controller */
		rc = ast_sf_open(AST_SF_TYPE_PNOR, &fl_ctrl);
		if (rc) {
			fprintf(stderr, "Failed to open controller\n");
			exit(1);
		}
#ifdef __powerpc__
	}
#endif

	/* Open flash chip */
	rc = flash_init(fl_ctrl, &fl_chip);
	if (rc) {
		fprintf(stderr, "Failed to open flash chip\n");
		exit(1);
	}

	/* Unlock flash (PNOR only) */
	if (need_write)
		need_relock = set_wrprotect(false);

	/* Setup cleanup function */
	atexit(flash_access_cleanup_pnor);
}

static void print_version(void)
{
	printf("Palmetto Flash tool " PFLASH_VERSION "\n");
}

static void print_help(const char *pname)
{
	printf("Usage: %s [options] commands...\n\n", pname);
	printf(" Options:\n");
	printf("\t-a address, --address=address\n");
	printf("\t\tSpecify the start address for erasing, reading\n");
	printf("\t\tor flashing\n\n");
	printf("\t-s size, --size=size\n");
	printf("\t\tSpecify the size in bytes for erasing, reading\n");
	printf("\t\tor flashing\n\n");
	printf("\t-P part_name, --partition=part_name\n");
	printf("\t\tSpecify the partition whose content is to be erased\n");
	printf("\t\tprogrammed or read. This is an alternative to -a and -s\n");
	printf("\t\tif both -P and -s are specified, the smallest of the\n");
	printf("\t\ttwo will be used\n\n");
	printf("\t-f, --force\n");
	printf("\t\tDon't ask for confirmation before erasing or flashing\n\n");
	printf("\t-d, --dummy\n");
	printf("\t\tDon't write to flash\n\n");
#ifdef __powerpc__
	printf("\t-l, --lpc\n");
	printf("\t\tUse LPC accesses instead of PCI\n\n");
#endif
	printf("\t-b, --bmc\n");
	printf("\t\tTarget BMC flash instead of host flash\n\n");
	printf(" Commands:\n");
	printf("\t-4, --enable-4B\n");
	printf("\t\tSwitch the flash and controller to 4-bytes address\n");
	printf("\t\tmode (no confirmation needed).\n\n");
	printf("\t-3, --disable-4B\n");
	printf("\t\tSwitch the flash and controller to 3-bytes address\n");
	printf("\t\tmode (no confirmation needed).\n\n");
	printf("\t-r file, --read=file\n");
	printf("\t\tRead flash content from address into file, use -s\n");
	printf("\t\tto specify the size to read (or it will use the source\n");
	printf("\t\tfile size if used in conjunction with -p and -s is not\n");
	printf("\t\tspecified). When using -r together with -e or -p, the\n");
	printf("\t\tread will be peformed first\n\n");
	printf("\t-E, --erase-all\n");
	printf("\t\tErase entire flash chip\n");
	printf("\t\t(Not supported on all chips/controllers)\n\n");
	printf("\t-e, --erase\n");
	printf("\t\tErase the specified region. If size or address are not\n");
	printf("\t\tspecified, but \'--program\' is used, then the file\n");
	printf("\t\tsize will be used (rounded to an erase block) and the\n");
	printf("\t\taddress defaults to 0.\n\n");
	printf("\t-p file, --program=file\n");
	printf("\t\tWill program the file to flash. If the address is not\n");
	printf("\t\tspecified, it will use 0. If the size is not specified\n");
	printf("\t\tit will use the file size. Otherwise it will limit to\n");
	printf("\t\tthe specified size (whatever is smaller). If used in\n");
	printf("\t\tconjunction with any erase command, the erase will\n");
	printf("\t\ttake place first.\n\n");
	printf("\t-t, --tune\n");
	printf("\t\tJust tune the flash controller & access size\n");
	printf("\t\t(Implicit for all other operations)\n\n");
	printf("\t-i, --info\n");
	printf("\t\tDisplay some information about the flash.\n\n");
	printf("\t-h, --help\n");
	printf("\t\tThis message.\n\n");
}

int main(int argc, char *argv[])
{
	const char *pname = argv[0];
	uint32_t address = 0, read_size = 0, write_size = 0;
	uint32_t erase_start = 0, erase_size = 0;
	bool erase = false;
	bool program = false, erase_all = false, info = false, do_read = false;
	bool enable_4B = false, disable_4B = false, use_lpc = true;
	bool show_help = false, show_version = false;
	bool has_sfc = false, has_ast = false;
	bool no_action = false, tune = false;
	char *write_file = NULL, *read_file = NULL, *part_name = NULL;
	int rc;

	while(1) {
		static struct option long_opts[] = {
			{"address",	required_argument,	NULL,	'a'},
			{"size",	required_argument,	NULL,	's'},
			{"partition",	required_argument,	NULL,	'P'},
			{"lpc",		no_argument,		NULL,	'l'},
			{"bmc",		no_argument,		NULL,	'b'},
			{"enable-4B",	no_argument,		NULL,	'4'},
			{"disable-4B",	no_argument,		NULL,	'3'},
			{"read",	required_argument,	NULL,	'r'},
			{"erase-all",	no_argument,		NULL,	'E'},
			{"erase",	no_argument,		NULL,	'e'},
			{"program",	required_argument,	NULL,	'p'},
			{"force",	no_argument,		NULL,	'f'},
			{"info",	no_argument,		NULL,	'i'},
			{"tune",	no_argument,		NULL,	't'},
			{"dummy",	no_argument,		NULL,	'd'},
			{"help",	no_argument,		NULL,	'h'},
			{"version",	no_argument,		NULL,	'v'},
			{"debug",	no_argument,		NULL,	'g'},
		};
		int c, oidx = 0;

		c = getopt_long(argc, argv, "a:s:P:r:43Eep:fdihlvbtg",
				long_opts, &oidx);
		if (c == EOF)
			break;
		switch(c) {
		case 'a':
			address = strtoul(optarg, NULL, 0);
			break;
		case 's':
			read_size = write_size = strtoul(optarg, NULL, 0);
			break;
		case 'P':
			part_name = strdup(optarg);
			break;
		case '4':
			enable_4B = true;
			break;
		case '3':
			disable_4B = true;
			break;
		case 'r':
			do_read = true;
			read_file = strdup(optarg);
			break;
		case 'E':
			erase_all = erase = true;
			break;
		case 'e':
			erase = true;
			break;
		case 'p':
			program = true;
			write_file = strdup(optarg);
			break;
		case 'f':
			must_confirm = false;
			break;
		case 'd':
			must_confirm = false;
			dummy_run = true;
			break;
		case 'i':
			info = true;
			break;
		case 'l':
			use_lpc = true;
			break;
		case 'b':
			bmc_flash = true;
			break;
		case 't':
			tune = true;
			break;
		case 'v':
			show_version = true;
			break;
		case 'h':
			show_help = show_version = true;
			break;
		case 'g':
			libflash_debug = true;
			break;
		default:
			exit(1);
		}
	}

	/* Check if we need to access the flash at all (which will
	 * also tune them as a side effect
	 */
	no_action = !erase && !program && !info && !do_read &&
		!enable_4B && !disable_4B && !tune;

	/* Nothing to do, if we didn't already, print usage */
	if (no_action && !show_version)
		show_help = show_version = true;

	if (show_version)
		print_version();
	if (show_help)
		print_help(pname);

	if (no_action)
		return 0;

	/* --enable-4B and --disable-4B are mutually exclusive */
	if (enable_4B && disable_4B) {
		fprintf(stderr, "--enable-4B and --disable-4B are mutually"
			" exclusive !\n");
		exit(1);
	}

	/* 4B not supported on BMC flash */
	if (enable_4B && bmc_flash) {
		fprintf(stderr, "--enable-4B not supported on BMC flash !\n");
		exit(1);
	}

	/* partitions not supported on BMC flash */
	if (part_name && bmc_flash) {
		fprintf(stderr, "--partition not supported on BMC flash !\n");
		exit(1);
	}

	/* part-name and erase-all make no sense together */
	if (part_name && erase_all) {
		fprintf(stderr, "--partition and --erase-all are mutually"
			" exclusive !\n");
		exit(1);
	}

	/* Read command should always come with a file */
	if (do_read && !read_file) {
		fprintf(stderr, "Read with no file specified !\n");
		exit(1);
	}

	/* Program command should always come with a file */
	if (program && !write_file) {
		fprintf(stderr, "Program with no file specified !\n");
		exit(1);
	}

	/* If both partition and address specified, error out */
	if (address && part_name) {
		fprintf(stderr, "Specify partition or address, not both !\n");
		exit(1);
	}

	/* If file specified but not size, get size from file
	 */
	if (write_file && !write_size) {
		struct stat stbuf;

		if (stat(write_file, &stbuf)) {
			perror("Failed to get file size");
			exit(1);
		}
		write_size = stbuf.st_size;
	}

	/* Check platform */
	check_platform(&has_sfc, &has_ast);

	/* Prepare for access */
	if (bmc_flash) {
		if (!has_ast) {
			fprintf(stderr, "No BMC on this platform\n");
			exit(1);
		}
		flash_access_setup_bmc(use_lpc, erase || program);
	} else {
		if (!has_ast && !has_sfc) {
			fprintf(stderr, "No BMC nor SFC on this platform\n");
			exit(1);
		}
		flash_access_setup_pnor(use_lpc, has_sfc, erase || program);
	}

	rc = flash_get_info(fl_chip, &fl_name,
			    &fl_total_size, &fl_erase_granule);
	if (rc) {
		fprintf(stderr, "Error %d getting flash info\n", rc);
		exit(1);
	}

	/* If -t is passed, then print a nice message */
	if (tune)
		printf("Flash and controller tuned\n");

	/* If read specified and no read_size, use flash size */
	if (do_read && !read_size && !part_name)
		read_size = fl_total_size;

	/* We have a partition specified, grab the details */
	if (part_name)
		lookup_partition(part_name);

	/* We have a partition, adjust read/write size if needed */
	if (ffsh && ffs_index >= 0) {
		uint32_t pstart, pmaxsz, pactsize;
		int rc;

		rc = ffs_part_info(ffsh, ffs_index, NULL,
				   &pstart, &pmaxsz, &pactsize);
		if (rc) {
			fprintf(stderr,"Failed to get partition info\n");
			exit(1);
		}

		/* Read size is obtained from partition "actual" size */
		if (!read_size)
			read_size = pactsize;

		/* Write size is max size of partition */
		if (!write_size)
			write_size = pmaxsz;

		/* Crop write size to partition size */
		if (write_size > pmaxsz) {
			printf("WARNING: Size (%d bytes) larger than partition"
			       " (%d bytes), cropping to fit\n",
			       write_size, pmaxsz);
			write_size = pmaxsz;
		}

		/* If erasing, check partition alignment */
		if (erase && ((pstart | pmaxsz) & 0xfff)) {
			fprintf(stderr,"Partition not aligned properly\n");
			exit(1);
		}

		/* Set address */
		address = pstart;
	}

	/* Align erase boundaries */
	if (erase && !erase_all) {
		uint32_t mask = 0xfff;
		uint32_t erase_end;

		/* Dummy size for erase, will be adjusted later */
		if (!write_size)
			write_size = 1;
		erase_start = address & ~mask;
		erase_end = ((address + write_size) + mask) & ~mask;
		erase_size = erase_end - erase_start;

		if (erase_start != address || erase_size != write_size)
			fprintf(stderr, "WARNING: Erase region adjusted"
				" to 0x%08x..0x%08x\n",
				erase_start, erase_end);
	}

	/* Process commands */
	if (enable_4B)
		enable_4B_addresses();
	if (disable_4B)
		disable_4B_addresses();
	if (info)
		print_flash_info();
	if (do_read)
		do_read_file(read_file, address, read_size);
	if (erase_all)
		erase_chip();
	else if (erase)
		erase_range(erase_start, erase_size, program);
	if (program)
		program_file(write_file, address, write_size);

	return 0;
}
