#ifndef _VDEVICE_H_
#define _VDEVICE_H_
#include "engine.h"


struct vdevice;

struct vdevice *vdevice_create(char *new_name, struct engine ***engine_list, size_t *number_of_engines);
void vdevice_destroy(struct vdevice *this_vdevice);

char *vdevice_get_name(struct vdevice *this_vdevice);
char *vdevice_get_status(struct vdevice *this_vdevice);

char *vdevice_html_summary(struct vdevice *this_vdevice);
#endif
