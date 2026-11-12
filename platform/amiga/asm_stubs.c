/* Stand-ins for x86 asm functions not remade in C yet (Amiga build).
 * Logs the name on the first call, to RAM:SyndWars-stubs.txt.
 */
#include <stdio.h>
#include <string.h>
#include "asm_stubs.h"

#define MAX_NAMES 512

static const char *seen[MAX_NAMES];
static int nseen;

void asm_stub_hit(const char *name)
{
    int i;
    FILE *f;

    for (i = 0; i < nseen; i++)
        if (seen[i] == name || strcmp(seen[i], name) == 0)
            return;
    if (nseen < MAX_NAMES)
        seen[nseen++] = name;
    f = fopen("RAM:SyndWars-stubs.txt", "a");
    if (f != NULL) {
        fprintf(f, "%s\n", name);
        fclose(f);
    }
}
