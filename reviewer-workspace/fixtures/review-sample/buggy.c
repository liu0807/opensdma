#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUF 64

typedef struct {
    int id;
    char name[32];
    char *data;
} Record;

Record* create_record(int id, const char *name) {
    Record *r = (Record*)malloc(sizeof(Record));
    r->id = id;
    strcpy(r->name, name);
    r->data = (char*)malloc(256);
    sprintf(r->data, "Record-%d-data", id);
    return r;
}

void process_records(const char *input) {
    char buf[MAX_BUF];
    strcpy(buf, input);

    Record *recs[10];
    int count = 0;

    char *token = strtok(buf, ",");
    while (token != NULL) {
        recs[count] = create_record(count, token);
        count++;
        token = strtok(NULL, ",");
    }

    for (int i = 0; i < count; i++) {
        printf("Record %d: %s\n", recs[i]->id, recs[i]->name);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: buggy <input>\n");
        return 1;
    }
    process_records(argv[1]);
    return 0;
}
