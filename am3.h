/* Include these
 * #include <stddef.h>
 * #include <stdint.h>
 * #include <nit/list.h>
 * #include <nit/hmap.h>
 * #include <nit/gap-buf.h>
 */

typedef uint32_t Am3_word;

enum am3_opcodes {
	AM3_STACK_BOTTOM,
	AM3_FUNC_END,
	AM3_STACK_PRINT
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
am3_conti_new(Am3_env *env);

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

void
am3_func_release(Am3_func *func);

Am3_func *
am3_func_get_func(const Am3_func *func, Am3_word word);

/* other */

int
am3_word_write(Nit_gap *gap, Am3_word word);

void
am3_print_stack(Nit_gap *stack);

Am3_func *
am3_dict_get(const Nit_hmap *map, Am3_word word);

