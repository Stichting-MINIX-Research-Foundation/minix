/*	$NetBSD: extern.h,v 1.1.1.1 2008/05/18 14:31:33 aymeric Exp $ */

/* Do not edit: automatically built by build/distrib. */
void __vi_InitCopyPaste
   __P((int (*)(), int (*)(), int (*)(), int (*)())); 
void	__vi_AcquirePrimary __P((Widget));
void	__vi_PasteFromClipboard __P((Widget));
void __vi_send_command_string __P((String));
void __vi_cancel_cb __P((Widget, XtPointer, XtPointer));
void __vi_modal_dialog __P((Widget));
Widget vi_create_menubar __P((Widget));
int __vi_editopt __P((IPVI *, const char *, u_int32_t, const char *, u_int32_t, u_int32_t));
void __vi_show_options_dialog __P((Widget, String));
int __vi_toggle __P((char *));
Widget __vi_create_search_toggles __P((Widget, optData[]));
void __vi_set_text_ruler __P((int, int));
void __vi_search __P((Widget));
void __XutConvertResources __P((Widget, String, XutResource *, int));
void __vi_set_scroll_block __P((void));
void __vi_clear_scroll_block __P((void));
void vi_input_func __P((XtPointer, int *, XtInputId *));
void	__vi_draw_text __P((xvi_screen *, int, int, int));
void	__vi_expose_func __P((Widget, XtPointer, XtPointer));
Widget vi_create_editor __P((String, Widget, void (*)(void)));
void __vi_set_cursor __P((xvi_screen *, int));
void __vi_set_word_at_caret __P((xvi_screen *));
void draw_caret __P((xvi_screen *));
void __vi_erase_caret __P((xvi_screen *));
void	__vi_move_caret __P((xvi_screen *, int, int));
Widget __vi_CreateTabbedFolder
    __P((String, Widget, String, int, void (*)(Widget, int)));
