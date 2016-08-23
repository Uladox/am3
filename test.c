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
	Am3_tsan *tsan = am3_tsan_new(NULL);



	am3_word_write(123456, &tsan->data);
	am3_word_write(9076126, &tsan->data);
	am3_apply_word(AM3_STACK_PRINT, tsan);
	am3_tsan_free(tsan);
}
