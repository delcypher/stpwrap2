%option prefix="ol"

%{
#include <string>
#include "output-tokens.h"

namespace SMTLIBOutput
{
	std::string* foundString=0;
}

using namespace SMTLIBOutput;

//Function to use if something goes wrong during lexing
extern void exitUnknown();

/* Macro for saving a C++ string and keeping memory tidy */
#define SAVE_TOKEN if(foundString!=0) delete foundString; foundString = new std::string(yytext,yyleng);
%}

/* yywrap returns 1, so we only parse one file */
%option noyywrap

/* Array identifier regex */
ARRAY_ID	[A-Za-z][A-Za-z0-9_.-]*
HEXD		[a-fA-F0-9]

/* (av) array value condition */
%x av
%%

<*>[ \n\t] { /* ignore whitespace */}
<INITIAL>^sat[\n]		{ECHO ; return T_SAT;}
<INITIAL>^unsat[\n]		{ECHO ; return T_UNSAT;}
<INITIAL>^unknown[\n]	{ECHO ; return T_UNKNOWN;}
<INITIAL>^ASSERT		{BEGIN(av); return T_ASSERT;}

<av>"("		{return T_LBRACKET;}
<av>")"		{return T_RBRACKET;}
<av>0x{HEXD}+	{SAVE_TOKEN; return T_HEX;}
<av>{ARRAY_ID}	{ SAVE_TOKEN ;return T_ARRAYID;}
<av>"["		{return T_LSQRBRACKET;}
<av>"]"		{return T_RSQRTBRACKET;}
<av>"="		{return T_EQUAL;}
<av>";"		{ /* End of assert statement  */ BEGIN(INITIAL);}

<*>. 		{/* invalid */ fprintf(stderr,"Invalid token: `%s`\n",yytext); exitUnknown();}
<<EOF>>		{ return T_EOF;}

%%%
