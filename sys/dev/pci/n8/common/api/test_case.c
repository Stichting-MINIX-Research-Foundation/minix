/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* pke_bn.c: OPENSSL big number library wrappers and big number <-> big cache functions */

#include "n8_OS_intf.h"
#include "test_case.h"


#ifdef N8DEBUG
#define CHECKFSCANFRETURN(r,n) if ((r) != (n)){\
             DBG(("error in fscanf at %d of %s, expected %d got %d\n", \
             __LINE__, __FILE__,(n),(r)));\
             return TC_FSCANF_ERR;}
#else
#define CHECKFSCANFRETURN(r,n) if ((r) != (n)) return TC_FSCANF_ERR;
#endif

#define CHECKREADATTRRETURN(r) if((r) != TC_SUCCESS) return (r);


int pkeFormattedReadNum(FILE *fp, int length, int offset, char *valuePtr)
{
    int i, j;
    int iResult;
    char *tmpPtr;
    unsigned int temp;
    
    tmpPtr = valuePtr;

    for (i=0; i < length; i++) {
       for (j=0; j< 15; j++) {
           iResult = fscanf(fp, "%2x", &temp);
           CHECKFSCANFRETURN(iResult, 1);
           *tmpPtr++ = (unsigned char) (temp & 0xff);
        }
        iResult = fscanf(fp,"%2x\n", &temp);
        CHECKFSCANFRETURN(iResult, 1);
        *tmpPtr++ = (unsigned char) (temp & 0xff);
    }
    *tmpPtr = 0;
    return TC_SUCCESS;
}
int pkeReadAttr(FILE *fp, ATTR *thisone, char *name)
{
    int r;
    char fmt[16];

    strcpy(fmt, name);
    strcat(fmt, " %d %d\n");
    r = fscanf(fp, fmt, &(thisone->length), &(thisone->offset));
    CHECKFSCANFRETURN(r,2);
    return pkeFormattedReadNum(fp,thisone->length,thisone->offset,thisone->value);
}
int pkeReadModm(FILE *fp, TEST_CASE *thisone)    
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
        
}
int pkeReadRmod(FILE *fp,TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadAddm(FILE *fp,TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->b),"b"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadSubm(FILE *fp,TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->b),"b"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadAinv(FILE *fp,TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadMulm(FILE *fp,TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->b),"b"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadMinv(FILE *fp,TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadExpm(FILE *fp, TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->b),"b"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->c),"c"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadExp(FILE *fp, TEST_CASE *thisone)
{
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->m),"m"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->a),"a"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->b),"b"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->r),"r"));
    return TC_SUCCESS;
}
int pkeReadRSA(FILE *fp, TEST_CASE *thisone)
{
   if (fscanf(fp, "sks %d %d\n", 
              &(thisone->use_sks), &(thisone->sks_addr)) != 2) {
       return TC_FSCANF_ERR;
   }
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[RSA_P_Operand]),"p"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[RSA_Q_Operand]),"q"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[RSA_D_Operand]),"d"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[RSA_N_Operand]),"n"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[RSA_U_Operand]),"u"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[RSA_E_Operand]),"e"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[RSA_I_Operand]),"i"));
    return TC_SUCCESS;
}
int pkeReadDSA(FILE *fp, TEST_CASE *thisone)
{
   if (fscanf(fp, "sks %d %d\n", 
              &(thisone->use_sks), &(thisone->sks_addr)) != 2) {
      return TC_FSCANF_ERR;
   }
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[DSA_N_Operand]),"n"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[DSA_E_Operand]),"e"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[DSA_P_Operand]),"p"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[DSA_G_Operand]),"g"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[DSA_Q_Operand]),"q"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[DSA_X_Operand]),"x"));
    CHECKREADATTRRETURN(pkeReadAttr(fp,&(thisone->opnd[DSA_Y_Operand]),"y"));
    return TC_SUCCESS;
}
int read_test_case(FILE *fp, TEST_CASE *thisI)
{
    char *token;
    int iResult;

    memset(thisI, 0, sizeof(TEST_CASE));
    token = (char *) &(thisI->token[0]);

    iResult = fscanf(fp, "%s %d\n", (char *)&(token[0]), &(thisI->seq_number));
    if(iResult != 2)
    {
        if(iResult == EOF) return TC_EOF;
        else return TC_FSCANF_ERR;
    }
    if (strcmp(token,"modm") == 0)
    {
        return pkeReadModm(fp, thisI);
    }
    else if (strcmp(token,"rmod") == 0) {
        return pkeReadRmod(fp, thisI);
    }
    else if (strcmp(token,"addm") == 0) {
        return pkeReadAddm(fp, thisI);
    }
    else if (strcmp(token,"subm") == 0) {
        return pkeReadSubm(fp, thisI);
    }
    else if (strcmp(token,"ainv") == 0) {
        return pkeReadAinv(fp, thisI);
    }
    else if (strcmp(token,"mulm") == 0) {
        return pkeReadMulm(fp, thisI);
    }
    else if (strcmp(token,"minv") == 0) {
        return pkeReadMinv(fp, thisI);
    }
    else if (strcmp(token,"expm") == 0) {
        return pkeReadExpm(fp, thisI);
    }
    else if (strcmp(token,"exp") == 0) {           /* added for performance testing 020509 MWW */
        return pkeReadExp(fp, thisI);
    }
    else if (strcmp(token, "rsa") == 0) {
       return pkeReadRSA(fp, thisI);
    }
    else if (strcmp(token, "dsa") == 0) {
       return pkeReadDSA(fp, thisI);
    }
    return TC_GENERIC_ERROR;

}
