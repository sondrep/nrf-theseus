/* Inspired by https://stackoverflow.com/a/70409249 */

#include <theseus/log.h>
#include <theseus/module.h>

#define THESEUS_MODULE_START THESEUS_CONCAT2(__start_, theseus_modules)
#define THESEUS_MODULE_STOP  THESEUS_CONCAT2(__stop_, theseus_modules)

#define THESEUS_MODULE_START_DECLARE extern const struct theseus_module THESEUS_MODULE_START[];
#define THESEUS_MODULE_STOP_DECLARE  extern const struct theseus_module THESEUS_MODULE_STOP[];

THESEUS_MODULE_START_DECLARE
THESEUS_MODULE_STOP_DECLARE

int theseus_modules_init(void)
{
	const struct theseus_module *a = THESEUS_MODULE_START;
	const struct theseus_module *b = THESEUS_MODULE_STOP;
	int ret = 0;
	while (a != b) {
		if (a->init != NULL) {
			int err = a->init();
			/* We want to initialize all modules, but we want to return an error for the
			 * first module that fails */
			if (ret == 0) {
				ret = err;
			}
		}
		++a;
	}

	return ret;
}

void __attribute__ ((constructor)) init(){
	theseus_modules_init();
}
