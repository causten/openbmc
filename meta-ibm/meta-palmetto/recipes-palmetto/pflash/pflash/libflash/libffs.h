#ifndef __LIBFFS_H
#define __LIBFFS_H

#include <libflash/libflash.h>
#include <libflash/ffs.h>

/* FFS handle, opaque */
struct ffs_handle;

/* Error codes:
 *
 * < 0 = flash controller errors
 *   0 = success
 * > 0 = libffs / libflash errors
 */
#define FFS_ERR_BAD_MAGIC	100
#define FFS_ERR_BAD_VERSION	101
#define FFS_ERR_BAD_CKSUM	102
#define FFS_ERR_PART_NOT_FOUND	103

int ffs_open_flash(struct flash_chip *chip, uint32_t offset,
		   uint32_t max_size, struct ffs_handle **ffs);

/* TODO
int ffs_open_image(void *image, uint32_t size, struct ffs_handle **ffs);
*/

void ffs_close(struct ffs_handle *ffs);

int ffs_lookup_part(struct ffs_handle *ffs, const char *name,
		    uint32_t *part_idx);

int ffs_part_info(struct ffs_handle *ffs, uint32_t part_idx,
		  char **name, uint32_t *start,
		  uint32_t *total_size, uint32_t *act_size);

int ffs_update_act_size(struct ffs_handle *ffs, uint32_t part_idx,
			uint32_t act_size);


#endif /* __LIBFFS_H */
