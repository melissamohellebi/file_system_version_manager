#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "requettes.h"
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char ** argv)
{
    if(argc < 3){
        printf("Il faut un nom de fichier suivi de la version voulue\n");
        return 0;
    }
    int fd = open (argv[1], O_RDWR);
    long ret_val = ioctl(fd, RESTOR_VERSION, argv[2]);
    return 0;
}
