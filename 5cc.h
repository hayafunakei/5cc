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
void error_tok(Token *tok, char *fmt, ...);
Token *consume(char *op);
Token *consume_ident();
void expect(char *op);
int expect_number();
char *expect_ident();
bool at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
Token *tokenize();
char *str_n_dup(const char *s, size_t n);

// 入力プログラム
extern char *user_input;
// 現在着目しているトークン
extern Token *token;

//
// parse.c
//

// ローカル変数の型
typedef struct Var Var;
struct Var {
    char *name; // 変数の名前
    int offset; // RBPからのオフセット
};

typedef struct VarList VarList;
struct VarList {
    VarList *next;
    Var *var;
};

// 抽象構文木のノードの種類
typedef enum {
    ND_ADD,       // +
    ND_SUB,       // -
    ND_MUL,       // *
    ND_DIV,       // / 除算
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ADDR,      // unary &
    ND_DEREF,     // unary *
    ND_ASSIGN,    // = assign:代入する
    ND_RETURN,    // "return"
    ND_IF,        // "if"
    ND_WHILE,     // "while"
    ND_FOR,       // "for"
    ND_BLOCK,     // { ... }
    ND_FUNCALL,   // 関数call
    ND_EXPR_STMT, // 式文
    ND_VAR,       // 変数
    ND_NUM        // 整数
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
    NodeKind kind; // ノードの型
    Node *next;    // 次の行のスタートNode
    Token *tok;     // 代表となるトークン

    Node *lhs;     // 左辺
    Node *rhs;     // 右辺
    
    // "if", "while" または "for" ステートメント
    Node *cond;    // condition:評価式
    Node *then;  
    Node *els;
    Node *init;    // for初期化
    Node *inc;     // forインクリメント条件

    // Block
    Node *body;

    // 関数call
    char *funcname;
    Node *args;
    
    Var *var;      // kindがND_VARの場合のみ使う 当該変数への参照
    int val;       // KindがND_NUMの場合のみ使う
};

// 関数１単位 
typedef struct Function Function;
struct Function {
    Function *next;
    char *name;
    VarList *params;

    Node *node; // 一文目のNode
    VarList *locals;     
    int stack_size;
}; 

Function *program();

//
// codegen.c
//

void codegen(Function *prog);