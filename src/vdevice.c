#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include "vdevice.h"
#include "engine.h"

/// A struct to represent a "virtual" (compound) device - consisting of a number of identical devices in 
/// engines co-existing on the same host.
struct vdevice {
    /// The name of the vdevice
    char *name;
    /// Pointer to a list of engines.
    struct engine ***engine_list;
    /// Number of engines that share a host.
    size_t *number_of_engines;
    /// Status of the vdevice. Note that the vdevice has no (directly) underlying sensors, it only queries the "device-status" sensors for the corresponding actual devices in the underlying engines. This is updated each time the vdevice's status is retrieved. The virtual sensor doesn't have a value, just a status.
    char *status; 
};


/**
 * \fn      static int vdevice_update_status(struct vdevice *this_vdevice)
 * \details Query the corresponding actual devices in each engine on the host. The vdevice status is the "worst" status of the corresponding
 *          actual devices.
 *          Function is static because it's only used internally to this object.
 * \param   this_vdevice A pointer to the vdevice to be queried.
 * \return  Function in its current state should always return zero, indicating success.
 */
static int vdevice_update_status(struct vdevice *this_vdevice)
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

/**
 * \fn      struct vdevice *vdevice_create(char *new_name, struct engine ***engine_list, size_t *number_of_engines)
 * \details Allocate memory for a vdevice object, connect to the engines which it needs to watch.
 * \param   new_name A name for the vdevice to be created.
 * \param   engine_list A pointer to a list of pointers to engine objects.
 * \param   number_of_engines The number of engines in the list.
 * \return  A newly allocated pointer to the newly-created vdevice object.
 */
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


/**
 * \fn      void vdevice_destroy(struct vdevice *this_vdevice)
 * \details Free the memory associated with the vdevice object.
 * \param   this_vdevice A pointer to the vdevice to be destroyed.
 * \return  void
 */
void vdevice_destroy(struct vdevice *this_vdevice)
{
    /* Not going to free the engine list because it will be freed
     * by the host that contains it.
     */
    if (this_vdevice != NULL)
    {
        free(this_vdevice->name);
        free(this_vdevice->status);
        free(this_vdevice);
    }
}

/**
 * \fn      char *vdevice_get_name(struct vdevice *this_vdevice)
 * \details Get the name of the given vdevice.
 * \param   this_vdevice A pointer to the vdevice to be queried.
 * \return  A pointer to the name string of the vdevice. The char pointer
 *          is not newly allocated and therefore must not be free'd.
 */
char *vdevice_get_name(struct vdevice *this_vdevice)
{
    return this_vdevice->name;
}


/**
 * \fn      char *vdevice_get_status(struct vdevice *this_device)
 * \details Query the vdevice for its latest status.
 * \param   this_vdevice A pointer to the vdevice to be queried.
 * \return  A pointer to the status string of the vdevice. The char* is
 *          not newly allocated so therefore must not be free'd.
 */
char *vdevice_get_status(struct vdevice *this_vdevice)
{
    vdevice_update_status(this_vdevice);
    return this_vdevice->status;
}


/**
 * \fn      char *vdevice_html_summary(struct vdevice *this_vdevice)
 * \details Get an HTML summary of the vdevice. This is an HTML5 td with the class set to the vdevice's status,
 *          so that the higher-level CSS can render the button appropriately.
 * \param   this_vdevice A pointer to the vdevice.
 * \return  A newly allocated string containing the HTML summary of the vdevice.
 */
char *vdevice_html_summary(struct vdevice *this_vdevice)
{
    char format[] = "<td class=\"%s\">%s</td>";
    ssize_t needed = snprintf(NULL, 0, format, vdevice_get_status(this_vdevice), this_vdevice->name) + 1;
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, vdevice_get_status(this_vdevice), this_vdevice->name);
    return html_summary;
}
