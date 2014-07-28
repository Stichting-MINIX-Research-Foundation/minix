#ifndef AK4531_H
#define AK4531_H
/* best viewed with tabsize=4 */

#include <minix/drivers.h>
#include <minix/sound.h>

int ak4531_init(u16_t base, u16_t status_reg, u16_t bit, u16_t poll);
int ak4531_get_set_volume(struct volume_level *level, int flag);

#endif
