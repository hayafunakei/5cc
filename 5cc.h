#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;
typedef struct Member Member;

//
// tokenize.c
//

//トークンの種類
typedef enum {
    TK_RESERVED, // 記号
    TK_IDENT,    // 識別子
    TK_NUM,      // 整数トークン
    TK_STR,      // 文字列リテラル
    TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

// トークン型
typedef struct Token Token;
struct Token {
    TokenKind kind; // トークンの型
    Token *next;    // 次の入力トークン
    long val;        // kindがTK_NUMの場合、その数値
    char *str;      // トークン文字列
    int len;        // トークンの長さ

    char *contents; // 末尾の\nを含む文字列リテラルの内容
    char cont_len;  // 文字リテラルの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *peek(char *s);
Token *consume(char *op);
Token *consume_ident();
void expect(char *op);
long expect_number();
char *expect_ident();
bool at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
Token *tokenize();
char *str_n_dup(const char *s, size_t n);

// 入力プログラム
extern char *filename;
extern char *user_input;
// 現在着目しているトークン
extern Token *token;

//
// parse.c
//

// 変数の型
typedef struct Var Var;
struct Var {
    char *name;    // 変数の名前
    Type *ty;      // 型
    Token *tok;    // エラーメッセージで使用
    bool is_local; // ローカルかグローバル
    
    // ローカル変数
    int offset;   // RBPからのオフセット

    //  グローバル変数
    char *contents;
    int cont_len;
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
    ND_BITAND,    // &
    ND_BITOR,     // |
    ND_BITXOR,    // ^
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ADDR,      // unary &
    ND_DEREF,     // unary *
    ND_NOT,       // !
    ND_BITNOT,    // ~
    ND_LOGAND,    // &&
    ND_LOGOR,     // ||
    ND_ASSIGN,    // = assign:代入する
    ND_PRE_INC,   // pre ++
    ND_PRE_DEC,   // pre --
    ND_POST_INC,  // post ++
    ND_POST_DEC,  // post --
    ND_A_ADD,     // +=
    ND_A_SUB,     // -=
    ND_A_MUL,     // *=
    ND_A_DIV,     // /=
    ND_COMMA,     // , (コンマ演算子)
    ND_MEMBER,    // . (構造体メンバーアクセス)
    ND_RETURN,    // "return"
    ND_IF,        // "if"
    ND_WHILE,     // "while"
    ND_FOR,       // "for"
    ND_SWITCH,    // "switch"
    ND_CASE,      // "case"
    ND_SIZEOF,    // "sizeof"
    ND_BLOCK,     // { ... }
    ND_BREAK,     // "break"
    ND_CONTINUE,  // "continue"
    ND_GOTO,      // "goto"
    ND_LABEL,     // ラベル付きの文
    ND_FUNCALL,   // 関数call
    ND_EXPR_STMT, // 式文
    ND_STMT_EXPR, // 式文 GNU拡張 ({  })
    ND_VAR,       // 変数
    ND_NUM,       // 整数
    ND_CAST,      // 型キャスト
    ND_NULL,      // 空の文
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
    NodeKind kind; // ノードの型
    Node *next;    // 次の行のスタートNode
    Type *ty;      // 型  例:int型 または int型領域のポインタ
    Token *tok;    // 代表となるトークン

    Node *lhs;     // 左辺
    Node *rhs;     // 右辺
    
    // "if", "while" または "for" ステートメント
    Node *cond;    // condition:評価式
    Node *then;  
    Node *els;
    Node *init;    // for初期化
    Node *inc;     // forインクリメント条件

    // Block or statement_expression
    Node *body;

    // 構造体メンバーアクセス
    char *member_name;
    Member *member;

    // 関数call
    char *funcname;
    Node *args;

    // goto か ラベル付きの文で使用
    char *label_name;
    
    // Switch-cases
    Node *case_next;
    Node *default_case;
    int case_label;
    int case_end_label;

    Var *var;
    long val;
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

typedef struct {
    VarList *globals;
    Function *fns;
} Program;

Program *program();

//
// type.c
//

typedef enum { 
    TY_VOID,
    TY_BOOL,
    TY_CHAR,
    TY_SHORT,
    TY_INT, 
    TY_PTR, 
    TY_LONG,
    TY_ENUM,
    TY_ARRAY,
    TY_STRUCT, 
    TY_FUNC,
} TypeKind;

// 型の性質を表す
struct Type {
    TypeKind kind;      // 変数の型
    bool is_typedef;    // typedef
    bool is_static;     // static
    bool is_incomplete; // 不完全型
    int align;          // アライメント
    Type *base;         // ポインタ
    int array_size;     // 配列
    Member *members;    // struct構造体
    Type *return_ty;    // function
};

// 構造体メンバー
struct Member {
    Member *next;
    Type *ty;
    Token *tok; // エラーメッセージで使用
    char *name;
    int offset;
};

int align_to();
Type *void_type();
Type *bool_type();
Type *char_type();
Type *short_type();
Type *int_type();
Type *long_type();
Type *enum_type();
Type *struct_type();
Type *func_type(Type *return_ty);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
int size_of(Type *ty, Token *tok);

void add_type(Program *prog);

//
// codegen.c
//

void codegen(Program *prog);