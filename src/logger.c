#include <soilleirwl/logger.h>

#include <stdio.h>
#include <stdarg.h>

/*Globals(well in this file)=*/
static FILE *glog_file = NULL;
static swl_log_levels_t glog_level = 0;

static const char *swl_log_level_to_str(swl_log_levels_t level) {
	switch(level) {
		case SWL_LOG_INFO: return "INFO";
		case SWL_LOG_DEBUG: return "DEBUG";
		case SWL_LOG_WARN: return "WARN";
		case SWL_LOG_ERROR: return "ERROR";
		case SWL_LOG_FATAL: return "FATAL";
		default: return "UNKNOWN";
	}
}

int swl_log(swl_log_levels_t level, uint32_t line, const char *file, const char *fmt, ...) {
	va_list args;
	int ret = 0;

	if(level >= glog_level && glog_file) {
		fprintf(glog_file, "%s %s(%d): ", swl_log_level_to_str(level), file, line);
		va_start(args, fmt);
		ret = vfprintf(glog_file, fmt, args);
		va_end(args);
		fflush(glog_file);
	}
	return ret;
}

int swl_log_printf(swl_log_levels_t level, const char *fmt, ...) {
	va_list args;
	int ret = 0;

	if(level >= glog_level && glog_file) {
		va_start(args, fmt);
		ret = vfprintf(glog_file, fmt, args);
		va_end(args);
		fflush(glog_file);
	}
	return ret;
}

void swl_log_set_level(swl_log_levels_t level) {
	glog_level = level;
}

void swl_log_close() {
	if(glog_file) {
		fclose(glog_file);
		glog_file = NULL;
	}
}

void swl_log_set_fp(FILE *file) {
	swl_log_close();
	glog_file = file;
}

int swl_log_open(const char *path) {
	FILE *file;
	int ret = -1;

	file = fopen(path, "w");
	if(file) {
		swl_log_set_fp(file);
		ret = 0;
	}
	return ret;
}

int swl_log_init(swl_log_levels_t level, const char *path) {
	swl_log_set_level(level);
	return swl_log_open(path);
}

void swl_log_init_fp(swl_log_levels_t level, FILE *file) {
	swl_log_set_level(level);
	swl_log_set_fp(file);
}
