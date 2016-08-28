/* Include these
 * #include <stddef.h>
 * #include <stdint.h>
 * #include <nit/list.h>
 * #include <nit/hmap.h>
 * #include <nit/gap-buf.h>
 */

typedef uint32_t Am3_word;

enum am3_opcodes {
	AM3_STACK_ERROR = 0,
	AM3_STACK_BOTTOM,
	AM3_FUNC_END,
	AM3_STACK_PRINT,
	AM3_COPY_CONTI,
	AM3_APPLY_CONTI
};

typedef struct am3_env {
	struct am3_env *up;
	Nit_hmap *dict;
	int refs;
} Am3_env;

typedef struct {
	Am3_env *env;
	Nit_gap words;
} Am3_clos;

typedef struct {
	Nit_list list;
	Am3_env *env;
} Am3_elist;

typedef struct {
	Am3_elist *elist;
	Nit_gap data;
	Nit_gap code;
} Am3_conti;

enum am3_func_type {
	AM3_PRIM,
	AM3_CLOS,
	AM3_CONTI
};

typedef struct am3_func {
	enum am3_func_type type;
	int refs;
	union {
		int (*c_func)(struct am3_func *func, Nit_gap *stack);
		Am3_clos *clos;
		Am3_conti *conti;
	} val;
} Am3_func;

/* dict */

Nit_hmap *
am3_dict_new(void);

const char *
am3_dict_add(Nit_hmap *map, Am3_word word, Am3_func *func);

Am3_func *
am3_dict_get(const Nit_hmap *map, Am3_word word);

void
am3_dict_free(Nit_hmap *dict);

/* env */

Am3_env *
am3_env_new(Am3_env *up);

void
am3_env_release(Am3_env *env);

Am3_func *
am3_env_get_func(const Am3_env *env, Am3_word word);

/* clos */

void
am3_clos_free(Am3_clos *clos);

Am3_func *
am3_clos_get_func(const Am3_clos *clos, Am3_word word);

/* conti */

Am3_conti *
am3_conti_new(const Am3_elist *elist);

void
am3_conti_free(Am3_conti *conti);

Am3_func *
am3_conti_get_func(const Am3_conti *conti, Am3_word word);

int
am3_conti_apply_clos(Am3_conti *conti, const Am3_clos *clos);

int
am3_conti_apply_conti(Am3_conti *des, const Am3_conti *src);

int
am3_conti_apply_word(Am3_conti *conti, Am3_word word);

/* func */

Am3_func *
am3_func_new_clos(Am3_env *env);

Am3_func *
am3_func_new_conti(const Am3_elist *elist);

Am3_func *
am3_func_copy_conti(const Am3_conti *conti);

void
am3_func_release(Am3_func *func);

Am3_func *
am3_func_get_func(const Am3_func *func, Am3_word word);

void
am3_func_print(const Am3_func *func);

/* stack */

int
am3_stack_push(Nit_gap *gap, Am3_word word);

Am3_word
am3_stack_pop(Nit_gap *gap);

void
am3_stack_print(const Nit_gap *stack);

/* other */

int
am3_word_write(Nit_gap *gap, Am3_word word);



