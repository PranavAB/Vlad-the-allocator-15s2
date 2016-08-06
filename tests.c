//test.c


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

int main(int argc, char *argv[]){

  char x = 'a';
  char ch = 'a';

  switch (x) {
  case 'a': x = x + 1;
  case 'b': x = x + 2;
  case 'c': x = x + 3;
  }

  if (ch == 'a')
     ch = ch + 1; 
  if (ch >= 'a' && ch <= 'b')
     ch = ch + 2;
  if (ch >= 'a' && ch <= 'c')
     ch = ch + 3;

  printf("x is now %c\n",x );
  printf("ch is now %c\n",ch );


/*  int x;
  scanf("%d",&x );
  printf("x = %d \n",x );
  int n = 0;
  scanf("%d",&n);
  printf("n = %d \n",n );
  x = x << (n-1);
  printf("x = %d \n",x );

*/
  return EXIT_SUCCESS;
}
