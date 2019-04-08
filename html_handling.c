#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "array_handling.h"

#define BUF_SIZE 1024

int send_http_ok(int socket_fd)
{
    int r;
    char http_ok_message[] = "HTTP/1.1 200 OK\n\n";
    r = write(socket_fd, http_ok_message, strlen(http_ok_message));

    return r;
}


int send_html_header(int socket_fd)
{
    FILE *html_header_file;
    html_header_file = fopen("head.html", "r");
    if (html_header_file == NULL)
        return -1;

    int r;
    char *res;
    char line[BUF_SIZE];

    do {
        res = fgets(line, BUF_SIZE, html_header_file);
        /* TODO - this is where to add the parsing. So we'll need to add styles and scripts, and probably a specific header. */
        if (res != NULL)
        {
            r = write(socket_fd, line, strlen(line));
            if (r < 1)
                return -1;
        }
    } while (res != NULL);

    printf("%d\n", html_header_file);

    fclose(html_header_file);

    return 1;
}

int send_html_body_open(int socket_fd)
{
    int r;
    char html_body_open[] = "<body onload=\"JavaScript:timedRefresh(5000);\">\n";
    r = write(socket_fd, html_body_open, strlen(html_body_open));
    return r;
}

int send_html_body_close(int socket_fd)
{
    int r;
    char html_body_close[] = "</body>\n</html>\n";
    r = write(socket_fd, html_body_close, strlen(html_body_close));
    return r;
}

int send_html_section_start(int socket_fd)
{
    int r;
    char html_section_start[] = "<section>\n";
    r = write(socket_fd, html_section_start, strlen(html_section_start));
    return r;
}


int send_html_section_end(int socket_fd)
{
    int r;
    char html_section_end[] = "</section>\n";
    r = write(socket_fd, html_section_end, strlen(html_section_end));
    return r;
}

int send_html_table_start(int socket_fd)
{
    int r;
    char html_table_start[] = "<table>\n";
    r = write(socket_fd, html_table_start, strlen(html_table_start));
    return r;
}


int send_html_table_end(int socket_fd)
{
    int r;
    char html_table_end[] = "</table>\n";
    r = write(socket_fd, html_table_end, strlen(html_table_end));
    return r;
}

int send_html_table_arraylist_header(int socket_fd)
{
    int r;
    char html_table_header[] = "<tr>\n<th>Array Name</th><th>Monitor port</th><th>Multicast groups</th>\n</tr>";
    r = write(socket_fd, html_table_header, strlen(html_table_header));
    return r;
}

int send_html_table_arraylist_row(int socket_fd, struct cmc_array *array)
{
    int r;
    char *html_table_row;
    size_t needed = snprintf(NULL, 0, "<tr><td>%s</td><td>%d</td><td>%s</td>\n</tr>\n", array->name, array->monitor_port, array->multicast_groups) + 1;
    html_table_row = malloc(needed);
    sprintf(html_table_row, "<tr><td>%s</td><td><a href=\"%d\">%d</a></td><td>%s</td>\n</tr>\n", array->name, array->monitor_port, array->monitor_port, array->multicast_groups);
    r = write(socket_fd, html_table_row, strlen(html_table_row));
    free(html_table_row);
    return r;
}


