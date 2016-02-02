#	$NetBSD: libglsl.mk,v 1.1 2014/12/18 06:24:28 mrg Exp $

LIBGLSL_GENERATED_CXX_FILES = \
	glsl_lexer.cpp \
	glsl_parser.cpp 

LIBGLSL_FILES = \
	ast_array_index.cpp \
	ast_expr.cpp \
	ast_function.cpp \
	ast_to_hir.cpp \
	ast_type.cpp \
	builtin_functions.cpp \
	builtin_types.cpp \
	builtin_variables.cpp \
	glsl_parser_extras.cpp \
	glsl_types.cpp \
	glsl_symbol_table.cpp \
	hir_field_selection.cpp \
	ir_basic_block.cpp \
	ir_builder.cpp \
	ir_clone.cpp \
	ir_constant_expression.cpp \
	ir.cpp \
	ir_equals.cpp \
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
	ir_variable_refcount.cpp \
	linker.cpp \
	link_atomics.cpp \
	link_functions.cpp \
	link_interface_blocks.cpp \
	link_uniforms.cpp \
	link_uniform_initializers.cpp \
	link_uniform_block_active_visitor.cpp \
	link_uniform_blocks.cpp \
	link_varyings.cpp \
	loop_analysis.cpp \
	loop_controls.cpp \
	loop_unroll.cpp \
	lower_clip_distance.cpp \
	lower_discard.cpp \
	lower_discard_flow.cpp \
	lower_if_to_cond_assign.cpp \
	lower_instructions.cpp \
	lower_jumps.cpp \
	lower_mat_op_to_vec.cpp \
	lower_noise.cpp \
	lower_offset_array.cpp \
	lower_packed_varyings.cpp \
	lower_named_interface_blocks.cpp \
	lower_packing_builtins.cpp \
	lower_texture_projection.cpp \
	lower_variable_index_to_cond_assign.cpp \
	lower_vec_index_to_cond_assign.cpp \
	lower_vec_index_to_swizzle.cpp \
	lower_vector.cpp \
	lower_vector_insert.cpp \
	lower_vertex_id.cpp \
	lower_output_reads.cpp \
	lower_ubo_reference.cpp \
	opt_algebraic.cpp \
	opt_array_splitting.cpp \
	opt_constant_folding.cpp \
	opt_constant_propagation.cpp \
	opt_constant_variable.cpp \
	opt_copy_propagation.cpp \
	opt_copy_propagation_elements.cpp \
	opt_cse.cpp \
	opt_dead_builtin_varyings.cpp \
	opt_dead_code.cpp \
	opt_dead_code_local.cpp \
	opt_dead_functions.cpp \
	opt_flatten_nested_if_blocks.cpp \
	opt_flip_matrices.cpp \
	opt_function_inlining.cpp \
	opt_if_simplification.cpp \
	opt_noop_swizzle.cpp \
	opt_rebalance_tree.cpp \
	opt_redundant_jumps.cpp \
	opt_structure_splitting.cpp \
	opt_swizzle_swizzle.cpp \
	opt_tree_grafting.cpp \
	opt_vectorize.cpp \
	s_expression.cpp \
	strtod.c

LIBGLCPP_GENERATED_FILES = \
	glcpp-lex.c \
	glcpp-parse.c

LIBGLCPP_FILES = \
	pp.c

.PATH:	${X11SRCDIR.MesaLib}/src/glsl
.PATH:	${X11SRCDIR.MesaLib}/src/glsl/glcpp

SRCS+=	${LIBGLSL_GENERATED_CXX_FILES} \
	${LIBGLSL_FILES} \
	${LIBGLCPP_GENERATED_FILES} \
	${LIBGLCPP_FILES}
