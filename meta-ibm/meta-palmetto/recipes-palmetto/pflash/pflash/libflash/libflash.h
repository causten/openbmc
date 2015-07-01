#ifndef __LIBFLASH_H
#define __LIBFLASH_H

#include <stdint.h>
#include <stdbool.h>

#ifndef FL_INF
#define FL_INF(fmt...) do { printf(fmt); } while(0)
#endif

#ifndef FL_DBG
#define FL_DBG(fmt...) do { if (libflash_debug) printf(fmt); } while(0)
#endif

#ifndef FL_ERR
#define FL_ERR(fmt...) do { printf(fmt); } while(0)
#endif

extern bool libflash_debug;

/* API status/return:
 *
 *  <0 = flash controller errors passed through, 
 *  0  = success
 *  >0 = libflash error
 */

#define FLASH_ERR_MALLOC_FAILED		1
#define FLASH_ERR_CHIP_UNKNOWN		2
#define FLASH_ERR_PARM_ERROR		3
#define FLASH_ERR_ERASE_BOUNDARY	4
#define FLASH_ERR_WREN_TIMEOUT		5
#define FLASH_ERR_WIP_TIMEOUT		6
#define FLASH_ERR_BAD_PAGE_SIZE		7
#define FLASH_ERR_VERIFY_FAILURE	8
#define FLASH_ERR_4B_NOT_SUPPORTED	9
#define FLASH_ERR_CTRL_CONFIG_MISMATCH	10
#define FLASH_ERR_CHIP_ER_NOT_SUPPORTED	11
#define FLASH_ERR_CTRL_CMD_UNSUPPORTED	12
#define FLASH_ERR_CTRL_TIMEOUT          13

/* Flash chip, opaque */
struct flash_chip;
struct spi_flash_ctrl;

int flash_init(struct spi_flash_ctrl *ctrl, struct flash_chip **flash);
void flash_exit(struct flash_chip *chip);

int flash_get_info(struct flash_chip *chip, const char **name,
		   uint32_t *total_size, uint32_t *erase_granule);

/* libflash sets the 4b mode automatically based on the flash
 * size and controller capabilities but it can be overriden
 */
int flash_force_4b_mode(struct flash_chip *chip, bool enable_4b);

int flash_read(struct flash_chip *c, uint32_t pos, void *buf, uint32_t len);
int flash_erase(struct flash_chip *c, uint32_t dst, uint32_t size);
int flash_write(struct flash_chip *c, uint32_t dst, const void *src,
		uint32_t size, bool verify);
int flash_smart_write(struct flash_chip *c, uint32_t dst, const void *src,
		      uint32_t size);

/* chip erase may not be supported by all chips/controllers, get ready
 * for FLASH_ERR_CHIP_ER_NOT_SUPPORTED
 */
int flash_erase_chip(struct flash_chip *c);

#endif /* __LIBFLASH_H */
