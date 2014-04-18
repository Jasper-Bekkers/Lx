#pragma once

enum LexerFlags
{
	Lx_ScanDigits  = 1,
	Lx_ScanStrings = 2,
	Lx_ScanStringsCStyleEscapes = 4,
	Lx_ScanSkipPoundComments = 8,
};

enum LexerTokens
{
	LxToken_EndOfFile = -1,
	LxToken_Identifier = -2,
	LxToken_Digits = -3,
	LxToken_String = -4,
	LxToken_Comments = -5,
	LxToken_Invalid = -6
};

struct LexerStringDescription
{
	const char*  start;
	unsigned int length;
};

struct Lexer;
typedef Lexer * Lx;

void lxInit                   (Lx *, const char *string, unsigned int flags);
void lxFinalize               (Lx);
void lxReserve                (Lx, const char *lexeme, int precedence, int token);
void lxSymbol                 (Lx, const char *symbol, int precedence, int token);
void lxScan                   (Lx, int *outtoken);
void lxGetLine                (Lx, int *outline);
void lxGetStringDescription   (Lx, LexerStringDescription *outdesc);
bool lxStringDescriptionEqual (LexerStringDescription left, LexerStringDescription right);
void lxStringDescriptionCopy  (LexerStringDescription *to, LexerStringDescription from);
