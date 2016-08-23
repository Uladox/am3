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

#define DEFAULT_TSAN_CODE_SIZE 1024
#define DEFAULT_TSAN_DATA_SIZE 1024
#define DEFAULT_ENV_SEQUENCE 0

void
am3_env_release(Am3_env *env)
{
	if (!env || --env->refs)
		return;

	hmap_free(env->dict);
	am3_env_release(env->up);
}

void
am3_func_release(Am3_func *func)
{
	if (--func->refs)
		return;

	switch (func->type) {
	case AM3_PRIM:
		free(func);
		return;
	case AM3_COMP:
		gap_dispose(&func->val.clos.words);
		am3_env_release(func->val.clos.env);
		return;
	}
}

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

Am3_tsan *
am3_tsan_new(Am3_env *env)
{
	Am3_tsan *tsan = palloc(tsan);

	pcheck(tsan, NULL);

	if (gap_init(&tsan->code, DEFAULT_TSAN_CODE_SIZE)) {
		free(tsan);
		return NULL;
	}

        if (gap_init(&tsan->data, DEFAULT_TSAN_DATA_SIZE)) {
		gap_dispose(&tsan->code);
		free(tsan);
		return NULL;
	}

	pcheck_c((tsan->elist = palloc(tsan->elist)), NULL,
		 (gap_dispose(&tsan->data),
		  gap_dispose(&tsan->code), free(tsan)));

	tsan->elist->env = env;
	LIST_CONS(tsan->elist, NULL);

	if (env)
		++env->refs;

	return tsan;
}

void
am3_tsan_free(Am3_tsan *tsan)
{
	Am3_elist *elist = tsan->elist;
	Am3_elist *tmp;

	delayed_foreach(tmp, elist) {
		am3_env_release(tmp->env);
		free(tmp);
	}

	gap_dispose(&tsan->code);
	gap_dispose(&tsan->data);
	free(tsan);
}

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

int
am3_apply_word(Am3_word word, Am3_tsan *tsan)
{

	Am3_func *func;
	Am3_elist *elist;

	switch (word) {
	case AM3_FUNC_END:
		elist = tsan->elist;
		tsan->elist = LIST_NEXT(elist);
		am3_env_release(elist->env);
		free(elist);
		return 0;
	case AM3_STACK_PRINT:
		am3_print_stack(&tsan->data);
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
		++elist->env->refs;
		LIST_CONS(elist, tsan->elist);

		pcheck_c(am3_word_write(AM3_FUNC_END, &tsan->code) ||
			 gap_gap_put(&tsan->code, &func->val.clos.words),
			 1, free(elist));
		tsan->elist = elist;
	}

	return 0;
}

