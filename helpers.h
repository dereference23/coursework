#pragma once

#include <stdio.h>

static inline void fill_dat_path(char* const path, const char* const name)
{
	sprintf(path, "%s.dat", name);
}

static inline void fill_sch_path(char* const path, const char* const name)
{
	sprintf(path, "%s.schema", name);
}
