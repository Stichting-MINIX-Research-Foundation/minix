#ifndef __EDID_H
#define __EDID_H

#include <stdint.h>                                                             
#include <dev/videomode/videomode.h>                                            
#include <dev/videomode/edidvar.h>                                              
#include <dev/videomode/edidreg.h>

int fb_edid_args_parse(void);
int fb_edid_read(int minor, struct edid_info *info);

#endif /* __EDID_H */
