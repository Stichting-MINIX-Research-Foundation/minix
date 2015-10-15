#	$NetBSD: libglsl7.mk,v 1.1 2015/01/05 03:45:31 mrg Exp $

# This copy for MesaLib7 old drivers.

# Derived loosely from src/glsl/Makefile.

# XXX Now that we don't need glsl-compile as a tool, this should just
# be made into a library instead.

.PATH: ${X11SRCDIR.MesaLib7}/src/glsl
.PATH: ${X11SRCDIR.MesaLib7}/src/glsl/glcpp

CPPFLAGS+=	-I${X11SRCDIR.MesaLib7}/include
CPPFLAGS+=	-I${X11SRCDIR.MesaLib7}/src/glsl
CPPFLAGS+=	-I${X11SRCDIR.MesaLib7}/src/mapi
CPPFLAGS+=	-I${X11SRCDIR.MesaLib7}/src/mesa

SRCS.glsl.libglcpp= \
	glcpp-lex.c \
	glcpp-parse.c \
	pp.c

SRCS.glsl.glcpp= \
	${SRCS.glsl.libglcpp} \
	strtod.c \
	glcpp.c

SRCS.glsl.c= \
	strtod.c \
	ralloc.c \
	${SRCS.glsl.libglcpp}

SRCS.glsl.cxx= \
	ast_expr.cpp \
	ast_function.cpp \
	ast_to_hir.cpp \
	ast_type.cpp \
	glsl_lexer.cpp \
	glsl_parser.cpp \
	glsl_parser_extras.cpp \
	glsl_types.cpp \
	glsl_symbol_table.cpp \
	hir_field_selection.cpp \
	ir_basic_block.cpp \
	ir_clone.cpp \
	ir_constant_expression.cpp \
	ir.cpp \
	ir_expression_flattening.cpp \
	ir_function_can_inline.cpp \
	ir_function_detect_recursion.cpp \
	ir_function.cpp \
	ir_hierarchical_visitor.cpp \
	ir_hv_accept.cpp \
	ir_import_prototypes.cpp \
	ir_print_visitor.cpp \
	ir_reader.cpp \
	ir_rvalue_visitor.cpp \
	ir_set_program_inouts.cpp \
	ir_validate.cpp \
	ir_variable.cpp \
	ir_variable_refcount.cpp \
	linker.cpp \
	link_functions.cpp \
	loop_analysis.cpp \
	loop_controls.cpp \
	loop_unroll.cpp \
	lower_discard.cpp \
	lower_if_to_cond_assign.cpp \
	lower_instructions.cpp \
	lower_jumps.cpp \
	lower_mat_op_to_vec.cpp \
	lower_noise.cpp \
	lower_texture_projection.cpp \
	lower_variable_index_to_cond_assign.cpp \
	lower_vec_index_to_cond_assign.cpp \
	lower_vec_index_to_swizzle.cpp \
	lower_vector.cpp \
	opt_algebraic.cpp \
	opt_constant_folding.cpp \
	opt_constant_propagation.cpp \
	opt_constant_variable.cpp \
	opt_copy_propagation.cpp \
	opt_copy_propagation_elements.cpp \
	opt_dead_code.cpp \
	opt_dead_code_local.cpp \
	opt_dead_functions.cpp \
	opt_discard_simplification.cpp \
	opt_function_inlining.cpp \
	opt_if_simplification.cpp \
	opt_noop_swizzle.cpp \
	opt_redundant_jumps.cpp \
	opt_structure_splitting.cpp \
	opt_swizzle_swizzle.cpp \
	opt_tree_grafting.cpp \
	s_expression.cpp

SRCS.glsl= \
	${SRCS.glsl.c} \
	${SRCS.glsl.cxx}

SRCS.glsl+= \
	builtin_function.cpp
