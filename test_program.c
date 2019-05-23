#include <stdio.h>
#include <stdlib.h>

#include "host.h"

int main()
{

    struct host *my_host = host_create();
    host_add_device(my_host, "network");
    host_add_sensor_to_device(my_host, "network", "device-status");
    host_add_sensor_to_device(my_host, "network", "rx-err-cnt");
    host_add_sensor_to_device(my_host, "network", "rx-gbps");
    host_add_sensor_to_device(my_host, "network", "rx-pps");
    host_add_sensor_to_device(my_host, "network", "tx-enabled");
    host_add_sensor_to_device(my_host, "network", "tx-err-cnt");
    host_add_sensor_to_device(my_host, "network", "tx-gbps");
    host_add_sensor_to_device(my_host, "network", "tx-pps");

    host_update_sensor(my_host, "network", "rx-err-cnt", "51", "nominal");

    //printf("Sensor network.rx-err-cnt now reads: %s %s\n", host_get_sensor_value(my_host, 

    printf("0 is ");
    if (0)
        printf("true");
    else
        printf("false");

    printf("\n1 is ");
    if (1)
        printf("true");
    else
        printf("false");

    printf("\n-1 is ");
    if (-1)
        printf("true");
    else
        printf("false");



    host_destroy(my_host);

    return 0;
}

