%module netpgpperl
%{
#include <netpgp.h>
#undef SvPOK
#define SvPOK(x) 1
%}
%include netpgp.h
