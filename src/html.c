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
    return "<script>\nfunction timedRefresh(timeoutPeriod) { setTimeout(\"location.reload(true);\", timeoutPeriod); }\n</script>";
}


char *html_head_close()
{
    return "</head>\n";
}


char *html_body_open()
{
    return "<body>\n";
}


char *html_body_close()
{
    return "</body>\n";
}


char *html_close()
{
    return "</html>";
}
