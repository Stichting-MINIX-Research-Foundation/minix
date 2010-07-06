/* _PROTOTYPE(   void diag, (int level,char *msg, int a, int b)    ); */
void diag();			/* HACK.  should be varargs */

_PROTOTYPE(   void set_profile, (void )    );
_PROTOTYPE(   void scan_profile, (FILE *f)    );
_PROTOTYPE(   int eqin, (char *s1,char *s2)    );
_PROTOTYPE(   void set_defaults, (void )    );
_PROTOTYPE(   void set_option, (char *arg)    );
_PROTOTYPE(   void pr_comment, (void )    );
_PROTOTYPE(   int main, (int argc,char * *argv)    );
_PROTOTYPE(   void bakcopy, (void )    );
_PROTOTYPE(   void dump_line, (void )    );
_PROTOTYPE(   int code_target, (void )    );
_PROTOTYPE(   int label_target, (void )    );
_PROTOTYPE(   void fill_buffer, (void )    );
_PROTOTYPE(   int pad_output, (int current,int target)    );
_PROTOTYPE(   int count_spaces, (int current,char *buffer)    );
_PROTOTYPE(   void writefdef, (struct fstate *f,int nm)    );
_PROTOTYPE(   char *chfont, (struct fstate *of,struct fstate *nf,char *s)    );
_PROTOTYPE(   void parsefont, (struct fstate *f,char *s0)    );
_PROTOTYPE(   int lexi, (void )    );
_PROTOTYPE(   void addkey, (char *key,int val)    );
_PROTOTYPE(   void makext, (char *newname,char *newext)    );
_PROTOTYPE(   void parse, (int tk)    );
_PROTOTYPE(   void reduce, (void )    );

