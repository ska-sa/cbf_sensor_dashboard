#ifndef _VDEVICE_H_
#define _VDEVICE_H_
#include "engine.h"

/**
 * \file  vdevice.h
 * \brief The vdevice type stores the status of a compound device on the FPGA reported by the corr2_sensor_servelet.
 *        The vdevice aggregates the "device-status" sensors of identically-named devices on all the engines occupying
 *        a given host.
 */

struct vdevice;

struct vdevice *vdevice_create(char *new_name, struct engine ***engine_list, size_t *number_of_engines);
void vdevice_destroy(struct vdevice *this_vdevice);

char *vdevice_get_name(struct vdevice *this_vdevice);
char *vdevice_get_status(struct vdevice *this_vdevice);

char *vdevice_html_summary(struct vdevice *this_vdevice);
#endif
