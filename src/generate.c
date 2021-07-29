#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main (int argc, char const * argv[]) {
  char * txt = "All work and no play makes Jack a dull boy\n";
  // int counter = 10;

  while (1) {
    write(1, txt, strlen(txt));
  }

  return 0;
}