#include "semihosting_port.h"

#include "settings.h"
#include "serials.h"

bool semihosting_fileio_enabled(void)
{
	return settings.fileio_enable;
}

bool semihosting_shell_enabled(void)
{
	return settings.shell_enable;
}

void semihosting_putstr(const char *buf, size_t len)
{
	cdc0_write(buf, len);
}
