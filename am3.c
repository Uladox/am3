#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#define NIT_SHORT_NAMES
#include <nit/macros.h>
#include <nit/palloc.h>
#include <nit/list.h>
#include <nit/hmap.h>
#include <nit/gap-buf.h>

#include "am3.h"

#define DEFAULT_CONTI_CODE_SIZE 1024
#define DEFAULT_CONTI_DATA_SIZE 1024
#define DEFAULT_ENV_SEQUENCE 0

static int
func_hmap_compare(const void *entry_key, uint32_t entry_key_size,
		  const void *key, uint32_t key_size);

static void
func_hmap_free(void *key, void *storage);

/* env */

Am3_env *
am3_env_new(Am3_env *up)
{
	Am3_env *env = palloc(env);

	pcheck(env, NULL);
	env->dict = hmap_new(DEFAULT_ENV_SEQUENCE,
			    func_hmap_compare, func_hmap_free);
	pcheck_c(env->dict, NULL, free(env));
	env->up = up;
	env->refs = 1;

	return env;
}


void
am3_env_release(Am3_env *env)
{
	if (!env || --env->refs)
		return;

	hmap_free(env->dict);
	am3_env_release(env->up);
}

Am3_func *
am3_env_get_func(const Am3_env *env, Am3_word word)
{
	return am3_dict_get(env->dict, word);
}

/* conti */

static void
am3_elist_free(Am3_elist *elist)
{
	Am3_elist *tmp;

	delayed_foreach (tmp, elist) {
		am3_env_release(tmp->env);
		free(tmp);
	}
}

/* static void */
/* am3_elist_inc_refs(Am3_elist *elist) */
/* { */
/* 	foreach (elist) */
/* 		++elist->env->refs; */
/* } */

static Am3_elist *
am3_elist_copy(const Am3_elist *elist)
{
	Am3_elist *start;
	Am3_elist **new = &start;

	foreach (elist) {
		*new = palloc(*new);

	        if (!*new) {
			*new = NULL;
			am3_elist_free(start);
			return NULL;
		}

		(*new)->env = elist->env;
		++(*new)->env->refs;
		new = NEXT_REF(*new);
	}

	*new = NULL;

	return start;
}

Am3_conti *
am3_conti_new(Am3_env *env)
{
	Am3_conti *conti = palloc(conti);

	pcheck(conti, NULL);

	if (gap_init(&conti->code, DEFAULT_CONTI_CODE_SIZE)) {
		free(conti);
		return NULL;
	}

        if (gap_init(&conti->data, DEFAULT_CONTI_DATA_SIZE)) {
		gap_dispose(&conti->code);
		free(conti);
		return NULL;
	}

	pcheck_c((conti->elist = palloc(conti->elist)), NULL,
		 (gap_dispose(&conti->data),
		  gap_dispose(&conti->code), free(conti)));

	conti->elist->env = env;
	LIST_CONS(conti->elist, NULL);

	if (env)
		++env->refs;

	return conti;
}

void
am3_conti_free(Am3_conti *conti)
{

	am3_elist_free(conti->elist);
	gap_dispose(&conti->code);
	gap_dispose(&conti->data);
	free(conti);
}

Am3_func *
am3_conti_get_func(const Am3_conti *conti, Am3_word word)
{

	const Am3_elist *dynam = conti->elist;
	const Am3_env *lex;
	Am3_func *value = NULL;

	for (; dynam; dynam = LIST_NEXT(dynam))
		for (lex = dynam->env; lex; lex = lex->up)
			if ((value = am3_env_get_func(lex, word)))
				return value;

	return value;
}

int
am3_conti_apply_clos(Am3_conti *conti, const Am3_func *func)
{
	Am3_elist *elist = palloc(elist);

	pcheck(elist, 1);
	elist->env = func->env;
	++elist->env->refs;
	LIST_CONS(elist, conti->elist);

	pcheck_c(am3_word_write(&conti->code, AM3_FUNC_END) ||
		 gap_gap_put(&conti->code, func->val.words),
		 1, free(elist));
	conti->elist = elist;

	return 0;
}

int
am3_conti_apply_conti(Am3_conti *des, const Am3_conti *src)
{
	if (gap_replicate(&des->code, &src->code))
		return 1;

	if (gap_replicate(&des->data, &src->data))
		return 1;

	am3_elist_free(des->elist);

	if (!(des->elist = am3_elist_copy(src->elist)))
		return 1;

	return 0;
}

int
am3_conti_apply_word(Am3_conti *conti, Am3_word word)
{

	Am3_func *func;
	Am3_elist *elist;

	switch (word) {
	case AM3_FUNC_END:
		elist = conti->elist;
		conti->elist = LIST_NEXT(elist);
		am3_env_release(elist->env);
		free(elist);
		return 0;
	case AM3_STACK_PRINT:
		am3_print_stack(&conti->data);
		return 0;
	}

	pcheck((func = am3_conti_get_func(conti, word)), 1);

	switch (func->type) {
	case AM3_PRIM:
		return func->val.c_func(func, &conti->data);
	case AM3_CLOS:
		return am3_conti_apply_clos(conti, func);
	case AM3_CONTI:
		return am3_conti_apply_conti(conti, func->val.conti);
	}

	return 1;
}

/* func */

static int
func_hmap_compare(const void *entry_key, uint32_t entry_key_size,
		  const void *key, uint32_t key_size)
{
	(void) entry_key_size;
	(void) key_size;

	return *(Am3_word *) entry_key == *(Am3_word *) key;
}

static void
func_hmap_free(void *key, void *storage)
{
	free(key);
	am3_func_release(storage);
}

/* Am3_func * */
/* am3_func_new_clos() */

void
am3_func_release(Am3_func *func)
{
	if (--func->refs)
		return;

	am3_env_release(func->env);

	switch (func->type) {
	case AM3_PRIM:
		break;
	case AM3_CLOS:
		gap_dispose(func->val.words);
		free(func->val.words);
	        break;
	case AM3_CONTI:
		am3_conti_free(func->val.conti);
		break;
	}
		free(func);
}

Am3_func *
am3_func_get_func(const Am3_func *func, Am3_word word)
{
	return am3_env_get_func(func->env, word);
}

/* other */
int
am3_word_write(Nit_gap *gap, Am3_word word)
{
	return gap_write(gap, &word, sizeof(word));
}

void
am3_print_stack(Nit_gap *stack)
{
	int count = 0;
	int val = stack->start / sizeof(Am3_word);

	printf("#stack{ ");

	for (; count < val; ++count)
		printf("[%" PRIu32 "] ", ((Am3_word *) stack->bytes)[count]);

	count = stack->end + 1;
	val = stack->size;

	for (; count < val; ++count)
		printf("[%" PRIu32 "] ", ((Am3_word *) stack->bytes)[count]);

	printf("}\n");
}

Am3_func *
am3_dict_get(const Nit_hmap *map, Am3_word word)
{
	return hmap_get(map, &word, sizeof(word));
}
