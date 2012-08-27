%option prefix="il"
%{
#include <string>
#include "input-tokens.h"

namespace SMTLIBInput
{
	std::string* foundString=0;
}

using namespace SMTLIBInput;

/* Macro for saving a C++ string and keeping memory tidy */
#define SAVE_TOKEN if(foundString!=0) delete foundString; foundString = new std::string(yytext,yyleng);
%}

/* Array identifier regex */
ARRAY_ID	[A-Za-z][A-Za-z0-9_.-]*

/* yywrap returns 1, so we only parse one file */
%option noyywrap

/* (gv) get-value condition, (bv) bitvector condition */
%x gv bv comment
%%

<INITIAL>^"("set-option[ ]+:[a-z_-]+[ ]+([a-z0-9]+)?[ ]*")"[\n]	{ /* remove set-option commands */ }
<*>^"("check-sat[ ]*")"$	{ /* pass through and activate gv */ ECHO ; BEGIN(gv); return T_CHECKSAT; }
<INITIAL>.|\n	{ /*Show other commands */ ; ECHO; }

<gv,bv>[ \t\n]	{/* ignore spaces and new lines */ }
<gv>"(_"	{ /* In bitvector mode,needed as ARRAY_ID matches bv0 */ BEGIN(bv); }
<gv>"("		{return T_LBRACKET;}
<gv>")"		{return T_RBRACKET;}
<gv>"get-value"	{return T_GETVALUE;}
<gv>"select"	{return T_SELECT;}
<gv>"bv"	{return T_BV;}
<gv>"(exit)"	{ /* Pass through and end get-value parsing */ ECHO ; return T_EXIT;}
<gv>{ARRAY_ID}	{ SAVE_TOKEN ;return T_ARRAYID;}
<gv,bv>[0-9]+	{ SAVE_TOKEN; return T_DECIMAL;}
<gv>^";"		{/* ignore comments */ BEGIN(comment);}
<<EOF>>		{ return T_EOF;}

<bv>"bv"	{return T_BV;}
<bv>")"		{BEGIN(gv);}

<comment>[\n]	{/* ignore new line and switch off comment mode */ BEGIN(gv);}
<comment>.*		{/* ignore comment */}

%%%
