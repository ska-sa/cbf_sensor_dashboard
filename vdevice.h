#ifndef _VDEVICE_H_
#define _VDEVICE_H_
#include "device.h"

struct vdevice;

struct vdevice *vdevice_create(char *new_name, struct device **device_list, unsigned int number_of_engines);
void vdevice_destroy(struct vdevice *this_vdevice);

char *vdevice_get_status(struct vdevice *this_vdevice);

#endif
