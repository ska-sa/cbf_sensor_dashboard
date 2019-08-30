#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vdevice.h"
#include "engine.h"
#include "verbose.h"

struct vdevice {
    char *name;
    struct engine ***engine_list;
    size_t *number_of_engines;
    char *status; /*virtual sensor doesn't have a value, just a status.*/
};


int vdevice_update_status(struct vdevice *this_vdevice)
{
    /*Logic: loop through engines' sensors three times, each having a higher priority than the previous.
     * If any of them are nominal, it'll get written to the device status the first time.
     * On subsequent loops, a value of "warn" or "error" will overwrite the nominal one.
     * I can't personally think of a better way to do this.
     */
    if (this_vdevice->status != NULL)
    {
        free(this_vdevice->status);
        this_vdevice->status = NULL;
    }
    this_vdevice->status = strdup("unknown"); /*If none of the below triggers, this will remain.*/
    size_t i;
    verbose_message(BORING, "Updating vdevice status for %s\n", vdevice_get_name(this_vdevice));
    for (i = 0; i < *(this_vdevice->number_of_engines); i++)
    {
        if (!strcmp(engine_get_sensor_status((*this_vdevice->engine_list)[i], this_vdevice->name, "device-status"), "nominal"))
        {
            if (this_vdevice->status != NULL)
                free(this_vdevice->status);
            this_vdevice->status = strdup("nominal");
        }
    }
    for (i = 0; i < *(this_vdevice->number_of_engines); i++)
    {
        if (!strcmp(engine_get_sensor_status((*this_vdevice->engine_list)[i], this_vdevice->name, "device-status"), "warn"))
        {
            if (this_vdevice->status != NULL)
                free(this_vdevice->status);
            this_vdevice->status = strdup("warn");
        }
    }
    for (i = 0; i < *(this_vdevice->number_of_engines); i++)
    {
        if (!strcmp(engine_get_sensor_status((*this_vdevice->engine_list)[i], this_vdevice->name, "device-status"), "error"))
        {
            if (this_vdevice->status != NULL)
                free(this_vdevice->status);
            this_vdevice->status = strdup("error");
        }
    }
    return 0;
}


struct vdevice *vdevice_create(char *new_name, struct engine ***engine_list, size_t *number_of_engines)
{
    struct vdevice *new_vdevice = malloc(sizeof(*new_vdevice));
    if (new_vdevice != NULL)
    {
        new_vdevice->name = strdup(new_name);
        new_vdevice->engine_list = engine_list;
        new_vdevice->number_of_engines = number_of_engines;
        new_vdevice->status = strdup("unknown");
        //vdevice_update_status(new_vdevice); //possibly not really needed.
    }
    return new_vdevice;
}


void vdevice_destroy(struct vdevice *this_vdevice)
{
    /* Not going to free the engine list because it will be freed
     * by the host that contains it.
     */
    if (this_vdevice != NULL)
        free(this_vdevice);
}


char *vdevice_get_name(struct vdevice *this_vdevice)
{
    return this_vdevice->name;
}


char *vdevice_get_status(struct vdevice *this_vdevice)
{
    vdevice_update_status(this_vdevice);
    return this_vdevice->status;
}


char *vdevice_html_summary(struct vdevice *this_vdevice)
{
    char format[] = "<td><button class=\"%s\">%s</button></td>";
    ssize_t needed = snprintf(NULL, 0, format, vdevice_get_status(this_vdevice), this_vdevice->name) + 1;
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, vdevice_get_status(this_vdevice), this_vdevice->name);
    return html_summary;
}
