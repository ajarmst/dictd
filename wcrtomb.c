#include <wchar.h>
#include <stdlib.h>
#include <assert.h>

/*
  partial wcrtomb implementation:
  - doesn't change *ps
  - s should NOT be NULL
*/

size_t wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
{
   int i;
   int octet_count = 1;

   assert (s);
   assert (wc);

   if (wc <= 0x7F){
      s [0] = (char) wc;
      s [1] = 0;
   }else{
      if (wc <= 0x7FF){
	 s [0]  = 0xC0L;
	 octet_count = 2;
      }else if (wc <= 0xFFFFL){
	 s [0]  = 0xE0;
	 octet_count = 3;
      }else if (wc <= 0x1FFFFFL){
	 s [0]  = 0xF0;
	 octet_count = 4;
      }else if (wc <= 0x3FFFFFFL){
	 s [0]  = 0xF8;
	 octet_count = 5;
      }else if (wc <= 0x7FFFFFFFL){
	 s [0]  = 0xFC;
	 octet_count = 6;
      }else{
/* invalid UCS4 character */
	 return (size_t) -1;
      }

//      s [octet_count] = 0;

      for (i = octet_count-1; i--; ){
	 s [i + 1] = 0x80 | (wc & 0x3F);
	 wc >>= 6;
      }

      s [0] |= wc;
   }

   return octet_count;
}