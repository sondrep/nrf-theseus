
/**
 * @brief Initialize all the modules in theseus
 *
 * @return 0 if all modules initialized successfully, otherwise, returns the error code of the first
 * module that failed initialization
 */
int theseus_modules_init(void);

typedef int (*theseus_module_callback)(void);

struct theseus_module {
	theseus_module_callback init;
	theseus_module_callback deinit;
};

#define __THESEUS_STRINGIFY(x) #x

/**
 * @brief Stringify @p x
 *
 * @param x Text to "String"
 */
#define THESEUS_STRINGIFY(x) __THESEUS_STRINGIFY(x)

#define __THESEUS_CONCAT2(x, y) x##y

/**
 * @brief Concatenate the 2 tokens @p x and @p y
 *
 */
#define THESEUS_CONCAT2(x, y) __THESEUS_CONCAT2(x, y)

/**
 * @brief Declare that a variable is used to avoid compiler warnings
 *
 */
#define THESEUS_ATTRIBUTE_USED __attribute__((used))

/**
 * @brief Declare that a variable is part of the section called @p section_name
 *
 * @param section_name name of the section (not in quotes)
 */
#define THESEUS_ATTRIBUTE_SECTION(section_name)                                                    \
	__attribute__((section(THESEUS_STRINGIFY(section_name))))

/**
 * @brief Set a module in the thesues modules section.
 *
 * @example
 * THESEUS_MODULE_REF(log) = {.init = theseus_console_init};
 *
 * @param module_name name of the module that you wish to set.
 */
#define THESEUS_MODULE_SET(module_name)                                                            \
	static const struct theseus_module THESEUS_CONCAT2(__theseus_mod_, module_name)            \
		THESEUS_ATTRIBUTE_SECTION(theseus_modules) THESEUS_ATTRIBUTE_USED
