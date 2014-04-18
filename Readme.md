Lx
========

Lx is a simple lexer that has been used to parse a variety of simple languages.

Basic usage
========

```c
lxInit(lexer, buffer, Lx_ScanDigits | Lx_ScanStrings | Lx_ScanStringsCStyleEscapes);

// ...
lxSymbol(lexer, "++", 900, T_Decrement);

lxReserve(lexer, "class",  100, T_Class);
// A bunch of keywords
lxReserve(lexer, "string",  1500, T_String);

do
{
	// for each successive token, call
	lxScan(lexer, &look);

	// store off the textual description of the current lexeme
	lxGetStringDescription(lexer, &desc);

	// process the token
} while(look != LxToken_EndOfFile);

lxFinalize(c->lexer);
```

Of course a typical parser would implement something more traditional, and easier to work with.

```c
// typical implementations of 'match' and 'move' parser operations
static void dcMove(Dc c)
{
	// store lookahead and textual description
	lxScan(c->lexer, &c->look);
	lxGetStringDescription(c->lexer, &c->desc);
}

static void dcMatch(Dc c, int expectedToken)
{
	if(c->look == expectedToken)
	{
		dcMove(c);
	}
	else if(c->look == LxToken_Invalid)
	{
		int line;
		lxGetLine(c->lexer, &line);
		dcError(c, EL_Error, "Parse error on line %d encountered token LxToken_Invalid (%.*s)\n", line, c->desc.length, c->desc.start);
	}
	else
	{
		int line;
		lxGetLine(c->lexer, &line);
		dcError(c, EL_Error, "Syntax error on line %d expected token %s got %s (context: %.*s)\n", 
			line, dcTokenToString(expectedToken), dcTokenToString(c->look) , c->desc.length, c->desc.start);
	}
}
```


Options
=======

`lxInit` takes a string buffer and some flags, `Lx_ScanDigits` indicates that we should be lexing digits (floats, hex, int, etc.), `Lx_ScanStrings` indicates that we match strings in quotes and `Lx_ScanStringsCStyleEscapes` allows for c-style escapes. `Lx_ScanSkipPoundComments` skips single-line '#'-style comments that can start at any point in the document and run until the end of the line.

Although a `LxToken_Comments` token is defined, it's not supposed to be returned to the calling code, and therefor doesn't need to be skipped explicitly inside the parser.