//===--- Format.h - Format C++ code -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Various functions to configurably format source code.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FORMAT_FORMAT_H
#define LLVM_CLANG_FORMAT_FORMAT_H

#include "clang/Basic/LangOptions.h"
#include "clang/Tooling/Core/Replacement.h"
#include "llvm/ADT/ArrayRef.h"
#include <system_error>

namespace clang {

class Lexer;
class SourceManager;
class DiagnosticConsumer;

namespace format {

enum class ParseError { Success = 0, Error, Unsuitable };
class ParseErrorCategory final : public std::error_category {
public:
  const char *name() const LLVM_NOEXCEPT override;
  std::string message(int EV) const override;
};
const std::error_category &getParseCategory();
std::error_code make_error_code(ParseError e);

/// \brief The \c FormatStyle is used to configure the formatting to follow
/// specific guidelines.
struct FormatStyle {
  /// \brief Supported languages. When stored in a configuration file, specifies
  /// the language, that the configuration targets. When passed to the
  /// reformat() function, enables syntax features specific to the language.
  enum LanguageKind {
    /// Do not use.
    LK_None,
    /// Should be used for C, C++, ObjectiveC, ObjectiveC++.
    LK_Cpp,
    /// Should be used for Java.
    LK_Java,
    /// Should be used for JavaScript.
    LK_JavaScript,
    /// Should be used for Protocol Buffers
    /// (https://developers.google.com/protocol-buffers/).
    LK_Proto
  };

  /// \brief Language, this format style is targeted at.
  LanguageKind Language;

  /// \brief The column limit.
  ///
  /// A column limit of \c 0 means that there is no column limit. In this case,
  /// clang-format will respect the input's line breaking decisions within
  /// statements unless they contradict other rules.
  unsigned ColumnLimit;

  /// \brief The maximum number of consecutive empty lines to keep.
  unsigned MaxEmptyLinesToKeep;

  /// \brief If true, empty lines at the start of blocks are kept.
  bool KeepEmptyLinesAtTheStartOfBlocks;

  /// \brief The penalty for each line break introduced inside a comment.
  unsigned PenaltyBreakComment;

  /// \brief The penalty for each line break introduced inside a string literal.
  unsigned PenaltyBreakString;

  /// \brief The penalty for each character outside of the column limit.
  unsigned PenaltyExcessCharacter;

  /// \brief The penalty for breaking before the first \c <<.
  unsigned PenaltyBreakFirstLessLess;

  /// \brief The penalty for breaking a function call after "call(".
  unsigned PenaltyBreakBeforeFirstCallParameter;

  /// \brief The & and * alignment style.
  enum PointerAlignmentStyle {
    /// Align pointer to the left.
    PAS_Left,
    /// Align pointer to the right.
    PAS_Right,
    /// Align pointer in the middle.
    PAS_Middle
  };

  /// Pointer and reference alignment style.
  PointerAlignmentStyle PointerAlignment;

  /// \brief If \c true, analyze the formatted file for the most common
  /// alignment of & and *. \c PointerAlignment is then used only as fallback.
  bool DerivePointerAlignment;

  /// \brief The extra indent or outdent of access modifiers, e.g. \c public:.
  int AccessModifierOffset;

  /// \brief Supported language standards.
  enum LanguageStandard {
    /// Use C++03-compatible syntax.
    LS_Cpp03,
    /// Use features of C++11 (e.g. \c A<A<int>> instead of
    /// <tt>A<A<int> ></tt>).
    LS_Cpp11,
    /// Automatic detection based on the input.
    LS_Auto
  };

  /// \brief Format compatible with this standard, e.g. use
  /// <tt>A<A<int> ></tt> instead of \c A<A<int>> for LS_Cpp03.
  LanguageStandard Standard;

  /// \brief Indent case labels one level from the switch statement.
  ///
  /// When \c false, use the same indentation level as for the switch statement.
  /// Switch statement body is always indented one level more than case labels.
  bool IndentCaseLabels;

  /// \brief Indent if a function definition or declaration is wrapped after the
  /// type.
  bool IndentWrappedFunctionNames;

  /// \brief Different ways to indent namespace contents.
  enum NamespaceIndentationKind {
    /// Don't indent in namespaces.
    NI_None,
    /// Indent only in inner namespaces (nested in other namespaces).
    NI_Inner,
    /// Indent in all namespaces.
    NI_All
  };

  /// \brief The indentation used for namespaces.
  NamespaceIndentationKind NamespaceIndentation;

  /// \brief The number of spaces before trailing line comments
  /// (\c // - comments).
  ///
  /// This does not affect trailing block comments (\c /**/ - comments) as those
  /// commonly have different usage patterns and a number of special cases.
  unsigned SpacesBeforeTrailingComments;

  /// \brief If \c false, a function declaration's or function definition's
  /// parameters will either all be on the same line or will have one line each.
  bool BinPackParameters;

  /// \brief If \c false, a function call's arguments will either be all on the
  /// same line or will have one line each.
  bool BinPackArguments;

  /// \brief If \c true, clang-format detects whether function calls and
  /// definitions are formatted with one parameter per line.
  ///
  /// Each call can be bin-packed, one-per-line or inconclusive. If it is
  /// inconclusive, e.g. completely on one line, but a decision needs to be
  /// made, clang-format analyzes whether there are other bin-packed cases in
  /// the input file and act accordingly.
  ///
  /// NOTE: This is an experimental flag, that might go away or be renamed. Do
  /// not use this in config files, etc. Use at your own risk.
  bool ExperimentalAutoDetectBinPacking;

  /// \brief Allow putting all parameters of a function declaration onto
  /// the next line even if \c BinPackParameters is \c false.
  bool AllowAllParametersOfDeclarationOnNextLine;

  /// \brief Penalty for putting the return type of a function onto its own
  /// line.
  unsigned PenaltyReturnTypeOnItsOwnLine;

  /// \brief If the constructor initializers don't fit on a line, put each
  /// initializer on its own line.
  bool ConstructorInitializerAllOnOneLineOrOnePerLine;

  /// \brief Always break constructor initializers before commas and align
  /// the commas with the colon.
  bool BreakConstructorInitializersBeforeComma;

  /// \brief Allows contracting simple braced statements to a single line.
  ///
  /// E.g., this allows <tt>if (a) { return; }</tt> to be put on a single line.
  bool AllowShortBlocksOnASingleLine;

  /// \brief If \c true, <tt>if (a) return;</tt> can be put on a single
  /// line.
  bool AllowShortIfStatementsOnASingleLine;

  /// \brief If \c true, <tt>while (true) continue;</tt> can be put on a
  /// single line.
  bool AllowShortLoopsOnASingleLine;

  /// \brief If \c true, short case labels will be contracted to a single line.
  bool AllowShortCaseLabelsOnASingleLine;

  /// \brief Different styles for merging short functions containing at most one
  /// statement.
  enum ShortFunctionStyle {
    /// \brief Never merge functions into a single line.
    SFS_None,
    /// \brief Only merge functions defined inside a class.
    SFS_Inline,
    /// \brief Only merge empty functions.
    SFS_Empty,
    /// \brief Merge all functions fitting on a single line.
    SFS_All,
  };

  /// \brief Dependent on the value, <tt>int f() { return 0; }</tt> can be put
  /// on a single line.
  ShortFunctionStyle AllowShortFunctionsOnASingleLine;

  /// \brief Add a space after \c @property in Objective-C, i.e. use
  /// <tt>\@property (readonly)</tt> instead of <tt>\@property(readonly)</tt>.
  bool ObjCSpaceAfterProperty;

  /// \brief Add a space in front of an Objective-C protocol list, i.e. use
  /// <tt>Foo <Protocol></tt> instead of \c Foo<Protocol>.
  bool ObjCSpaceBeforeProtocolList;

  /// \brief If \c true, horizontally aligns arguments after an open bracket.
  ///
  /// This applies to round brackets (parentheses), angle brackets and square
  /// brackets. This will result in formattings like
  /// \code
  /// someLongFunction(argument1,
  ///                  argument2);
  /// \endcode
  bool AlignAfterOpenBracket;

  /// \brief If \c true, horizontally align operands of binary and ternary
  /// expressions.
  bool AlignOperands;

  /// \brief If \c true, aligns trailing comments.
  bool AlignTrailingComments;

  /// \brief If \c true, aligns escaped newlines as far left as possible.
  /// Otherwise puts them into the right-most column.
  bool AlignEscapedNewlinesLeft;

  /// \brief The number of columns to use for indentation.
  unsigned IndentWidth;

  /// \brief The number of columns used for tab stops.
  unsigned TabWidth;

  /// \brief The number of characters to use for indentation of constructor
  /// initializer lists.
  unsigned ConstructorInitializerIndentWidth;

  /// \brief The number of characters to use for indentation of ObjC blocks.
  unsigned ObjCBlockIndentWidth;

  /// \brief If \c true, always break after function definition return types.
  ///
  /// More truthfully called 'break before the identifier following the type
  /// in a function definition'. PenaltyReturnTypeOnItsOwnLine becomes
  /// irrelevant.
  bool AlwaysBreakAfterDefinitionReturnType;

  /// \brief If \c true, always break after the <tt>template<...></tt> of a
  /// template declaration.
  bool AlwaysBreakTemplateDeclarations;

  /// \brief If \c true, always break before multiline string literals.
  bool AlwaysBreakBeforeMultilineStrings;

  /// \brief Different ways to use tab in formatting.
  enum UseTabStyle {
    /// Never use tab.
    UT_Never,
    /// Use tabs only for indentation.
    UT_ForIndentation,
    /// Use tabs whenever we need to fill whitespace that spans at least from
    /// one tab stop to the next one.
    UT_Always
  };

  /// \brief The way to use tab characters in the resulting file.
  UseTabStyle UseTab;

  /// \brief The style of breaking before or after binary operators.
  enum BinaryOperatorStyle {
    /// Break after operators.
    BOS_None,
    /// Break before operators that aren't assignments.
    BOS_NonAssignment,
    /// Break before operators.
    BOS_All,
  };

  /// \brief The way to wrap binary operators.
  BinaryOperatorStyle BreakBeforeBinaryOperators;

  /// \brief If \c true, ternary operators will be placed after line breaks.
  bool BreakBeforeTernaryOperators;

  /// \brief Different ways to attach braces to their surrounding context.
  enum BraceBreakingStyle {
    /// Always attach braces to surrounding context.
    BS_Attach,
    /// Like \c Attach, but break before braces on function, namespace and
    /// class definitions.
    BS_Linux,
    /// Like \c Attach, but break before function definitions, and 'else'.
    BS_Stroustrup,
    /// Always break before braces.
    BS_Allman,
    /// Always break before braces and add an extra level of indentation to
    /// braces of control statements, not to those of class, function
    /// or other definitions.
    BS_GNU
  };

  /// \brief The brace breaking style to use.
  BraceBreakingStyle BreakBeforeBraces;

  /// \brief If \c true, format braced lists as best suited for C++11 braced
  /// lists.
  ///
  /// Important differences:
  /// - No spaces inside the braced list.
  /// - No line break before the closing brace.
  /// - Indentation with the continuation indent, not with the block indent.
  ///
  /// Fundamentally, C++11 braced lists are formatted exactly like function
  /// calls would be formatted in their place. If the braced list follows a name
  /// (e.g. a type or variable name), clang-format formats as if the \c {} were
  /// the parentheses of a function call with that name. If there is no name,
  /// a zero-length name is assumed.
  bool Cpp11BracedListStyle;

  /// \brief If \c true, spaces will be inserted after '(' and before ')'.
  bool SpacesInParentheses;

  /// \brief If \c true, spaces will be inserted after '<' and before '>' in
  /// template argument lists
  bool SpacesInAngles;

  /// \brief If \c true, spaces will be inserted after '[' and before ']'.
  bool SpacesInSquareBrackets;

  /// \brief If \c true, spaces may be inserted into '()'.
  bool SpaceInEmptyParentheses;

  /// \brief If \c true, spaces are inserted inside container literals (e.g.
  /// ObjC and Javascript array and dict literals).
  bool SpacesInContainerLiterals;

  /// \brief If \c true, spaces may be inserted into C style casts.
  bool SpacesInCStyleCastParentheses;

  /// \brief If \c true, a space may be inserted after C style casts.
  bool SpaceAfterCStyleCast;

  /// \brief Different ways to put a space before opening parentheses.
  enum SpaceBeforeParensOptions {
    /// Never put a space before opening parentheses.
    SBPO_Never,
    /// Put a space before opening parentheses only after control statement
    /// keywords (<tt>for/if/while...</tt>).
    SBPO_ControlStatements,
    /// Always put a space before opening parentheses, except when it's
    /// prohibited by the syntax rules (in function-like macro definitions) or
    /// when determined by other style rules (after unary operators, opening
    /// parentheses, etc.)
    SBPO_Always
  };

  /// \brief Defines in which cases to put a space before opening parentheses.
  SpaceBeforeParensOptions SpaceBeforeParens;

  /// \brief If \c false, spaces will be removed before assignment operators.
  bool SpaceBeforeAssignmentOperators;

  /// \brief Indent width for line continuations.
  unsigned ContinuationIndentWidth;

  /// \brief A regular expression that describes comments with special meaning,
  /// which should not be split into lines or otherwise changed.
  std::string CommentPragmas;

  /// \brief Disables formatting at all.
  bool DisableFormat;

  /// \brief A vector of macros that should be interpreted as foreach loops
  /// instead of as function calls.
  ///
  /// These are expected to be macros of the form:
  /// \code
  /// FOREACH(<variable-declaration>, ...)
  ///   <loop-body>
  /// \endcode
  ///
  /// For example: BOOST_FOREACH.
  std::vector<std::string> ForEachMacros;

  bool operator==(const FormatStyle &R) const {
    return AccessModifierOffset == R.AccessModifierOffset &&
           AlignAfterOpenBracket == R.AlignAfterOpenBracket &&
           AlignOperands == R.AlignOperands &&
           AlignEscapedNewlinesLeft == R.AlignEscapedNewlinesLeft &&
           AlignTrailingComments == R.AlignTrailingComments &&
           AllowAllParametersOfDeclarationOnNextLine ==
               R.AllowAllParametersOfDeclarationOnNextLine &&
           AllowShortFunctionsOnASingleLine ==
               R.AllowShortFunctionsOnASingleLine &&
           AllowShortBlocksOnASingleLine == R.AllowShortBlocksOnASingleLine &&
           AllowShortIfStatementsOnASingleLine ==
               R.AllowShortIfStatementsOnASingleLine &&
           AllowShortLoopsOnASingleLine == R.AllowShortLoopsOnASingleLine &&
           AlwaysBreakAfterDefinitionReturnType ==
               R.AlwaysBreakAfterDefinitionReturnType &&
           AlwaysBreakTemplateDeclarations ==
               R.AlwaysBreakTemplateDeclarations &&
           AlwaysBreakBeforeMultilineStrings ==
               R.AlwaysBreakBeforeMultilineStrings &&
           BinPackParameters == R.BinPackParameters &&
           BinPackArguments == R.BinPackArguments &&
           BreakBeforeBinaryOperators == R.BreakBeforeBinaryOperators &&
           BreakBeforeTernaryOperators == R.BreakBeforeTernaryOperators &&
           BreakBeforeBraces == R.BreakBeforeBraces &&
           BreakConstructorInitializersBeforeComma ==
               R.BreakConstructorInitializersBeforeComma &&
           ColumnLimit == R.ColumnLimit &&
           ConstructorInitializerAllOnOneLineOrOnePerLine ==
               R.ConstructorInitializerAllOnOneLineOrOnePerLine &&
           ConstructorInitializerIndentWidth ==
               R.ConstructorInitializerIndentWidth &&
           DerivePointerAlignment == R.DerivePointerAlignment &&
           ExperimentalAutoDetectBinPacking ==
               R.ExperimentalAutoDetectBinPacking &&
           IndentCaseLabels == R.IndentCaseLabels &&
           IndentWrappedFunctionNames == R.IndentWrappedFunctionNames &&
           IndentWidth == R.IndentWidth && Language == R.Language &&
           MaxEmptyLinesToKeep == R.MaxEmptyLinesToKeep &&
           KeepEmptyLinesAtTheStartOfBlocks ==
               R.KeepEmptyLinesAtTheStartOfBlocks &&
           NamespaceIndentation == R.NamespaceIndentation &&
           ObjCBlockIndentWidth == R.ObjCBlockIndentWidth &&
           ObjCSpaceAfterProperty == R.ObjCSpaceAfterProperty &&
           ObjCSpaceBeforeProtocolList == R.ObjCSpaceBeforeProtocolList &&
           PenaltyBreakComment == R.PenaltyBreakComment &&
           PenaltyBreakFirstLessLess == R.PenaltyBreakFirstLessLess &&
           PenaltyBreakString == R.PenaltyBreakString &&
           PenaltyExcessCharacter == R.PenaltyExcessCharacter &&
           PenaltyReturnTypeOnItsOwnLine == R.PenaltyReturnTypeOnItsOwnLine &&
           PointerAlignment == R.PointerAlignment &&
           SpacesBeforeTrailingComments == R.SpacesBeforeTrailingComments &&
           Cpp11BracedListStyle == R.Cpp11BracedListStyle &&
           Standard == R.Standard && TabWidth == R.TabWidth &&
           UseTab == R.UseTab && SpacesInParentheses == R.SpacesInParentheses &&
           SpacesInSquareBrackets == R.SpacesInSquareBrackets &&
           SpacesInAngles == R.SpacesInAngles &&
           SpaceInEmptyParentheses == R.SpaceInEmptyParentheses &&
           SpacesInContainerLiterals == R.SpacesInContainerLiterals &&
           SpacesInCStyleCastParentheses == R.SpacesInCStyleCastParentheses &&
           SpaceAfterCStyleCast == R.SpaceAfterCStyleCast &&
           SpaceBeforeParens == R.SpaceBeforeParens &&
           SpaceBeforeAssignmentOperators == R.SpaceBeforeAssignmentOperators &&
           ContinuationIndentWidth == R.ContinuationIndentWidth &&
           CommentPragmas == R.CommentPragmas &&
           ForEachMacros == R.ForEachMacros;
  }
};

/// \brief Returns a format style complying with the LLVM coding standards:
/// http://llvm.org/docs/CodingStandards.html.
FormatStyle getLLVMStyle();

/// \brief Returns a format style complying with one of Google's style guides:
/// http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml.
/// http://google-styleguide.googlecode.com/svn/trunk/javascriptguide.xml.
/// https://developers.google.com/protocol-buffers/docs/style.
FormatStyle getGoogleStyle(FormatStyle::LanguageKind Language);

/// \brief Returns a format style complying with Chromium's style guide:
/// http://www.chromium.org/developers/coding-style.
FormatStyle getChromiumStyle(FormatStyle::LanguageKind Language);

/// \brief Returns a format style complying with Mozilla's style guide:
/// https://developer.mozilla.org/en-US/docs/Developer_Guide/Coding_Style.
FormatStyle getMozillaStyle();

/// \brief Returns a format style complying with Webkit's style guide:
/// http://www.webkit.org/coding/coding-style.html
FormatStyle getWebKitStyle();

/// \brief Returns a format style complying with GNU Coding Standards:
/// http://www.gnu.org/prep/standards/standards.html
FormatStyle getGNUStyle();

/// \brief Returns style indicating formatting should be not applied at all.
FormatStyle getNoStyle();

/// \brief Gets a predefined style for the specified language by name.
///
/// Currently supported names: LLVM, Google, Chromium, Mozilla. Names are
/// compared case-insensitively.
///
/// Returns \c true if the Style has been set.
bool getPredefinedStyle(StringRef Name, FormatStyle::LanguageKind Language,
                        FormatStyle *Style);

/// \brief Parse configuration from YAML-formatted text.
///
/// Style->Language is used to get the base style, if the \c BasedOnStyle
/// option is present.
///
/// When \c BasedOnStyle is not present, options not present in the YAML
/// document, are retained in \p Style.
std::error_code parseConfiguration(StringRef Text, FormatStyle *Style);

/// \brief Gets configuration in a YAML string.
std::string configurationAsText(const FormatStyle &Style);

/// \brief Reformats the given \p Ranges in the token stream coming out of
/// \c Lex.
///
/// DEPRECATED: Do not use.
tooling::Replacements reformat(const FormatStyle &Style, Lexer &Lex,
                               SourceManager &SourceMgr,
                               ArrayRef<CharSourceRange> Ranges);

/// \brief Reformats the given \p Ranges in the file \p ID.
///
/// Each range is extended on either end to its next bigger logic unit, i.e.
/// everything that might influence its formatting or might be influenced by its
/// formatting.
///
/// Returns the \c Replacements necessary to make all \p Ranges comply with
/// \p Style.
tooling::Replacements reformat(const FormatStyle &Style,
                               SourceManager &SourceMgr, FileID ID,
                               ArrayRef<CharSourceRange> Ranges);

/// \brief Reformats the given \p Ranges in \p Code.
///
/// Otherwise identical to the reformat() function consuming a \c Lexer.
tooling::Replacements reformat(const FormatStyle &Style, StringRef Code,
                               ArrayRef<tooling::Range> Ranges,
                               StringRef FileName = "<stdin>");

/// \brief Returns the \c LangOpts that the formatter expects you to set.
///
/// \param Style determines specific settings for lexing mode.
LangOptions getFormattingLangOpts(const FormatStyle &Style = getLLVMStyle());

/// \brief Description to be used for help text for a llvm::cl option for
/// specifying format style. The description is closely related to the operation
/// of getStyle().
extern const char *StyleOptionHelpDescription;

/// \brief Construct a FormatStyle based on \c StyleName.
///
/// \c StyleName can take several forms:
/// \li "{<key>: <value>, ...}" - Set specic style parameters.
/// \li "<style name>" - One of the style names supported by
/// getPredefinedStyle().
/// \li "file" - Load style configuration from a file called '.clang-format'
/// located in one of the parent directories of \c FileName or the current
/// directory if \c FileName is empty.
///
/// \param[in] StyleName Style name to interpret according to the description
/// above.
/// \param[in] FileName Path to start search for .clang-format if \c StyleName
/// == "file".
/// \param[in] FallbackStyle The name of a predefined style used to fallback to
/// in case the style can't be determined from \p StyleName.
///
/// \returns FormatStyle as specified by \c StyleName. If no style could be
/// determined, the default is LLVM Style (see getLLVMStyle()).
FormatStyle getStyle(StringRef StyleName, StringRef FileName,
                     StringRef FallbackStyle);

} // end namespace format
} // end namespace clang

namespace std {
template <>
struct is_error_code_enum<clang::format::ParseError> : std::true_type {};
}

#endif // LLVM_CLANG_FORMAT_FORMAT_H
