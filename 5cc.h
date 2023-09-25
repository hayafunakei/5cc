#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// tokenize.c
//

//トークンの種類
typedef enum {
    TK_RESERVED, // 記号
    TK_IDENT,    // 識別子
    TK_NUM,      // 整数トークン
    TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

// トークン型
typedef struct Token Token;
struct Token {
    TokenKind kind; // トークンの型
    Token *next;    // 次の入力トークン
    int val;        // kindがTK_NUMの場合、その数値
    char *str;      // トークン文字列
    int len;        // トークンの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
bool consume(char *op);
Token *consume_ident();
void expect(char *op);
int expect_number();
bool at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
Token *tokenize();

// 入力プログラム
extern char *user_input;
// 現在着目しているトークン
extern Token *token;

//
// parse.c
//

// 抽象構文木のノードの種類
typedef enum {
    ND_ADD,    // +
    ND_SUB,    // -
    ND_MUL,    // *
    ND_DIV,    // / 除算
    ND_EQ,     // ==
    ND_NE,     // !=
    ND_LT,     // <
    ND_LE,     // <=
    ND_ASSIGN, // = assign:代入する
    ND_LVAR,   // ローカル変数
    ND_NUM     // 整数
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
    NodeKind kind; // ノードの型
    Node *next;     // 次の行
    Node *lhs;     // 左辺
    Node *rhs;     // 右辺
    int val;       //  KindがND_NUMの場合のみ使う
    int offset;    // kindがND_LVARの場合のみ使う ベースポインタからのオフセット値
};

// 各行の先頭のノード
extern Node *code[100];

void program();

//
// codegen.c
//

void codegen();