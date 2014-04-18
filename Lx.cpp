#include "Lx.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct StringBuffer
{
	char *str;
	unsigned int num;
	unsigned int alloc;
};

typedef StringBuffer *Str;

void strInit(Str *str)
{
	Str s = (Str)calloc(1, sizeof(StringBuffer));

	s->alloc = 0;
	s->num = 0;
	s->str = 0;

	*str = s;
}

void strFinalize(Str s)
{
	free(s->str);
	free(s);
}

void strAppendChar(Str str, char c)
{
	unsigned int k = str->num++;
	if(str->num + 2 > str->alloc)
	{
		str->alloc = str->alloc * 2 + 2;
		str->str = (char*)realloc(str->str, str->alloc);
	}

	str->str[k] = c;
	str->str[k + 1] = 0;
}

struct Lexeme
{
	const char *str;
	int precedence;
	int token;
};

struct Symbol
{
	const char *symbol;
	int precedence;
	int token;
};

struct Lexer
{
	long position;
	long tokenstart;
	const char *string;
	unsigned int flags;

	Lexeme *lexemes;
	unsigned int numlexemes;
	unsigned int alloclexemes;

	Symbol *symbols;
	unsigned int numsymbols;
	unsigned int allocsymbols;

	int peek;
	int line;
	StringBuffer *buffer;
};

static int lxCompareSymbols(const void *left, const void *right)
{
	return ((Symbol*)left)->precedence - ((Symbol*)right)->precedence;
}

static int lxCompareLexemes(const void *left, const void *right)
{
	return ((Lexeme*)left)->precedence - ((Lexeme*)right)->precedence;
}

static bool lxIsWhitespace(int c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool lxIsLetter(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

enum LxDigitBase
{
	LxBaseDecimal,
	LxBaseOctal,
	LxBaseHex,
};

static bool lxIsDigit(int c, LxDigitBase base = LxBaseDecimal)
{
	if(base == LxBaseHex)
	{
		return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
	}
	else if(base == LxBaseOctal)
	{
		return	c >= '0' && c <= '7';
	}
	else
	{
		// checking like this allows for some odd numbers like "1e" or "3-12+e" 
		// but generally serves our purposes for the lexer
		return (c >= '0' && c <= '9') || c == 'e' || c == 'E' || c == '+' || c == '-';
	}
}

static void lxRead(Lx lexer)
{
	lexer->peek = lexer->string[lexer->position];
	lexer->position++;
}

void lxInit(Lx* lexer, const char *string, unsigned int flags)
{
	Lx l = (Lx)calloc(1, sizeof(Lexer));
	l->position = 0;
	l->tokenstart = 0;
	l->string = string;
	assert(l->string);

	l->flags = flags;

	l->numlexemes = 0;
	l->alloclexemes = 0;
	l->lexemes = 0;

	l->numsymbols = 0;
	l->allocsymbols = 0;
	l->symbols = 0;

	l->line = 1;
	l->peek = ' ';
	
	strInit(&l->buffer);

	*lexer = l;
}

void lxFinalize(Lx lexer)
{
	strFinalize(lexer->buffer);
	free(lexer->lexemes);
	free(lexer->symbols);
	free(lexer);
}

void lxReserve(Lx lexer, const char *lexeme, int precedence, int token)
{
	unsigned int k = lexer->numlexemes++;

	if(lexer->numlexemes > lexer->alloclexemes)
	{
		lexer->alloclexemes = lexer->alloclexemes * 2 + 1;
		lexer->lexemes = (Lexeme*)realloc(lexer->lexemes, lexer->alloclexemes * sizeof(Lexeme));
	}

	lexer->lexemes[k].str        = lexeme;
	lexer->lexemes[k].precedence = precedence;
	lexer->lexemes[k].token      = token;

	qsort(lexer->lexemes, lexer->numlexemes, sizeof(Lexeme), lxCompareLexemes);
}

void lxSymbol(Lx lexer, const char *symbol, int precedence, int token)
{
	unsigned int k = lexer->numsymbols++;

	if(lexer->numsymbols > lexer->allocsymbols)
	{
		lexer->allocsymbols = lexer->allocsymbols * 2 + 1;
		lexer->symbols = (Symbol*)realloc(lexer->symbols, lexer->allocsymbols * sizeof(Symbol));
	}

	lexer->symbols[k].symbol = symbol;
	lexer->symbols[k].precedence = precedence;
	lexer->symbols[k].token = token;

	qsort(lexer->symbols, lexer->numsymbols, sizeof(Symbol), lxCompareSymbols);
}

static bool lxIsNewline(char c)
{
	return c == '\r' || c == '\n';
}

static bool lxSkipNewline(Lx lexer)
{
	bool nl = false;

	while(lxIsNewline(lexer->peek))
	{
		lxRead(lexer);
		nl = true;
		lexer->line++;
	}

	return nl;
}

void lxSkipWhitespace(Lx lexer, int *outtoken)
{
	if(*outtoken != LxToken_Invalid) return;

	int previous = lexer->peek;
	for (;; lxRead(lexer))
	{
		if(lexer->peek == 0)
		{
			*outtoken = LxToken_EndOfFile;
			return;
		}
		else if(lxIsWhitespace(lexer->peek))
		{
			if (lxIsNewline(lexer->peek) && !lxIsNewline(previous))
			{
				lexer->line++;
			}

			previous = lexer->peek;
		}
		else break;
	}
}

void lxScanStrings(Lx lexer, int *outtoken)
{
	if(!(lexer->flags & Lx_ScanStrings)) return;
	if(*outtoken != LxToken_Invalid) return;

	if(lexer->peek == '"')
	{
		bool escaped = false;

		do
		{
			escaped = false;

			strAppendChar(lexer->buffer, lexer->peek);
			lxRead(lexer);

			// happens when forgetting to close the string
			if(lexer->peek == 0)
			{
				*outtoken = LxToken_Invalid;
				return;
			}

			if(lexer->flags & Lx_ScanStringsCStyleEscapes)
			{
				if(lexer->peek == '\\')
				{
					// append the '\\'
					strAppendChar(lexer->buffer, lexer->peek);
					lxRead(lexer);

					if(lexer->peek == '"')
					{
						// append the '"'
						strAppendChar(lexer->buffer, lexer->peek);
						lxRead(lexer);

						escaped = true;
					}
				}
			}
		}
		while(lexer->peek != '"' || escaped);

		// for the closing '"'
		strAppendChar(lexer->buffer, lexer->peek);
		lxRead(lexer);

		*outtoken = LxToken_String;
	}
}

void lxScanSymbols(Lx lexer, int *outtoken)
{
	if(*outtoken != LxToken_Invalid) return;

	for(unsigned int i = 0; i < lexer->numsymbols; i++)
	{
		unsigned int len = strlen(lexer->symbols[i].symbol);

		for(unsigned int k = 0; k < len; k++)
		{
			if(lexer->peek == lexer->symbols[i].symbol[k])
			{
				strAppendChar(lexer->buffer, lexer->peek);
				lxRead(lexer);

				if(k + 1 == len)
				{
					*outtoken = lexer->symbols[i].token;
					return;
				}
			}
		}
	}
}

void lxScanDigits(Lx lexer, int *outtoken)
{
	if(!(lexer->flags & Lx_ScanDigits)) return;
	if(*outtoken != LxToken_Invalid) return;

	bool isnegative = false;

	if (lexer->peek == '-')
	{
		isnegative = true;
		strAppendChar(lexer->buffer, lexer->peek);
		lxRead(lexer);
	}

	if(lxIsDigit(lexer->peek) || lexer->peek == '.')
	{
		unsigned int periods = lexer->peek == '.' ? 1 : 0;
		bool hasdigits = false; // exists to not match just a '.'
		LxDigitBase base = LxBaseDecimal;

		if(lexer->peek == '0')
		{
			hasdigits = true;
			strAppendChar(lexer->buffer, lexer->peek);
			lxRead(lexer);
			if(lexer->peek != '.') base = LxBaseOctal;
			else if(lexer->peek == 'x') base = LxBaseHex;
		}

		if (!hasdigits || lxIsDigit(lexer->peek) || lexer->peek == '.')
		{
			do
			{
				hasdigits |= lxIsDigit(lexer->peek, base);
				strAppendChar(lexer->buffer, lexer->peek);
				lxRead(lexer);

				if (lxIsDigit(lexer->peek, base)) continue;

				if (base == LxBaseDecimal)
				{
					// floats
					if (lexer->peek == '.') periods++;
					if (lexer->peek == '.' && periods == 1) continue;
				}

				break;
			} 
			while (true);
		}

		if(hasdigits) *outtoken = LxToken_Digits;
	}
}

void lxScanIdentifiers(Lx lexer, int *outtoken)
{
	if(*outtoken != LxToken_Invalid) return;

	if(lxIsLetter(lexer->peek))
	{
		do
		{
			strAppendChar(lexer->buffer, lexer->peek);
			lxRead(lexer);

			if(lexer->peek == 0) break;
		}
		while(lxIsLetter(lexer->peek) || lxIsDigit(lexer->peek));

		for(unsigned int i = 0; i < lexer->numlexemes; i++)
		{
			if(!strcmp(lexer->buffer->str, lexer->lexemes[i].str))
			{
				*outtoken = lexer->lexemes[i].token;
				return;
			}
		}

		*outtoken = LxToken_Identifier;
		return;
	}
}

void lxSkipComments(Lx lexer, int *outtoken)
{
	if (!(lexer->flags & Lx_ScanSkipPoundComments)) return;

	if (lexer->peek == '#')
	{
		*outtoken = LxToken_Comments;

		do
		{
			strAppendChar(lexer->buffer, lexer->peek);
			lxRead(lexer);
		} 
		while (lexer->peek != '\r');

		lxRead(lexer);
		lxRead(lexer);
	}
}

void lxScan(Lx lexer, int *outtoken)
{
	if(lexer->buffer->alloc)
	{
		lexer->buffer->str[0] = 0;
		lexer->buffer->num = 0;
	}

	if (lexer->flags & Lx_ScanSkipPoundComments)
	{
		do
		{
			*outtoken = LxToken_Invalid;
			lxSkipWhitespace(lexer, outtoken);
			lxSkipComments(lexer, outtoken);
		}
		while (*outtoken == LxToken_Comments);
	}

	*outtoken = LxToken_Invalid;
	lxSkipWhitespace(lexer, outtoken);

	lexer->tokenstart = lexer->position - 1;

	lxScanDigits(lexer, outtoken);
	lxScanStrings(lexer, outtoken);
	lxScanSymbols(lexer, outtoken);
	lxScanIdentifiers(lexer, outtoken);

	if(*outtoken == LxToken_Invalid)
	{
		*outtoken = LxToken_EndOfFile;
	}
}

void lxGetLine(Lx lexer, int *outline)
{
	*outline = lexer->line;
}

void lxGetStringDescription(Lx lexer, LexerStringDescription *outdesc)
{
	outdesc->start  = lexer->string + lexer->tokenstart;
	outdesc->length = lexer->position - lexer->tokenstart - 1;
}

bool lxStringDescriptionEqual(LexerStringDescription left, LexerStringDescription right)
{
	if(left.length != right.length) return false;

	for(unsigned int i = 0; i < left.length; i++)
	{
		if(left.start[i] != right.start[i]) return false;
	}

	return true;
}

void lxStringDescriptionCopy(LexerStringDescription *to, LexerStringDescription from)
{
	to->start = from.start;
	to->length = from.length;
}
