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

#define DEFAULT_ENV_SEQUENCE 0
#define DEFAULT_CLOS_CODE_SIZE  16
#define DEFAULT_CONTI_CODE_SIZE 1024
#define DEFAULT_CONTI_DATA_SIZE 1024

/* dict */

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

Nit_hmap *
am3_dict_new(void)
{
	return hmap_new(DEFAULT_ENV_SEQUENCE,
			func_hmap_compare, func_hmap_free);
}

void
am3_dict_free(Nit_hmap *dict)
{
	hmap_free(dict);
}


/* remove this later */
const char *error = "error";

const char *
am3_dict_add(Nit_hmap *map, Am3_word word, Am3_func *func)
{
	Am3_word *copy = palloc(copy);

	pcheck(copy, error);
	*copy = word;

	hmap_remove(map, &word, sizeof(word));

	return hmap_add(map, copy, sizeof(word), func);
}

Am3_func *
am3_dict_get(const Nit_hmap *map, Am3_word word)
{
	return hmap_get(map, &word, sizeof(word));
}

/* env */

Am3_env *
am3_env_new(Am3_env *up)
{
	Am3_env *env = palloc(env);

	pcheck(env, NULL);
	pcheck_c((env->dict = am3_dict_new()), NULL, free(env));
	env->up = up;
	env->refs = 1;

	return env;
}


void
am3_env_release(Am3_env *env)
{
	if (!env || --env->refs)
		return;

	am3_dict_free(env->dict);
	am3_env_release(env->up);
	free(env);
}

Am3_func *
am3_env_get_func(const Am3_env *env, Am3_word word)
{
	return am3_dict_get(env->dict, word);
}

const char *
am3_env_add_func(Am3_env *env, Am3_word word, Am3_func *func)
{
	return am3_dict_add(env->dict, word, func);
}

/* clos */

Am3_clos *
am3_clos_new(Am3_env *env)
{
	Am3_clos *clos = palloc(clos);

	pcheck(clos, NULL);

        if (gap_init(&clos->words, DEFAULT_CLOS_CODE_SIZE)) {
		free(clos);
		return NULL;
	}

	++(clos->env = env)->refs;

	return clos;
}

void
am3_clos_free(Am3_clos *clos)
{
	gap_dispose(&clos->words);
	am3_env_release(clos->env);
	free(clos);
}

Am3_func *
am3_clos_get_func(const Am3_clos *clos, Am3_word word)
{
	return am3_env_get_func(clos->env, word);
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
am3_conti_new(const Am3_elist *elist)
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

	if (!elist) {
		pcheck_c((conti->elist = palloc(conti->elist)), NULL,
			 (gap_dispose(&conti->data), gap_dispose(&conti->code),
			  free(conti)));
		pcheck_c((conti->elist->env = am3_env_new(NULL)), NULL,
			 (gap_dispose(&conti->data),  gap_dispose(&conti->code),
			  free(conti->elist), free(conti)));

		LIST_CONS(conti->elist, NULL);

		return conti;
	}

	pcheck_c((conti->elist = am3_elist_copy(elist)), NULL,
		 (gap_dispose(&conti->data), gap_dispose(&conti->code),
		  free(conti)));

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

const char *
am3_conti_add_func(Am3_conti *conti, Am3_word word, Am3_func *func)
{
	return am3_env_add_func(conti->elist->env, word, func);
}

int
am3_conti_apply_clos(Am3_conti *conti, const Am3_clos *clos)
{
	Am3_elist *elist = palloc(elist);

	pcheck(elist, 1);
	elist->env = clos->env;
	++elist->env->refs;
	LIST_CONS(elist, conti->elist);

	pcheck_c(am3_word_write(&conti->code, AM3_FUNC_END) ||
		 gap_gap_put(&conti->code, &clos->words),
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

Am3_conti *
am3_conti_copy(const Am3_conti *src)
{
	Am3_conti *des = palloc(des);

	pcheck(des, NULL);

	if (gap_clone(&des->code, &src->code)) {
		free(des);
		return NULL;
	}

	if (gap_clone(&des->data, &src->data)) {
		gap_dispose(&des->code);
		free(des);
		return NULL;
	}

	if (!(des->elist = am3_elist_copy(src->elist))) {
		gap_dispose(&des->data);
		gap_dispose(&des->code);
		free(des);
		return NULL;
	}

	return des;
}

int
am3_conti_apply_word(Am3_conti *conti, Am3_word word)
{

	/* tmp vals */
	Am3_func *func;
	Am3_elist *elist;
	Am3_word word2;

	switch (word) {
	case AM3_FUNC_END:
		elist = conti->elist;
		conti->elist = LIST_NEXT(elist);
		am3_env_release(elist->env);
		free(elist);
		return 0;
	case AM3_STACK_PRINT:
		am3_stack_print(&conti->data);
		return 0;
	case AM3_COPY_CONTI:
		if ((word2 = am3_stack_pop(&conti->data)) == AM3_STACK_ERROR)
			return 1;

		pcheck((func = am3_func_copy_conti(conti)), 1);

		if (am3_conti_add_func(conti, word2, func))
			return 1;

		return 0;
	case AM3_APPLY_CONTI:
		if ((word2 = am3_stack_pop(&conti->data)) == AM3_STACK_ERROR)
			return 1;

		pcheck((func = am3_conti_get_func(conti, word2)), 1);

		return am3_conti_apply_conti(conti, func->val.conti);
	}

	pcheck((func = am3_conti_get_func(conti, word)), 1);

	switch (func->type) {
	case AM3_PRIM:
		return func->val.c_func(func, &conti->data);
	case AM3_CLOS:
		return am3_conti_apply_clos(conti, func->val.clos);
	case AM3_CONTI:
		return am3_conti_apply_conti(conti, func->val.conti);
	}

	return 1;
}

int
am3_conit_eval_1(Am3_conti *conti)
{
	Am3_word word;

	if ((word = am3_stack_next(&conti->code)) == AM3_STACK_ERROR)
		return 1;

	return am3_conti_apply_word(conti, word);
}

/* func */

Am3_func *
am3_func_new_clos(Am3_env *env)
{
	Am3_func *func = palloc(func);

	pcheck(func, NULL);
	pcheck_c((func->val.clos = am3_clos_new(env)),
		 NULL, free(func));

	func->type = AM3_CLOS;
	++func->refs;

	return func;
}

Am3_func *
am3_func_new_conti(const Am3_elist *elist)
{
	Am3_func *func = palloc(func);

	pcheck(func, NULL);
	pcheck_c((func->val.conti = am3_conti_new(elist)),
		 NULL, free(func));

	func->type = AM3_CONTI;
	++func->refs;

	return func;
}

Am3_func *
am3_func_copy_conti(const Am3_conti *conti)
{
	Am3_func *func = palloc(func);

	pcheck(func, NULL);
	pcheck_c((func->val.conti = am3_conti_copy(conti)),
		 NULL, free(func));

	func->type = AM3_CONTI;
	func->refs = 1;

	return func;
}

void
am3_func_release(Am3_func *func)
{
	if (--func->refs)
		return;

	switch (func->type) {
	case AM3_PRIM:
		break;
	case AM3_CLOS:
		am3_clos_free(func->val.clos);
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
	switch (func->type) {
	case AM3_PRIM:
		return NULL;
	case AM3_CLOS:
		return am3_clos_get_func(func->val.clos, word);
	case AM3_CONTI:
		return am3_conti_get_func(func->val.conti, word);
	}

	return NULL;
}

void
am3_func_print(const Am3_func *func)
{
	printf("(func ");

	if (!func) {
		printf("nil)\n");
		return;
	}

	printf("(type ");
	switch (func->type) {
	case AM3_PRIM:
		printf("prim");
		break;
	case AM3_CLOS:
		printf("clos");
		break;
	case AM3_CONTI:
		printf("conti");
		break;
	}

	printf(") (refs %i))\n", func->refs);
}

/* stack */
int
am3_stack_push(Nit_gap *gap, Am3_word word)
{
	return am3_word_write(gap, word);
}

Am3_word
am3_stack_pop(Nit_gap *gap)
{
	Am3_word word;

	if (gap_cut_b(gap, &word, sizeof(word)))
		return AM3_STACK_ERROR;

	return word;
}

Am3_word
am3_stack_next(Nit_gap *gap)
{
	Am3_word word;

	if (gap_prev(gap, &word, sizeof(word)))
		return AM3_STACK_ERROR;

	return word;
}

void
am3_stack_print(const Nit_gap *stack)
{
	const Am3_word *words = (Am3_word *) stack->bytes;
	int count = 0;
	int val = stack->start / sizeof(Am3_word);

	printf("(stack");

	for (; count < val; ++count)
		printf(" [%" PRIu32 "]", words[count]);

	printf(" @c");

	words += (stack->end + 1) / sizeof(Am3_word);
	count = 0;
	val = (stack->size - stack->end - 1) / sizeof(Am3_word);

	for (; count < val; ++count)
		printf(" [%" PRIu32 "]", words[count]);

	printf(")\n");
}

/* other */
int
am3_word_write(Nit_gap *gap, Am3_word word)
{
	return gap_write(gap, &word, sizeof(word));
}

