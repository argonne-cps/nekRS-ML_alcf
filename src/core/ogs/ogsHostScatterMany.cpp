/*

The MIT License (MIT)

Copyright (c) 2017 Tim Warburton, Noel Chalmers, Jesse Chan, Ali Karakus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "gslib.h"

void ogsHostScatterMany(void *const *v, const int k, const char *type, const char *op, void *gsh_)
{
  auto gsh = static_cast<gs_data*>(gsh_);

  /* need gs_float or gs_double */
  if(!strcmp(type, "float")){
    //    printf("performing string gs on %s\n", type);
    if(!strcmp(op, "add"))
      gs_many(v, k, gs_float, gs_add, 0, gsh, 0);
    else if(!strcmp(op, "mul"))
      gs_many(v, k, gs_float, gs_mul, 0, gsh, 0);
    else if(!strcmp(op, "min"))
      gs_many(v, k, gs_float, gs_min, 0, gsh, 0);
    else if(!strcmp(op, "max"))
      gs_many(v, k, gs_float, gs_max, 0, gsh, 0);
  }
  
  if(!strcmp(type, "double")){
    //    printf("performing double gs on %s\n", type);
    if(!strcmp(op, "add"))
      gs_many(v, k, gs_double, gs_add, 0, gsh, 0);
    else if(!strcmp(op, "mul"))
      gs_many(v, k, gs_double, gs_mul, 0, gsh, 0);
    else if(!strcmp(op, "min"))
      gs_many(v, k, gs_double, gs_min, 0, gsh, 0);
    else if(!strcmp(op, "max"))
      gs_many(v, k, gs_double, gs_max, 0, gsh, 0);
  }

  if(!strcmp(type, "int")){
    //    printf("performing int gs\n");
    if(!strcmp(op, "add"))
      gs_many(v, k, gs_int, gs_add, 0, gsh, 0);
    else if(!strcmp(op, "mul"))
      gs_many(v, k, gs_int, gs_mul, 0, gsh, 0);
    else if(!strcmp(op, "min"))
      gs_many(v, k, gs_int, gs_min, 0, gsh, 0);
    else if(!strcmp(op, "max"))
      gs_many(v, k, gs_int, gs_max, 0, gsh, 0);
  }

  if(!strcmp(type, "long long int")){
    //    printf("performing long_long gs\n");
    if(!strcmp(op, "add"))
      gs_many(v, k, gs_long_long, gs_add, 0, gsh, 0);
    else if(!strcmp(op, "mul"))
      gs_many(v, k, gs_long_long, gs_mul, 0, gsh, 0);
    else if(!strcmp(op, "min"))
      gs_many(v, k, gs_long_long, gs_min, 0, gsh, 0);
    else if(!strcmp(op, "max"))
      gs_many(v, k, gs_long_long, gs_max, 0, gsh, 0);
  } 
}
