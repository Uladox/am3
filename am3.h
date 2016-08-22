/* Include these
 * #include <stddef.h>
 * #include <stdint.h>
 * #include <nit/list.h>
 * #include <nit/hmap.h>
 * #include <nit/gap-buf.h>
 */

typedef uint32_t Am3_word;

enum am3_func_type {
	AM3_PRIM,
	AM3_COMP
};

typedef struct am3_env {
	struct am3_env *up;
	Nit_hmap *dict;
	int refs;
} Am3_env;

typedef struct {
	Am3_env *env;
	Nit_gap words;
	int refs;
} Am3_clos;

typedef struct {
	enum am3_func_type type;
	union {
		int (*c_func)(Nit_gap *stack);
		Am3_clos clos;
	} val;
} Am3_func;

typedef struct {
	Nit_list list;
	Am3_clos *clos;
} Am3_clist;

typedef struct {
	Am3_clist *clist;
	Nit_gap stack;
} Am3_tsan;

int
am3_eval_word(Am3_word word, Am3_tsan *tsan);

