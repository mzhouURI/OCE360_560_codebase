#include <stdio.h>
#include "pico/stdlib.h"

//regular timer
#include "pico/time.h"


int main()
{
    stdio_init_all();
    
    //loop
    while (true) {
        printf("Hello, world!\n");
        
        //simple time stuff
        uint32_t ms_since_boot = to_ms_since_boot(get_absolute_time()); 
        float sec_since_boot = (float)ms_since_boot/1000.0;

        printf("Uptime: %lf, \n", sec_since_boot);

        printf("Hex:  %x\n", (unsigned long) sec_since_boot);
        printf("Int:  %d\n", (unsigned long) sec_since_boot);
        printf("char:  %c\n", (unsigned long) sec_since_boot);


        sleep_ms(1000);

        
    }
}
