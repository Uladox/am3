#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#define NIT_SHORT_NAMES
#include <nit/macros.h>
#include <nit/palloc.h>
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
am3_word_write(Am3_word word, Nit_gap *gap)
{
	return gap_write(gap, &word, sizeof(word));
}

Am3_func *
am3_dict_get(Am3_word word, const Nit_hmap *map)
{
	return hmap_get(map, &word, sizeof(word));
}

static inline Am3_func *
am3_env_get_func(Am3_word word, const Am3_env *env)
{
	return am3_dict_get(word, env->dict);
}

static inline Am3_func *
am3_clos_get_func(Am3_word word, const Am3_clos *clos)
{
	return am3_env_get_func(word, clos->env);
}

Am3_func *
am3_tsan_get_func(Am3_word word, const Am3_tsan *tsan)
{

	const Am3_elist *dynam = tsan->elist;
	const Am3_env *lex;
	Am3_func *value = NULL;

	for (; dynam; dynam = LIST_NEXT(dynam))
		for (lex = dynam->env; lex; lex = lex->up)
			if ((value = am3_env_get_func(word, lex)))
				return value;

	return value;
}

int
am3_apply_word(Am3_word word, Am3_tsan *tsan)
{
	Am3_func *func;
	Am3_elist *elist;

	switch (word) {
	case AM3_FUNC_END:
		elist = tsan->elist;
		tsan->elist = LIST_NEXT(elist);
		free(elist);
		return 0;
	}

	pcheck((func = am3_tsan_get_func(word, tsan)), 1);
	switch (func->type) {
	case AM3_PRIM:
		return func->val.c_func(&tsan->data);
	case AM3_COMP:
		elist = palloc(elist);
		pcheck(elist, 1);
		elist->env = func->val.clos.env;
		LIST_CONS(elist, tsan->elist);

		pcheck_c(am3_word_write(AM3_FUNC_END, &tsan->code) ||
			 gap_gap_put(&tsan->code, &func->val.clos.words),
			 1, free(elist));
		tsan->elist = elist;
	}

	return 0;
}
