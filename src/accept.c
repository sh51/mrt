#include <stdio.h>
#include <stdlib.h>

int main (int argc, char const * argv[]) {
  char *input;
  size_t bufsize = 64;
  int counter = 0;

  input = (char *)malloc(bufsize * sizeof(char));

  while (1) {
    if (getline(&input, &bufsize, stdin) == -1) continue;
    if (++counter%2) printf("%d lines received\n", counter);
  }

  return 0;
}