#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <argp.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define FSI_CLK		4	//GPIOA4
#define FSI_DAT		5	//GPIOA5
#define CRONUS_SEL	6	//GPIOA6
#define PCIE_RST_N	13	//GPIOB5
#define PEX_PERST_N	14	//GPIOB6
#define POWER		33    	//GPIOE1
#define PGOOD		23    	//GPIOC7
#define FSI_ENABLE      24      //GPIOD0

/* Where to put the firmware image if booting from memory */
#define MEM_IMG_BASE (0x54000000)

/* Start of flash memory if booting from flash */
#define FLASH_IMG_BASE (0x30000000)

/* LPC registers */
#define LPC_BASE		0x1e789000
#define LPC_HICR6		0x80
#define LPC_HICR7		0x88
#define LPC_HICR8		0x8c

const char *argp_program_version = "0.1";
const char *argp_program_bug_address =
           "<ejhauptl@us.ibm.com>";

static char doc[] =
"Power Control Program.\nThis program can power on, off and reboot the system.\nAdditionally, it can power on but not start hostboot for Cronus work.";

static char args_doc[] = "ARG1";

/* initialise an argp_option struct with the options we except */
static struct argp_option options[] =
{
  {"on", 'n', 0, 0, "Turn on the system." },
  {"off", 'f', 0, 0, "Turn off the system." },
  {"reboot", 'r', 0, 0, "Reboot the system." },
  {"boot", 'b', 0, 0, "Issue boot command only." },
  {"cronus", 'c', 0, 0, "Turn on the system for Cronus work." },
  {"state", 's', 0, 0, "Check the current power state." },
  {"image", 'i', "FILE", 0, "Boot from the firmware image in FILE." },
  { 0 }
};

/* Used by `main' to communicate with `parse_opt'. */
struct arguments
{
  char *args[1];                /* ARG1 */
  int on, off, cronus, reboot, boot, state;
  char *firmware_image;
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the INPUT argument from `argp_parse', which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'n':
      arguments->on = 1;
      break;
    case 'f':
      arguments->off = 1;
      break;
    case 'r':
      arguments->reboot = 1;
      break;
    case 'c':
      arguments->cronus = 1;
      break;
    case 'b':
      arguments->boot = 1;
      break;
    case 's':
      arguments->state = 1;
      break;
    case 'i':
      arguments->firmware_image = arg;
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 1)
        /* Too many arguments. */
        argp_usage (state);

      arguments->args[state->arg_num] = arg;

      break;

    case ARGP_KEY_END:
      if (state->arg_num < 0)
        /* Not enough arguments. */
        argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

void export_gpio(int gpio) {
	int fd;
	char buf[254];
	
	//export GPIOs
	fd = open("/sys/class/gpio/export", O_WRONLY);
	sprintf(buf, "%d", gpio); 
	write(fd, buf, strlen(buf));
	close(fd);
}


void set_direction_out(int gpio) {
	int fd;
	char buf[254];

	sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
	fd = open(buf, O_WRONLY);
	write(fd, "out", 3); 
	close(fd);
}

void set_direction_in(int gpio) {
	int fd;
	char buf[254];

	sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
	fd = open(buf, O_WRONLY);
	write(fd, "in", 2); 
	close(fd);
}
int read_gpio(int gpio) {
	int fd;
	char buf[254];
	char val[1];

	export_gpio(gpio);
	set_direction_in(gpio);
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	fd = open(buf, O_RDONLY);
	read(fd,&val,1);
	close(fd);
	if (val[0]=='1') {
		return 1;
	}
	return 0;	
}

void write_value(int fd,int value) {
	if (value==0) {
		write(fd, "0", 1);
		//printf("write 0\n");
	} else {
		write(fd, "1", 1); 
		//printf("write 1\n");
	}
}

void write_value_c(int fd,char value) {
	if (value=='0') {
		write(fd, "0", 1);
		//printf("write 0\n");
	} else {
		write(fd, "1", 1); 
		//printf("write 1\n");
	}
}


void clock_cycle(int fd,int num_clks) {
        int i=0;
        for (i=0;i<num_clks;i++) {
                write_value(fd,0);
                write_value(fd,1);
        }
}

int open_gpio_out(int gpio) {

	int fd;
	char buf[254];

	export_gpio(gpio);
	set_direction_out(gpio);
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	fd = open(buf, O_WRONLY);
	
	return fd;
}
int open_gpio_in(int gpio) {

	int fd;
	char buf[254];

	export_gpio(gpio);
	set_direction_in(gpio);
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	fd = open(buf, O_RDONLY);
	
	return fd;
}


void apply_magic_pattern() {
	int fsi_clk;
	int fsi_dat;
	int i;
	int fsi_enable;

	//putcfam pu 281c 30000000 -p0
	char a[] = "000011111111110101111000111001100111111111111111111111111111101111111111";
	//putcfam pu 281c B0000000 -p0
	char b[] = "000011111111110101111000111000100111111111111111111111111111101101111111";

	fsi_enable=open_gpio_out(FSI_ENABLE);
        write_value(fsi_enable,1);

	//setup gpio
	fsi_clk=open_gpio_out(FSI_CLK);
	fsi_dat=open_gpio_out(FSI_DAT);

	printf("Starting boot...\n");
	//init state
	write_value(fsi_clk,1);
	write_value(fsi_dat,1); /* Data standby state */
	clock_cycle(fsi_clk,5000);
	//not sure why i need to do this
	write_value(fsi_dat,0);
	clock_cycle(fsi_clk,256);

	write_value(fsi_dat,1);
	clock_cycle(fsi_clk,50);

	for(i=0;i<strlen(a);i++) {
		write_value_c(fsi_dat,a[i]);
		clock_cycle(fsi_clk,1);
	}
	write_value(fsi_dat,1); /* Data standby state */
	clock_cycle(fsi_clk,5000);

	for(i=0;i<strlen(b);i++) {
		write_value_c(fsi_dat,b[i]);
		clock_cycle(fsi_clk,1);
	}
	write_value(fsi_dat,1); /* Data standby state */
	clock_cycle(fsi_clk,2);

        write_value(fsi_clk,0); /* hold clk low for clock mux */
        write_value(fsi_enable,0);
        clock_cycle(fsi_clk,16);
        write_value(fsi_clk,0); /* Data standby state */


	//close(fsi_clk);
	//close(fsi_dat);
}

uint32_t readl(void *addr)
{
	asm volatile("" : : : "memory");
	return *(volatile uint32_t *)addr;
}

void writel(uint32_t val, void *addr)
{
	asm volatile("" : : : "memory");
	*(volatile uint32_t *)addr = val;
}

void copy_flash_img(int mem_fd, int flash_fd, unsigned int size)
{
  static void *memimg, *fwimg;
  size_t pagesize = getpagesize();

  memimg = mmap(NULL, ((size/pagesize)+1)*pagesize,
		PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, MEM_IMG_BASE);
  if (memimg == MAP_FAILED) {
    perror("Unable to map image destination memory");
    exit(1);
  }

  fwimg = mmap(NULL,size, PROT_READ, MAP_SHARED, flash_fd, 0);
  if (fwimg == MAP_FAILED) {
    perror("Unable to open image source memory");
    exit(1);
  }

  /* Copy boot image */
  memcpy(memimg, fwimg, size);
}

void boot_firmware_image(int mem_fd, char *filename)
{
  int fw_fd;
  struct stat st;

  fw_fd = open(filename, O_RDONLY);
  if (fw_fd < 0) {
    perror("Unable to open flash image\n");
    exit(1);
  }

  if (stat(filename, &st)) {
    perror("Unable to determine size of firmware image");
    exit(1);
  }

  copy_flash_img(mem_fd, fw_fd, st.st_size);
  close(fw_fd);
}

int main( int argc, char *argv[] ) {
	struct arguments arguments;

  	/* Default values. */
  	arguments.on = 0;
  	arguments.off = 0;
 	arguments.reboot = 0;
	arguments.cronus = 0;
        arguments.state = 0;
	
  	/* Parse our arguments; every option seen by `parse_opt' will
     	be reflected in `arguments'. */
	arguments.firmware_image = NULL;
  	argp_parse (&argp, argc, argv, 0, 0, &arguments);
	
	int found=0;
	if (arguments.off == 1 || arguments.reboot == 1){
		int power;
		int pex;
		int pcie;
		power=open_gpio_out(POWER);
		pex=open_gpio_out(PEX_PERST_N);
		pcie=open_gpio_out(PCIE_RST_N);
                printf( "Powering Off (takes 5 seconds)");
		fflush(stdout);
                write_value(power,1);
		write_value(pex,0);
		write_value(pcie,0);
		close(power);
		close(pex);
		close(pcie);
		int t=0;
		for(t=0;t<5;t++) {
			printf(".");
			fflush(stdout);
			sleep(1);
		}
		printf("\n");
		found=1;
        }
	
	if (arguments.on == 1 || arguments.reboot == 1 ) {
		int power;
                int pex;
                int pcie;

		printf( "Powering On");
		if (read_gpio(PGOOD)==1) {
			printf("System already powered on.  Exiting...\n");
			exit(-1);
		}
		
		power=open_gpio_out(POWER);
                pex=open_gpio_out(PEX_PERST_N);
                pcie=open_gpio_out(PCIE_RST_N);

		write_value(power,0);
                write_value(pex,1);
                write_value(pcie,1);

		int done=0;
		int timeout=0;
		while(!done && timeout<5) {
			sleep(1);
			printf( "." );
			if (read_gpio(PGOOD)==1) {
				done=1;
			}
			timeout++;
		}
		if (!done) {
			printf("ERROR:  System will not power on!\n");
			close(power);
			exit(-1);
		}
		int steady=0;
		while(steady<3){   //double check power is settled
			sleep(1);
			printf( "." );
			steady++;
		}
                printf("\nSystem is powered on\n");
		close(power);
                close(pex);
                close(pcie);
		
		found=1;
	}
	if (arguments.boot == 1) {
		int cronus;
 		int fsi_enable;
		int mem_fd;
		void *lpcreg;
		
		mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (mem_fd < 0) {
		  perror("Unable to open /dev/mem");
		  exit(1);
		}

		lpcreg = mmap(NULL, getpagesize(),
			      PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, LPC_BASE);
		if (lpcreg == MAP_FAILED) {
		  perror("Unable to map LPC register memory");
		  exit(1);
		}

		if (arguments.firmware_image) {
		  printf("Booting from %s\n", arguments.firmware_image);
		  boot_firmware_image(mem_fd, arguments.firmware_image);
		  writel(MEM_IMG_BASE | 0xe00, lpcreg+LPC_HICR7);
		} else {
		  writel(FLASH_IMG_BASE | 0xe00, lpcreg+LPC_HICR7);
		}
		writel(0xfe0001ff, lpcreg+LPC_HICR8);

		cronus=open_gpio_out(CRONUS_SEL);
		fsi_enable=open_gpio_out(FSI_ENABLE);
		write_value(cronus,1);  //Set Cronus control to BMC
		//write_value(fsi_enable,1);
		apply_magic_pattern();  //boot system
		//write_value(fsi_enable,0);  //must disable fsi so SBE gets clocks from ref clocks
		//write_value(cronus,0);  //Set Cronus control to FSP2
		//close(cronus);
		//close(fsi_enable);
		found=1;

	}
        if (arguments.cronus == 1){
		int cronus;
 		int fsi_enable;

		cronus=open_gpio_out(CRONUS_SEL);
		fsi_enable=open_gpio_out(FSI_ENABLE);
		write_value(cronus,0);  //Set Cronus control to FSP2
		write_value(fsi_enable,1);
		//close(cronus);
		//close(fsi_enable);
		printf( "Ready for Cronus Work.  Not booting.\n");
		found=1;
        }
        if (arguments.state == 1){
		int status;
		status = read_gpio(PGOOD);
		if (status == 0){
			printf( "System Power On.\n" );
		}else{
			printf( "System Power Off.\n" );
		}
		found=1;
	}
	if (!found) {
		printf ("Not a valid option.\n");
	}
	return(0);
}

