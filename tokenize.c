#include "5cc.h"

//
// トークナイザー
//

char *user_input;
Token *token;

// エラーを報告するための関数
// printfと同じ引数をとる
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラー個所を報告して終了する。
void verror_at(char *loc, char *fmt, va_list ap) {
    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " "); // pos個の空白を入力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラー箇所を報告して終了する。
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

// エラー個所を報告して終了する。
void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (tok)
        verror_at(tok->str, fmt, ap);
    
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

char *str_n_dup(const char *s, size_t n) {
    char *p;
    size_t dn;

    for (dn = 0; dn < n && s[dn] != '\0'; dn++)
        continue;
    p = malloc(dn + 1);
    if (p != NULL) {
        memcpy(p, s, dn);
        p[dn] = '\0';
    }
    return p;
}

// 次のトークンが期待している記号(文字列)の時には、
// トークンを返し、次のトークンに読み進める。 
Token *consume(char *op) {
    if (token->kind != TK_RESERVED ||
          strlen(op) != token->len ||
          memcmp(token->str, op, token->len))
        return NULL;
    Token *t = token;
    token = token->next;
    return t;
}

// 次のトークンが変数のときは、確認したTokenを返して、トークンを１つ読み進める。
// それ以外の場合にはNULLを返す。
Token *consume_ident(){
    if(token->kind != TK_IDENT)
        return NULL;
    Token *tmpTok = token;
    token = token->next;
    return tmpTok;
}

// 次のトークンが期待している記号のときには、トークンを１つ読み進める
//　それ以外の場合にはエラーを報告する。
void expect(char *op) {
    if (token->kind != TK_RESERVED ||
          strlen(op) != token->len ||
          memcmp(token->str, op, token->len))
        error_tok(token, "'%s'ではありません", op);
    token = token->next;
}

// 次のトークンが数値の場合、トークンを１つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
    if (token->kind != TK_NUM)
        error_tok(token, "ここで少なくとも数字となるべきです");
    int val = token->val;
    token = token->next;
    return val;
}

// 次のトークンが識別子であることを確認する。トークンを１つ読み進めてその識別子を返す。
// それ以外の場合はエラーを報告する。
char *expect_ident() {
    if (token->kind != TK_IDENT)
        error_tok(token, "演算子ではありません");
    char *s = str_n_dup(token->str, token->len);
    token = token->next;
    return s;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool startswith(char *p, char *q) {
    return memcmp(p, q, strlen(q)) == 0;
}


bool is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

bool is_alnum(char c) {
    return is_alpha(c) || ('0' <= c && c <= '9');
}

char *starts_with_reserved(char *p) {
        // キーワード
        static char *kw[] = {"return", "if", "else", "while", "for"};

        for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
            int len = strlen(kw[i]);
            if (startswith(p, kw[i]) && !is_alnum(p[len]))
                return kw[i];
        }

        // 複数文字の演算子
        static char *ops[] = {"==", "!=", "<=", ">="};

        for (int i = 0; i < sizeof(ops) / sizeof(*ops); i++) 
            if (startswith(p, ops[i]))
                return ops[i];
        
        return NULL;
}

// 入力文字列pをトークナイズして返す
Token *tokenize() {
    char *p = user_input;
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        // 空白文字をスキップ
        if (isspace(*p)){
            p++;
            continue;
        }

        // キーワード・複数文字の演算子
        char *kw = starts_with_reserved(p);
        if (kw) { 
            int len = strlen(kw);
            cur = new_token(TK_RESERVED, cur, p, len);
            p += len;
            continue;
        }

        // 一文字の記号
        if (strchr("+-*/()<>;={},&", *p)) {
            cur = new_token(TK_RESERVED, cur, p, 1);
            p++;
            continue;
        }

        // 識別子
        if (is_alpha(*p)) {
            char *q = p++;
            while (is_alnum(*p))
                p++;
            cur = new_token(TK_IDENT, cur, q, p - q);
            continue;
        }

        // 整数値　リテラル
        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }
        //配置の矛盾はパース時にチェックする。

        error_at(p, "無効なトークン");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}
