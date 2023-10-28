#include "5cc.h"

//
// トークナイザー
//

char *filename;
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
// 前提:すべての行が必ず'\n'で終わっていること
// 
// foo.c:10: x = y + + 1;
//                   ^ 式ではありません
void verror_at(char *loc, char *fmt, va_list ap) {
    // locを含む行の頭を取得する
    char *line = loc;
    while (user_input < line && line[-1] != '\n')
        line--;    
    
    char *end = loc;
    while (*end != '\n')
        end++;
    
    // 行数を取得
    int line_num = 1;
    for (char *p = user_input; p < line; p++)
        if (*p == '\n')
            line_num++;
    
    // 当該行を表示
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // エラーメッセージを表示
    int pos = loc - line + indent;
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

// 現在のトークンが与えられた文字列とマッチする場合真を返す
Token *peek(char *s) {
    if (token->kind != TK_RESERVED || strlen(s) != token->len ||
          memcmp(token->str, s, token->len))
        return NULL;
    return token;
}

// 次のトークンが期待している記号(文字列)の時には、
// トークンを返し、次のトークンに読み進める。 
Token *consume(char *s) {
    if  (!peek(s))
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
void expect(char *s) {
    if (!peek(s))    
        error_tok(token, "\"%s\"ではありません", s);
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
        error_tok(token, "識別子ではありません");
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
        static char *kw[] = {"return", "if", "else", "while", "for", "int", 
                             "sizeof", "char", "struct", "typedef"}; 
        for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
            int len = strlen(kw[i]);
            if (startswith(p, kw[i]) && !is_alnum(p[len]))
                return kw[i];
        }

        // 複数文字の演算子
        static char *ops[] = {"==", "!=", "<=", ">=", "->"};

        for (int i = 0; i < sizeof(ops) / sizeof(*ops); i++) 
            if (startswith(p, ops[i]))
                return ops[i];
        
        return NULL;
}

char get_escape_char(char c) {
    switch (c) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case 'e': return 27;
        case '0': return 0;
        default:return c;
    }
}

Token *read_string_literal(Token *cur, char *start) {
    char *p = start + 1;
    char buf[1024];
    int len = 0;

    for (;;) {
        if (len == sizeof(buf))
            error_at(start, "文字列が大きすぎます");
        if (*p == '\0')
            error_at(start, "\"が閉じられていません");
        if (*p == '"')
            break;

        if (*p == '\\') {
            p++;
            buf[len++] = get_escape_char(*p++);
        } else {
            buf[len++] = *p++;
        }
    }

    Token *tok = new_token(TK_STR, cur, start, p - start + 1);
    tok->contents = malloc(len + 1);
    memcpy(tok->contents, buf, len);
    tok->contents[len] = '\0';
    tok->cont_len = len + 1;
    return tok;
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
        
        // コメント行をスキップする
        if (startswith(p, "//")) {
            p += 2;
            while (*p != '\n')
                p++;
            continue;
        }

        // ブロックコメントをスキップする
        if (startswith(p, "/*")) {
            char *q = strstr(p + 2, "*/");
            if(!q)
                error_at(p, "ブロックコメントが閉じられていません");
            p = q + 2;
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
        if (strchr("+-*/()<>;={},&[].", *p)) {
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

        // 文字列リテラル
        if (*p == '"') {
            cur = read_string_literal(cur, p);
            p += cur->len;
            continue;
        }

        // 整数値リテラル
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
