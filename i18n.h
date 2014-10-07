#ifndef I18N_H
#define I18N_H

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef ENABLE_NLS
#include <libintl.h>

#define _(S)    gettext(S)
#else /* !ENABLE_NLS */
#define _(S)    (S)
#endif /* ENABLE_NLS */

#define N_(S)   (S)

#ifdef __cplusplus
extern "C" {
#endif

void i18n_init(void);
void i18n_switch_language(const char *language_identifier);

#ifdef __cplusplus
}
#endif

#endif /* !I18N_H */
