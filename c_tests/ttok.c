#include <stdio.h>
#include <assert.h>

#include <algorithm>
#include <string>
#include <cstring>
#include <sstream>
#include <cctype>
#include <map>
#include <cstdint>
#include <vector>
#include <chrono>

#define _strnicmp strncasecmp

enum Token_Enum {
    Token_VARIABLE, Token_GOSUB, Token_GOTO, Token_PRINT, Token_RETURN, Token_END,                     // statements
    Token_REM, Token_DIM, Token_CONSTANT, Token_OPENPAREN, Token_CLOSEPAREN,
    Token_MULT, Token_DIV, Token_PLUS, Token_MINUS, Token_EQ, Token_NE, Token_LE, Token_GE, Token_LT, Token_GT, Token_AND, Token_OR, Token_XOR,  // operators in order of precedence
    Token_FOR, Token_NEXT, Token_IF, Token_THEN, Token_ELSE, Token_LINENUM, Token_STRING, Token_TO, Token_COMMA,
    Token_COLON, Token_SEMICOLON, Token_EXPRESSION, Token_TIME, Token_ELAP, Token_TRON, Token_TROFF,
    Token_ATOMIC, Token_INC, Token_DEC, Token_NOT, Token_INVALID };

#define Token int

const char * Tokens[] = {
    "VARIABLE", "GOSUB", "GOTO", "PRINT", "RETURN", "END",
    "REM", "DIM", "CONSTANT", "OPENPAREN", "CLOSEPAREN",
    "MULT", "DIV", "PLUS", "MINUS", "EQ", "NE", "LE", "GE", "LT", "GT", "AND", "OR", "XOR",
    "FOR", "NEXT", "IF", "THEN", "ELSE", "LINENUM", "STRING", "TO", "COMMA",
    "COLON", "SEMICOLON", "EXPRESSION", "TIME$", "ELAP$", "TRON", "TROFF",
    "ATOMIC", "INC", "DEC", "NOT", "INVALID" };

#ifndef _countof
        template < typename T, size_t N > size_t _countof( T ( & arr )[ N ] ) { return std::extent< T[ N ] >::value; }
#endif

bool isDigit( char c ) { return c >= '0' && c <= '9'; }
bool isAlpha( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ); }
bool isWhite( char c ) { return ' ' == c || 9 /* tab */ == c; }
bool isToken( char c ) { return isAlpha( c ) || ( '%' == c ); }
bool isOperator( char c ) { return '<' == c || '>' == c || '=' == c; }

const char * pastNum( const char * p )
{
    while ( isDigit( *p ) )
        p++;
    return p;
} //pastNum


const char * TokenStr( Token i )
{
    if ( i < 0 || i > Token_INVALID )
    {
        printf( "token %d is malformed\n", i );
        return Tokens[ _countof( Tokens ) - 1 ];
    }

    return Tokens[ i ];
} //TokenStr

Token readTokenInner( const char * p, int & len )
{
    if ( 0 == *p )
    {
        len = 0;
        return Token_INVALID;
    }

    if ( '(' == *p )
    {
        len = 1;
        return Token_OPENPAREN;
    }

    if ( ')' == *p )
    {
        len = 1;
        return Token_CLOSEPAREN;
    }

    if ( ',' == *p )
    {
        len = 1;
        return Token_COMMA;
    }

    if ( ':' == *p )
    {
        len = 1;
        return Token_COLON;
    }

    if ( ';' == *p )
    {
        len = 1;
        return Token_SEMICOLON;
    }

    if ( '*' == *p )
    {
        len = 1;
        return Token_MULT;
    }

    if ( '/' == *p )
    {
        len = 1;
        return Token_DIV;
    }

    if ( '+' == *p )
    {
        len = 1;
        return Token_PLUS;
    }

    if ( '-' == *p )
    {
        len = 1;
        return Token_MINUS;
    }

    if ( '^' == *p )
    {
        len = 1;
        return Token_XOR;
    }

    if ( isDigit( *p ) )
    {
        len = (int) ( pastNum( p ) - p );
        return Token_CONSTANT;
    }

    if ( isOperator( *p ) )
    {
        if ( isOperator( * ( p + 1 ) ) )
        {
            len = 2;
            char c1 = *p;
            char c2 = * ( p + 1 );

            if ( c1 == '<' && c2 == '=' )
                return Token_LE;
            if ( c1 == '>' && c2 == '=' )
                return Token_GE;
            if ( c1 == '<' && c2 == '>' )
                return Token_NE;

            return Token_INVALID;
        }
        else
        {
            len = 1;

            if ( '<' == *p )
                return Token_LT;
            if ( '=' == *p )
                return Token_EQ;
            if ( '>' == *p )
                return Token_GT;

            return Token_INVALID;
        }
    }

    if ( *p == '"' )
    {
        const char * pend = strchr( p + 1, '"' );

        while ( pend && '"' == * ( pend + 1 ) )
            pend = strchr( pend + 2, '"' );

        if ( pend )
        {
            len = 1 + (int) ( pend - p );
            return Token_STRING;
        }

        return Token_INVALID;
    }

    if ( !_strnicmp( p, "TIME$", 5 ) )
    {
       len = 5;
       return Token_TIME;
    }

    if ( !_strnicmp( p, "ELAP$", 5 ) )
    {
        len = 5;
        return Token_ELAP;
    }

    len = 0;
    while ( ( isToken( * ( p + len ) ) ) && len < 10 )
        len++;

    if ( 1 == len && isAlpha( *p ) )
        return Token_VARIABLE; // in the future, this will be true

    if ( 2 == len )
    {
        if ( !_strnicmp( p, "OR", 2 ) )
            return Token_OR;

        if ( !_strnicmp( p, "IF", 2 ) )
            return Token_IF;

        if ( !_strnicmp( p, "TO", 2 ) )
            return Token_TO;

        if ( isAlpha( *p ) && ( '%' == * ( p + 1 ) ) )
            return Token_VARIABLE;
    }
    else if ( 3 == len )
    {
        if ( !_strnicmp( p, "REM", 3 ) )
            return Token_REM;

        if ( !_strnicmp( p, "DIM", 3 ) )
           return Token_DIM;

        if ( !_strnicmp( p, "AND", 3 ) )
           return Token_AND;

        if ( !_strnicmp( p, "FOR", 3 ) )
           return Token_FOR;

        if ( !_strnicmp( p, "END", 3 ) )
           return Token_END;

        if ( isAlpha( *p ) && isAlpha( * ( p + 1 ) ) && ( '%' == * ( p + 2 ) ) )
           return Token_VARIABLE;
    }
    else if ( 4 == len )
    {
        if ( !_strnicmp( p, "GOTO", 4 ) )
           return Token_GOTO;

        if ( !_strnicmp( p, "NEXT", 4 ) )
           return Token_NEXT;

        if ( !_strnicmp( p, "THEN", 4 ) )
           return Token_THEN;

        if ( !_strnicmp( p, "ELSE", 4 ) )
           return Token_ELSE;

        if ( !_strnicmp( p, "TRON", 4 ) )
           return Token_TRON;
    }
    else if ( 5 == len )
    {
        if ( !_strnicmp( p, "GOSUB", 5 ) )
           return Token_GOSUB;

        if ( !_strnicmp( p, "PRINT", 5 ) )
           return Token_PRINT;

        if ( !_strnicmp( p, "TROFF", 5 ) )
           return Token_TROFF;
    }

    else if ( 6 == len )
    {
        if ( !_strnicmp( p, "RETURN", 5 ) )
           return Token_RETURN;

        if ( !_strnicmp( p, "SYSTEM", 5 ) ) // system is the same as end; both exit execution
           return Token_END;
    }

    return Token_INVALID;
} //readTokenInner

Token readToken( const char * p, int & len )
{
    Token t = readTokenInner( p, len );
    printf( "  read token %s from string '%s', length %d\n", TokenStr( t ), p, len );

    return t;
} //readToken

int main()
{
    int len;
    readToken( "al% = v%", len);
}