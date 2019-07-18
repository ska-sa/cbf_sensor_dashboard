#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "web.h"
#include "html.h"

#define BUF_SIZE 1024

char *html_doctype()
{
    return "<!DOCTYPE html>\n";
}


char *html_open()
{
    return "<html>\n";
}


char *html_head_open()
{
    return "<head><meta charset=\"utf-8\">\n";
}


char *html_title(char *title)
{
    char format[] = "<title>%s</title>\n";
    ssize_t needed = snprintf(NULL, 0, format, title) + 1;
    char *the_title = malloc((size_t) needed);
    int r = sprintf(the_title, format, title);
    if (r<0)
        return NULL;
    return the_title;
}


char *html_script()
{
    return "<script>\nfunction timedRefresh(timeoutPeriod) { setTimeout(\"location.reload(true);\", timeoutPeriod); }\n</script>\n";
}


char *html_style()
{
    FILE *style_file = fopen("html/styles.css", "r");
    if (style_file == NULL)
    {
        perror("fopen(html/styles.css)");
        return NULL;
    }

    char buffer[BUF_SIZE];
    char *result;
    char *return_value = strdup("<style>\n");
    for (result = fgets(buffer, BUF_SIZE, style_file); result != NULL; result = fgets(buffer, BUF_SIZE, style_file))
    {
        return_value = realloc(return_value, strlen(return_value) + strlen(buffer) + 1);
        strcat(return_value, buffer);
    }
    fclose(style_file);
    return_value = realloc(return_value, strlen(return_value) + strlen("</style>\n") + 1);
    strcat(return_value, "</style>\n");
    return return_value;
}


char *html_head_close()
{
    return "</head>\n";
}


char *html_body_open()
{
    return "<body onload=\"JavaScript:timedRefresh(5000)\">\n";
}


char *html_body_close()
{
    return "</body>\n";
}


char *html_close()
{
    return "</html>";
}
