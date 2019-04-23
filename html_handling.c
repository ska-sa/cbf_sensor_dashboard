#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "array_handling.h"
#include "http_handling.h"

#define BUF_SIZE 1024

int send_html_header(struct webpage_client *client)
{
    FILE *html_header_file;
    html_header_file = fopen("head.html", "r");
    if (html_header_file == NULL)
    {
        perror("fopen()");
        fprintf(stderr, "couldn't open head.html\n");
        return -1;
    }

    int r;
    char *res;
    char line[BUF_SIZE];

    if (!client)
    {
        printf("client not allocated!\n");
        perror("send_html_header()");
        return -1;
    }

    do {
        res = fgets(line, BUF_SIZE, html_header_file);
        /* TODO - this is where to add the parsing. So we'll need to add styles and scripts, and probably a specific header. */
        if (res != NULL)
        {
            r = add_to_buffer(client, line);
            if (r < 0)
                return r;
        }
    } while (res != NULL);

    fclose(html_header_file);

    return 0;
}

int send_html_body_open(struct webpage_client *client)
{
    int r;
    //char html_body_open[] = "<body onload=\"JavaScript:timedRefresh(5000);\">\n";
    char html_body_open[] = "<body>\n";
    r = add_to_buffer(client, html_body_open);
    return r;
}

int send_html_body_close(struct webpage_client *client)
{
    int r;
    char html_body_close[] = "</body>\n</html>\n";
    r = add_to_buffer(client, html_body_close);
    return r;
}

int send_html_section_start(struct webpage_client *client)
{
    int r;
    char html_section_start[] = "<section>\n";
    r = add_to_buffer(client, html_section_start);
    return r;
}


int send_html_section_end(struct webpage_client *client)
{
    int r;
    char html_section_end[] = "</section>\n";
    r = add_to_buffer(client, html_section_end);
    return r;
}

int send_html_paragraph(struct webpage_client *client, char *line)
{
    int r;
    char *paragraph;
    size_t needed = snprintf(NULL, 0, "<p>%s</p>\n", line) + 1;
    paragraph = malloc(needed);
    sprintf(paragraph, "<p>%s</p>\n", line);
    r = add_to_buffer(client, paragraph);
    free(paragraph);
    return r;
}

int send_html_table_start(struct webpage_client *client)
{
    int r;
    char html_table_start[] = "<table>\n";
    r = add_to_buffer(client, html_table_start);
    return r;
}


int send_html_table_end(struct webpage_client *client)
{
    int r;
    char html_table_end[] = "</table>\n";
    r = add_to_buffer(client, html_table_end);
    return r;
}

int send_html_table_arraylist_header(struct webpage_client *client)
{
    int r;
    char html_table_header[] = "<tr>\n<th>Array Name</th><th>Monitor port</th><th>Multicast groups</th>\n</tr>";
    r = add_to_buffer(client, html_table_header);
    return r;
}

int send_html_table_arraylist_row(struct webpage_client *client, struct cmc_array *array)
{
    int r;
    char *html_table_row;
    size_t needed = snprintf(NULL, 0, "<tr><td><a href=\"%s\">%s</a></td><td>%d</td><td>%s</td>\n</tr>\n", array->name, array->name, array->monitor_port, array->multicast_groups) + 1;
    html_table_row = malloc(needed);
    sprintf(html_table_row, "<tr><td><a href=\"%s\">%s</a></td><td>%d</td><td>%s</td>\n</tr>\n", array->name, array->name, array->monitor_port, array->multicast_groups);
    r = add_to_buffer(client, html_table_row);
    free(html_table_row);
    return r;
}

int send_html_table_sensor_row(struct webpage_client *client, struct fhost *fhost, struct xhost *xhost)
{
    if (client == NULL || fhost == NULL || xhost == NULL)
    {
        size_t needed = snprintf(NULL, 0, "Something went wrong. Please refresh later.") + 1;
        char *message = malloc(needed);
        sprintf(message, "%s", "Something went wrong. Please refresh later.");
        int r = add_to_buffer(client, message);
        free(message);
        return r;
    }
    size_t needed = snprintf(NULL, 0, "<tr><td>f%02d %s</td><td><button class=\"%s\">device-status</button></td></tr>\n", fhost->host_number, fhost->hostname, fhost->device_status) + 1;
    char *html_table_row = malloc(needed);
    sprintf(html_table_row, "<tr><td>f%02d %s</td><td><button class=\"%s\">device-status</button></td></tr>\n", fhost->host_number, fhost->hostname, fhost->device_status);
    int r = add_to_buffer(client, html_table_row);
    free(html_table_row);
    return r;
}

