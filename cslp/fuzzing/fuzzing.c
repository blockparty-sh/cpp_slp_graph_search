#include <stdio.h>
#include <stdlib.h>
#include <cslp/cslp.h>

int main(int argc, char * argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Need to pass file argument\n");
        return 1;
    }

    const char * name = argv[1];

    FILE * file;
    char * buffer;
    unsigned long fileLen;

    file = fopen(name, "rb");
    if (! file) {
        fprintf(stderr, "Unable to open file %s\n", name);
        return 1;
    }
    
    fseek(file, 0, SEEK_END);
    fileLen = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = (char *) malloc(fileLen+1);
    if (! buffer) {
        fprintf(stderr, "Memory error!\n");
        fclose(file);
        return 1;
    }

    fread(buffer, fileLen, 1, file);
    fclose(file);

    cslp_validator validator = cslp_validator_init();
    cslp_validator_add_tx(validator, buffer, fileLen);
    cslp_validator_destroy(validator);

    free(buffer);

    return 0;
}
