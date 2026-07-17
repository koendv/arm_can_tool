#ifndef _LOGGER_H_
#define _LOGGER_H_

/* log string */
void logger(const char *buf, const uint32_t buflen);

/* write buffer to file */
void logger_flush(void);

#endif

