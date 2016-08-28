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
	Am3_word word = 123456;

	am3_word_write(&conti->data, word);
	am3_word_write(&conti->data, word);
	am3_conti_apply_word(conti, AM3_STACK_PRINT);
	am3_conti_apply_word(conti, AM3_COPY_CONTI);
	am3_func_print(am3_conti_get_func(conti, word));
	am3_conti_apply_word(conti, AM3_STACK_PRINT);
	am3_conti_free(conti);
}
