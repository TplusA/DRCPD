#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <locale.h>

#include "i18n.h"

#ifdef ENABLE_NLS

void i18n_init(void)
{
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    setlocale(LC_ALL, "");
}

void i18n_switch_language(const char *language_identifier)
{
    setenv("LANGUAGE", language_identifier, 1);
    setlocale(LC_ALL, "");
}

#else /* !ENABLE_NLS  */

void i18n_init(void) {}
void i18n_switch_language(const char *language_identifier) {}

#endif /* ENABLE_NLS */
