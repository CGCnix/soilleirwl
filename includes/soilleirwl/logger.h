#pragma once

#include <stdio.h>
#include <stdint.h>

typedef enum swl_log_levels {
	SWL_LOG_INFO,
	SWL_LOG_DEBUG,
	SWL_LOG_WARN,
	SWL_LOG_ERROR,
	SWL_LOG_FATAL,
} swl_log_levels_t;

/*Returns the same value as printf i.e. the number of bytes printed
 * or zero if the log level is silenced or the log file is unset.
 * other wise it just reutrns what vfprintf Returns
 */
int swl_log(swl_log_levels_t level, uint32_t line, const char *file, const char *fmt, ...);
int swl_log_printf(swl_log_levels_t level, const char *fmt, ...);


/*Initialise stuff*/
/*All of these are guranteed to succeed*/
void swl_log_set_level(swl_log_levels_t level);
void swl_log_set_fp(FILE *file);
void swl_log_init_fp(swl_log_levels_t level, FILE *file);
void swl_log_close();

/* These return -1 on error 0 on success
 * on error log file state is unchanged
 * But level will still be updated
 */
int swl_log_open(const char *path);
int swl_log_init(swl_log_levels_t level, const char *path);

/*Helpers*/
#define  swl_info(...)  swl_log(SWL_LOG_INFO, __LINE__, __FILE__, __VA_ARGS__);
#define swl_debug(...) swl_log(SWL_LOG_DEBUG, __LINE__, __FILE__, __VA_ARGS__);
#define  swl_warn(...)  swl_log(SWL_LOG_WARN, __LINE__, __FILE__, __VA_ARGS__);
#define swl_error(...) swl_log(SWL_LOG_ERROR, __LINE__, __FILE__, __VA_ARGS__);
#define swl_fatal(...) swl_log(SWL_LOG_FATAL, __LINE__, __FILE__, __VA_ARGS__);
