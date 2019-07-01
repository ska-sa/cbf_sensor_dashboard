#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "web.h"
#include "http.h"

#define BUF_SIZE 1024

int html_send_header(struct web_client *client)
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

int send_html_body_open(struct web_client *client)
{
    int r;
    char html_body_open[] = "<body onload=\"JavaScript:timedRefresh(5000);\">\n";
    //char html_body_open[] = "<body>\n";
    r = add_to_buffer(client, html_body_open);
    return r;
}

int send_html_body_close(struct web_client *client)
{
    int r;
    char html_body_close[] = "</body>\n</html>\n";
    r = add_to_buffer(client, html_body_close);
    return r;
}

int send_html_section_start(struct web_client *client)
{
    int r;
    char html_section_start[] = "<section>\n";
    r = add_to_buffer(client, html_section_start);
    return r;
}


int send_html_section_end(struct web_client *client)
{
    int r;
    char html_section_end[] = "</section>\n";
    r = add_to_buffer(client, html_section_end);
    return r;
}

int send_html_paragraph(struct web_client *client, char *line)
{
    int r;
    char *paragraph;
    char format[] = "<p>%s</p>\n";
    size_t needed = snprintf(NULL, 0, format, line) + 1;
    paragraph = malloc(needed);
    sprintf(paragraph, format, line);
    r = add_to_buffer(client, paragraph);
    free(paragraph);
    return r;
}

int send_html_table_start(struct web_client *client)
{
    int r;
    char html_table_start[] = "<table>\n";
    r = add_to_buffer(client, html_table_start);
    return r;
}


int send_html_table_end(struct web_client *client)
{
    int r;
    char html_table_end[] = "</table>\n";
    r = add_to_buffer(client, html_table_end);
    return r;
}

int send_html_table_arraylist_header(struct web_client *client)
{
    int r;
    char html_table_header[] = "<tr>\n<th>Array Name</th><th>Monitor port</th><th>Multicast groups</th>\n</tr>";
    r = add_to_buffer(client, html_table_header);
    return r;
}

int send_html_table_arraylist_row(struct web_client *client, struct cmc_array *array)
{
    int r;
    char *html_table_row;
    char format[] = "<tr><td><a href=\"%s\">%s</a></td><td>%d</td><td>%s</td>\n</tr>\n";
    size_t needed = snprintf(NULL, 0, format, array->name, array->name, array->monitor_port, array->multicast_groups) + 1;
    html_table_row = malloc(needed);
    sprintf(html_table_row, format, array->name, array->name, array->monitor_port, array->multicast_groups);
    r = add_to_buffer(client, html_table_row);
    free(html_table_row);
    return r;
}

int send_html_table_sensor_row(struct web_client *client, struct fhost *fhost, struct xhost *xhost)
{
    if (client == NULL || fhost == NULL || xhost == NULL)
    {
        char message[] = "Something went wrong. Please refresh later.";
        int r = add_to_buffer(client, message);
        return r;
    }
    char format[] = "<tr><td>f%02d %s</td>"
        "<td><button class=\"%s\">netw-rx</button></td>"
        "<td><button class=\"%s\">spead-rx</button></td>"
        "<td><button class=\"%s\">netw-reor</button></td>"
        "<td><button class=\"%s\">dig</button></td>"
        "<td><button class=\"%s\">sync</button></td>"
        "<td><button class=\"%s\">cd</button></td>"
        "<td><button class=\"%s\">pfb</button></td>"
        "<td><button class=\"%s\">quant</button></td>"
        "<td><button class=\"%s\">ct</button></td>"
        "<td><button class=\"%s\">spead-tx</button></td>"
        "<td><button class=\"%s\">netw-tx</button></td>"
        "<td>x%02d %s</td>"
        "<td><button class=\"%s\">netw-rx</button></td>"
        "<td><button class=\"%s\">netw-reor</button></td>"
        "<td><button class=\"%s\">miss-pkt</button></td>"
        "<td><button class=\"%s\">spead-rx</button></td>"
        "<td><button class=\"%s\">bram-reord</button></td>"
        "<td><button class=\"%s\">vacc</button></td>"
        "<td><button class=\"%s\">spead-tx</button></td>"
        "<td><button class=\"%s\">netw-tx</button></td></tr>\n";
    size_t needed = snprintf(NULL, 0, format,
            fhost->host_number, fhost->hostname,
            fhost->netw_rx,
            fhost->spead_rx,
            fhost->netw_reor,
            fhost->dig,
            fhost->sync,
            fhost->cd,
            fhost->pfb,
            fhost->quant,
            fhost->ct,
            fhost->spead_tx,
            fhost->netw_tx,
            xhost->host_number, xhost->hostname,
            xhost->netw_rx,
            xhost->netw_reor,
            xhost->miss_pkt,
            xhost->spead_rx,
            xhost->bram_reord,
            xhost->vacc,
            xhost->spead_tx,
            xhost->netw_tx) + 1;
    char *html_table_row = malloc(needed);
    sprintf(html_table_row, format, fhost->host_number, fhost->hostname,
            fhost->netw_rx,
            fhost->spead_rx,
            fhost->netw_reor,
            fhost->dig,
            fhost->sync,
            fhost->cd,
            fhost->pfb,
            fhost->quant,
            fhost->ct,
            fhost->spead_tx,
            fhost->netw_tx,
            xhost->host_number,
            xhost->hostname,
            xhost->netw_rx,
            xhost->netw_reor,
            xhost->miss_pkt,
            xhost->spead_rx,
            xhost->bram_reord,
            xhost->vacc,
            xhost->spead_tx,
            xhost->netw_tx);
    int r = add_to_buffer(client, html_table_row);
    free(html_table_row);
    return r;
}

int send_quad(struct web_client *client)
{
    int r;
    char quadrow[] = "<tr><td><iframe src=\"array_0.i0\"></iframe></td><td><iframe src=\"array_1.i0\"></iframe></td></tr>\n<tr><td><iframe src=\"array_2.i0\"></iframe></td><td><iframe src=\"array_3.i0\"></iframe></td></tr>\n";
    r = add_to_buffer(client, quadrow);
    return r;
}

