#include <stdio.h>

#include <stddef.h>
#include <stdint.h>
#include <nit/list.h>
#include <nit/hmap.h>
#include <nit/gap-buf.h>


#include "am3.h"

int
main(int argc, char *argv[])
{
	Am3_conti *conti = am3_conti_new(NULL);



	am3_word_write(123456, &conti->data);
	am3_word_write(9076126, &conti->data);
	am3_conti_apply_word(AM3_STACK_PRINT, conti);
	am3_conti_free(conti);
}
