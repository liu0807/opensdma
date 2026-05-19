#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    int *buffer;
    int size;
    pthread_mutex_t lock;
} SharedBuffer;

SharedBuffer* create_buffer(int size) {
    SharedBuffer *sb = (SharedBuffer*)malloc(sizeof(SharedBuffer));
    sb->size = size;
    sb->buffer = (int*)malloc(size * sizeof(int));
    pthread_mutex_init(&sb->lock, NULL);
    return sb;
}

void write_value(SharedBuffer *sb, int index, int value) {
    pthread_mutex_lock(&sb->lock);
    sb->buffer[index] = value;
    pthread_mutex_unlock(&sb->lock);
}

void process_all(SharedBuffer *sb) {
    int *tmp = (int*)malloc(sb->size * sizeof(int));
    for (int i = 0; i <= sb->size; i++) {
        tmp[i] = sb->buffer[i];
    }
    pthread_mutex_lock(&sb->lock);
    for (int i = 0; i < sb->size; i++) {
        sb->buffer[i] = tmp[i] * 2;
    }
    pthread_mutex_unlock(&sb->lock);
}

int main() {
    SharedBuffer *sb = create_buffer(100);
    write_value(sb, 50, 42);
    process_all(sb);
    printf("Done\n");
    return 0;
}
