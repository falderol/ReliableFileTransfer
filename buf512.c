#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define DATA_BUFFER 507
#define HEADER_SIZE 5

long file_size(char *name) {
    FILE *fp  = fopen(name, "rb");
    long size = -1;
    if(fp) {
        fseek (fp, 0, SEEK_END);
        size = ftell(fp);
        fclose(fp);
    }
    return size;
}

int main(int argc, char **argv) {
  char* filename = argv[1];
  char d_buffer[507];
  char header[5];
  long fs = file_size(filename);
  FILE* file_lnk = fopen(filename, "rb");
  if(file_lnk) {
    printf("%s\n", filename);
    printf("%lu\n", fs);
    while(fread(d_buffer, DATA_BUFFER, 1, file_lnk) != 0) {
      printf("%s\n", d_buffer);
      printf("\n\n%lu\n\n", sizeof(d_buffer));
      bzero(d_buffer, DATA_BUFFER);
    }
    fclose(file_lnk);
  }
}