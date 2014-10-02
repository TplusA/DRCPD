#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#include "i18n.h"

int main(int argc, char *argv[])
{
    i18n_init();

    const char *dummy = _("dummy");
    printf("%s\n", dummy);

    return 0;
}
