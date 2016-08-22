#include <stddef.h>
#include <stdint.h>

#define NIT_SHORT_NAMES
#include <nit/list.h>
#include <nit/hmap.h>
#include <nit/gap-buf.h>

#include "am3.h"

/* int */
/* am3_eval_func(Am3_func *func, Nit_gap *stack) */
/* { */
/* 	switch (func->type) { */
/* 	case AM3_PRIM: */
/* 		return func->c_func(stack); */
/* 	case AM3_COMP: */
/* 	} */
/* } */

/* int */
/* am3_eval_word(Am3_word word, Am3_func *env, Nit_gap *stack) */
/* { */
/* 	return am3_eval_func(stack); */
/* } */

int
am3_eval_word(Am3_word word, Am3_tsan *tsan)
{
	return 0;
}
