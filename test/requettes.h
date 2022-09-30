#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/ioctl.h>


/*---------------------------------------------------------------------------*/
/* sert pour l'ioctl */
#define MAGIQUE 'N'
/* 0 -> pour changer le numéro de la version courante */
#define CHANGE_VERSION _IOWR(MAGIQUE, 0, char*)
#define RESTOR_VERSION _IOWR(MAGIQUE, 1, char*)
#define RLEASE_VERSION _IOWR(MAGIQUE, 2, char*)
/*---------------------------------------------------------------------------*/