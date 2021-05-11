/**
 * simple LL(1) expression parser
 *
 * @author Tobias Weber
 * @date 14-mar-2020, 8-may-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * References:
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 *	- https://de.wikipedia.org/wiki/LL(k)-Grammatik
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "string.h"
#include "expr_parser.h"


// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

enum Token
{
	TOK_VALUE   = 1000,
	TOK_IDENT   = 1001,
	TOK_END     = 1002,

	TOK_INVALID = 10000,
};


static t_value plus_term(struct ParserContext* ctx);
static t_value plus_term_rest(struct ParserContext* ctx, t_value arg);
static t_value mul_term(struct ParserContext* ctx);
static t_value mul_term_rest(struct ParserContext* ctx, t_value arg);
static t_value pow_term(struct ParserContext* ctx);
static t_value pow_term_rest(struct ParserContext* ctx, t_value arg);
static t_value factor(struct ParserContext* ctx);
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// symbol table
// ----------------------------------------------------------------------------

struct Symbol* create_symbol(const char* name, t_value value)
{
	struct Symbol *sym = (struct Symbol*)malloc(sizeof(struct Symbol));
	if(sym)
	{
		my_strncpy(sym->name, name, MAX_IDENT);
		sym->value = value;
		sym->next = 0;
	}

	return sym;
}


struct Symbol* find_symbol(struct ParserContext* ctx, const char* name)
{
	struct Symbol* sym = &ctx->symboltable;

	while(sym)
	{
		if(my_strncmp(sym->name, name, MAX_IDENT) == 0)
			return sym;

		sym = sym->next;
	}

	return sym;
}


struct Symbol* assign_or_insert_symbol(struct ParserContext* ctx, const char* name, t_value value)
{
	struct Symbol* sym = find_symbol(ctx, name);

	if(sym)
	{
		sym->value = value;
	}
	else
	{
		sym = create_symbol(name, value);

		struct Symbol *table = &ctx->symboltable;
		while(1)
		{
			if(table->next == 0)
			{
				table->next = sym;
				break;
			}

			table = table->next;
		}
	}

	return sym;
}


void print_symbols(struct ParserContext* ctx)
{
	const struct Symbol* sym = ctx->symboltable.next;
	char msg[512];
	msg[0] = 0;

	while(sym)
	{
		my_strncat(msg, "\t", sizeof(msg));
		my_strncat(msg, sym->name, sizeof(msg));
		my_strncat(msg, " = ", sizeof(msg));
		char val[64];

#ifdef USE_INTEGER
		int_to_str(sym->value, 10, val);
#else
		real_to_str(sym->value, 10, val, 8);
#endif

		my_strncat(msg, val, sizeof(msg));
		my_strncat(msg, "\n", sizeof(msg));
		sym = sym->next;
	}

	printf("Symbol table:\n%s", msg);
}
// ------------------------------------------------------------------------



// ------------------------------------------------------------------------
// lexer
// ------------------------------------------------------------------------


#ifdef USE_INTEGER

static int match_int(const char* tok)
{
	int len = my_strlen(tok);

	for(int i=0; i<len; ++i)
	{
		if(!my_isdigit(tok[i], 0))
			return 0;
	}
	return 1;
}

#else

static int match_real(const char* tok)
{
	int len = my_strlen(tok);
	int point_seen = 0;

	for(int i=0; i<len; ++i)
	{
		if(my_isdigit(tok[i], 0))
			continue;
		if(tok[i] == '.' && !point_seen)
		{
			point_seen = 1;
			continue;
		}
		else
			return 0;
	}
	return 1;
}

#endif

static int match_ident(const char* tok)
{
	int len = my_strlen(tok);
	if(len == 0)
		return 0;
	if(!my_isalpha(tok[0]))
		return 0;

	for(int i=1; i<len; ++i)
	{
		if(!my_isdigit(tok[i], 0) && !my_isalpha(tok[i]))
			return 0;
	}

	//printf("Match: %s\n", tok);
	return 1;
}


/**
 * find all matching tokens for input string
 */
static int get_matching_token(const char* str, t_value* value)
{
#ifdef USE_INTEGER
	{	// int
		if(match_int(str))
		{
			t_value val = 0;
			val = my_atoi(str, 10);

			*value = val;
			return TOK_VALUE;
		}
	}
#else
	{	// real
		if(match_real(str))
		{
			t_value val = 0.;
			val = my_atof(str, 10);

			*value = val;
			return TOK_VALUE;
		}
	}
#endif

	{	// ident
		if(match_ident(str))
		{
			*value = 0;
			return TOK_IDENT;
		}
	}

	{	// tokens represented by themselves
		if(my_strcmp(str, "+")==0 || my_strcmp(str, "-")==0 || my_strcmp(str, "*")==0 ||
			my_strcmp(str, "/")==0 || my_strcmp(str, "%")==0 || my_strcmp(str, "^")==0 ||
			my_strcmp(str, "(")==0 || my_strcmp(str, ")")==0 || my_strcmp(str, ",")==0 ||
			my_strcmp(str, "=")==0)
		{
			*value = 0;
			return (int)str[0];
		}
	}

	*value = 0;
	return TOK_INVALID;
}


static void set_input(struct ParserContext* ctx, const char* input)
{
	ctx->input = input;
	ctx->input_len = my_strlen(ctx->input);
	ctx->input_idx = 0;
}


static int input_get(struct ParserContext* ctx)
{
	if(ctx->input_idx >= ctx->input_len)
		return EOF;

	return ctx->input[ctx->input_idx++];
}


static int input_peek(struct ParserContext* ctx)
{
	if(ctx->input_idx >= ctx->input_len)
		return EOF;

	return ctx->input[ctx->input_idx];
}


static void input_putback(struct ParserContext* ctx)
{
	if(ctx->input_idx > 0)
		--ctx->input_idx;
}


/**
 * @return token, yylval, yytext
 */
static int lex(struct ParserContext* ctx, t_value* lval, char* text)
{
	char input[MAX_IDENT];
	char longest_input[MAX_IDENT];
	input[0] = 0;
	longest_input[0] = 0;

	int longest_matching_token = TOK_INVALID;
	t_value longest_matching_value = 0;

	while(1)
	{
		int c = input_get(ctx);
		if(c == EOF)
			break;

		// if outside any other match...
		if(longest_matching_token == TOK_INVALID)
		{
			// ...ignore white spaces
			if(c==' ' || c=='\t')
				continue;
			// ...end on new line
			if(c=='\n')
			{
				*lval = 0;
				my_strncpy(text, longest_input, MAX_IDENT);
				return TOK_END;
			}
		}

		strncat_char(input, c, MAX_IDENT);
		t_value matching_val = 0;
		int matching = get_matching_token(input, &matching_val);
		if(matching != TOK_INVALID)
		{
			my_strncpy(longest_input, input, MAX_IDENT);
			longest_matching_token = matching;
			longest_matching_value = matching_val;

			if(input_peek(ctx)==EOF)
				break;
		}
		else
		{
			// no more matches
			input_putback(ctx);
			break;
		}
	}

	// at EOF
	if(longest_matching_token == TOK_INVALID && my_strlen(input) == 0)
	{
		*lval = 0;
		my_strncpy(text, longest_input, MAX_IDENT);
		return TOK_END;
	}

	// nothing matches
	if(longest_matching_token == TOK_INVALID)
	{
		printf("Invalid input in lexer: \"%s\".\n", input);

		*lval = 0;
		my_strncpy(text, longest_input, MAX_IDENT);
		return TOK_INVALID;
	}

	// found match
	if(longest_matching_token != TOK_INVALID)
	{
		*lval = longest_matching_value;
		my_strncpy(text, longest_input, MAX_IDENT);
		return longest_matching_token;
	}

	// should not get here
	*lval = 0;
	my_strncpy(text, longest_input, MAX_IDENT);
	return TOK_INVALID;
}
// ------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// lexer interface
// ----------------------------------------------------------------------------
static void next_lookahead(struct ParserContext* ctx)
{
	ctx->lookahead = lex(ctx, &ctx->lookahead_val, ctx->lookahead_text);
}


static int match(struct ParserContext* ctx, int expected)
{
	if(ctx->lookahead != expected)
	{
		printf("Could not match symbol! Expected: %d, got %d.\n", expected, ctx->lookahead);
		return 0;
	}

	return 1;
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// productions
// ----------------------------------------------------------------------------
/**
 * +,- terms
 * (lowest precedence, 1)
 */
static t_value plus_term(struct ParserContext* ctx)
{
	// plus_term -> mul_term plus_term_rest
	if(ctx->lookahead == '(' || ctx->lookahead == TOK_VALUE || ctx->lookahead == TOK_IDENT)
	{
		t_value term_val = mul_term(ctx);
		t_value expr_rest_val = plus_term_rest(ctx, term_val);

		return expr_rest_val;
	}
	else if(ctx->lookahead == '+')	// unary +
	{
		next_lookahead(ctx);
		t_value term_val = mul_term(ctx);
		t_value expr_rest_val = plus_term_rest(ctx, term_val);

		return expr_rest_val;
	}
	else if(ctx->lookahead == '-')	// unary -
	{
		next_lookahead(ctx);
		t_value term_val = -mul_term(ctx);
		t_value expr_rest_val = plus_term_rest(ctx, term_val);

		return expr_rest_val;
	}

	if(ctx->lookahead == 0 || ctx->lookahead == EOF)
		return 0;

	printf("Invalid lookahead in %s: %d.\n", __func__, ctx->lookahead);
	return 0.;
}


static t_value plus_term_rest(struct ParserContext* ctx, t_value arg)
{
	// plus_term_rest -> '+' mul_term plus_term_rest
	if(ctx->lookahead == '+')
	{
		next_lookahead(ctx);
		t_value term_val = arg + mul_term(ctx);
		t_value expr_rest_val = plus_term_rest(ctx, term_val);

		return expr_rest_val;
	}

	// plus_term_rest -> '-' mul_term plus_term_rest
	else if(ctx->lookahead == '-')
	{
		next_lookahead(ctx);
		t_value term_val = arg - mul_term(ctx);
		t_value expr_rest_val = plus_term_rest(ctx, term_val);

		return expr_rest_val;
	}
	// plus_term_rest -> epsilon
	else if(ctx->lookahead == ')' || ctx->lookahead == TOK_END || ctx->lookahead == ',')
	{
		return arg;
	}

	printf("Invalid lookahead in %s: %d.\n", __func__, ctx->lookahead);
	return 0.;
}


/**
 * *,/,% terms
 * (precedence 2)
 */
static t_value mul_term(struct ParserContext* ctx)
{
	// mul_term -> pow_term mul_term_rest
	if(ctx->lookahead == '(' || ctx->lookahead == TOK_VALUE || ctx->lookahead == TOK_IDENT)
	{
		t_value factor_val = pow_term(ctx);
		t_value term_rest_val = mul_term_rest(ctx, factor_val);

		return term_rest_val;
	}

	printf("Invalid lookahead in %s: %d.\n", __func__, ctx->lookahead);
	return 0.;
}


static t_value mul_term_rest(struct ParserContext* ctx, t_value arg)
{
	// mul_term_rest -> '*' pow_term mul_term_rest
	if(ctx->lookahead == '*')
	{
		next_lookahead(ctx);
		t_value factor_val = arg * pow_term(ctx);
		t_value term_rest_val = mul_term_rest(ctx, factor_val);

		return term_rest_val;
	}

	// mul_term_rest -> '/' pow_term mul_term_rest
	else if(ctx->lookahead == '/')
	{
		next_lookahead(ctx);
		t_value factor_val = arg / pow_term(ctx);
		t_value term_rest_val = mul_term_rest(ctx, factor_val);

		return term_rest_val;
	}

	// mul_term_rest -> '%' pow_term mul_term_rest
	else if(ctx->lookahead == '%')
	{
		next_lookahead(ctx);
		t_value factor_val = fmod(arg, pow_term(ctx));
		t_value term_rest_val = mul_term_rest(ctx, factor_val);

		return term_rest_val;
	}

	// mul_term_rest -> epsilon
	else if(ctx->lookahead == '+' || ctx->lookahead == '-' || ctx->lookahead == ')'
		|| ctx->lookahead == TOK_END || ctx->lookahead == ',')
	{
		return arg;
	}

	printf("Invalid lookahead in %s: %d.\n", __func__, ctx->lookahead);
	return 0.;
}


/**
 * ^ terms
 * (precedence 3)
 */
static t_value pow_term(struct ParserContext* ctx)
{
	// pow_term -> factor pow_term_rest
	if(ctx->lookahead == '(' || ctx->lookahead == TOK_VALUE || ctx->lookahead == TOK_IDENT)
	{
		t_value factor_val = factor(ctx);
		t_value term_rest_val = pow_term_rest(ctx, factor_val);

		return term_rest_val;
	}

	printf("Invalid lookahead in %s: %d.\n", __func__, ctx->lookahead);
	return 0.;
}


static t_value pow_term_rest(struct ParserContext* ctx, t_value arg)
{
	// pow_term_rest -> '^' factor pow_term_rest
	if(ctx->lookahead == '^')
	{
		next_lookahead(ctx);
		t_value factor_val = pow(arg, factor(ctx));
		t_value term_rest_val = pow_term_rest(ctx, factor_val);

		return term_rest_val;
	}

	// pow_term_rest -> epsilon
	else if(ctx->lookahead == '+' || ctx->lookahead == '-' || ctx->lookahead == ')'
		|| ctx->lookahead == TOK_END || ctx->lookahead == ','
		|| ctx->lookahead == '*' || ctx->lookahead == '/' || ctx->lookahead == '%')
	{
		return arg;
	}

	printf("Invalid lookahead in %s: %d.\n", __func__, ctx->lookahead);
	return 0.;
}


/**
 * () terms, real factor or identifier
 * (highest precedence, 4)
 */
static t_value factor(struct ParserContext* ctx)
{
	// factor -> '(' plus_term ')'
	if(ctx->lookahead == '(')
	{
		next_lookahead(ctx);
		t_value expr_val = plus_term(ctx);
		match(ctx, ')');
		next_lookahead(ctx);

		return expr_val;
	}

	// factor -> TOK_VALUE
	else if(ctx->lookahead == TOK_VALUE)
	{
		t_value val = ctx->lookahead_val;
		next_lookahead(ctx);

		return val;
	}

	// factor -> TOK_IDENT
	else if(ctx->lookahead == TOK_IDENT)
	{
		char ident[MAX_IDENT];
		my_strncpy(ident, ctx->lookahead_text, MAX_IDENT);

		next_lookahead(ctx);

		// function call
		// using next ctx->lookahead, grammar still ll(1)?
		if(ctx->lookahead == '(')
		{
			next_lookahead(ctx);

			// 0-argument function
			// factor -> TOK_IDENT '(' ')'
			if(ctx->lookahead == ')')
			{
				next_lookahead(ctx);

				// TODO
				//auto iter = m_mapFuncs0.find(ident);
				//if(iter == m_mapFuncs0.end())
				{
					printf("Unknown function: \"%s\".\n", ident);
					return 0.;
				}

				//return (*iter->second)();
			}

			// function with arguments
			else
			{
				// first argument
				t_value expr_val1 = plus_term(ctx);

				// one-argument-function
				// factor -> TOK_IDENT '(' plus_term ')'
				if(ctx->lookahead == ')')
				{
					next_lookahead(ctx);

					if(my_strncmp(ident, "sqrt", MAX_IDENT)==0)
					{
						return sqrt(expr_val1);
					}
					else if(my_strncmp(ident, "sin", MAX_IDENT)==0)
					{
						return sin(expr_val1);
					}
					else if(my_strncmp(ident, "cos", MAX_IDENT)==0)
					{
						return cos(expr_val1);
					}
					else if(my_strncmp(ident, "tan", MAX_IDENT)==0)
					{
						return tan(expr_val1);
					}
					else if(my_strncmp(ident, "asin", MAX_IDENT)==0)
					{
						return asin(expr_val1);
					}
					else if(my_strncmp(ident, "acos", MAX_IDENT)==0)
					{
						return acos(expr_val1);
					}
					else if(my_strncmp(ident, "atan", MAX_IDENT)==0)
					{
						return atan(expr_val1);
					}
					else if(my_strncmp(ident, "log", MAX_IDENT)==0)
					{
						return log(expr_val1);
					}
					else if(my_strncmp(ident, "log2", MAX_IDENT)==0)
					{
						return log2(expr_val1);
					}
					else if(my_strncmp(ident, "log10", MAX_IDENT)==0)
					{
						return log10(expr_val1);
					}
					else
					{
						printf("Unknown function: \"%s\".\n", ident);
						return 0.;
					}
				}

				// two-argument-function
				// factor -> TOK_IDENT '(' plus_term ',' plus_term ')'
				else if(ctx->lookahead == ',')
				{
					next_lookahead(ctx);
					t_value expr_val2 = plus_term(ctx);
					match(ctx, ')');
					next_lookahead(ctx);

					if(my_strncmp(ident, "atan2", MAX_IDENT)==0)
					{
						return atan2(expr_val1, expr_val2);
					}
					else if(my_strncmp(ident, "pow", MAX_IDENT)==0)
					{
						return pow(expr_val1, expr_val2);
					}
					else
					{
						printf("Unknown function: \"%s\".\n", ident);
						return 0.;
					}
				}
				else
				{
					printf("Invalid function call to \"%s\".\n", ident);
				}
			}
		}

		// assignment
		else if(ctx->lookahead == '=')
		{
			next_lookahead(ctx);
			t_value assign_val = plus_term(ctx);
			assign_or_insert_symbol(ctx, ident, assign_val);
			return assign_val;
		}

		// variable lookup
		else
		{
			const struct Symbol* sym = find_symbol(ctx, ident);
			if(!sym)
			{
				printf("Unknown identifier \"%s\".\n", ident);
				return 0.;
			}

			return sym->value;
		}
	}

	printf("Invalid lookahead in %s: \"%d\".\n", __func__, ctx->lookahead);
	return 0.;
}

// ------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// parser interface
// ----------------------------------------------------------------------------

void init_parser(struct ParserContext* ctx)
{
	ctx->lookahead = TOK_INVALID;
	ctx->lookahead_val = 0;
	ctx->lookahead_text[0] = 0;

	ctx->input_idx = 0;
	ctx->input_len = 0;
	ctx->input = 0;

	my_strncpy(ctx->symboltable.name, "", MAX_IDENT);
	ctx->symboltable.value = 0;

	struct Symbol *sym = create_symbol("pi", M_PI);
	ctx->symboltable.next = sym;
}


void deinit_parser(struct ParserContext* ctx)
{
	struct Symbol *sym = ctx->symboltable.next;

	while(sym)
	{
		struct Symbol *symnext = sym->next;
		free(sym);
		sym = symnext;
	}

	ctx->symboltable.next = 0;
}


t_value parse(struct ParserContext* ctx, const char* str)
{
	set_input(ctx, str);
	next_lookahead(ctx);
	return plus_term(ctx);
}
// ----------------------------------------------------------------------------


/* // test: gcc -Wall -Wextra -o 0 expr_parser.c string.c -lm
int main()
{
	struct ParserContext ctx;
	init_parser(&ctx);
	printf("%g\n", parse(&ctx, "123 + 500*2"));
	deinit_parser(&ctx);

	return 0;
}*/
