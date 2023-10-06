#include "5cc.h"

//
// パーサー
//

Var *locals;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
Var *find_var(Token *tok) {
    for (Var *var = locals; var; var = var->next)
        if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
            return var;
    return NULL;
}

Node *new_node(NodeKind kind){
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;   
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_unary(NodeKind kind, Node *expr) {
    Node *node = new_node(kind);
    node->lhs = expr;
    return node;
}

Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

char *str_n_dup(const char *s, size_t n) {
    char *p;
    size_t n1;

    for (n1 = 0; n1 < n && s[n1] != '\0'; n1++)
        continue;
    p = calloc(n1 + 1, sizeof(char));
    if (p != NULL) {
        memcpy(p, s, n1);
        p[n1] = '\0';
    }
    return p;
}

Program *program();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// program = stmt*
Program *program() {
    locals = NULL;

    Node head;
    head.next = NULL;
    Node *cur = &head;

    while (!at_eof()){
        cur->next = stmt();
        cur = cur->next;
    }
    
    Program *prog = calloc(1, sizeof(program));
    prog->node = head.next; // 一文目のポインタ
    prog->localValues = locals;
    return prog;
}

Node *read_expr_stmt() {
    return new_unary(ND_EXPR_STMT, expr());
}

// stmt･･･ステートメント(1文)
// stmt = "return" expr ";" 
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "while" "(" expr ")" stmt
//        | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//        | "{" stmt* "}"
//        | expr ";"  
Node *stmt() {     
    if (consume("return")) {
        Node *node = new_unary(ND_RETURN, expr()); // 必ずexpr以下となる。
        expect(";"); 
        return node;
    }
    
    if (consume("while")) {
        Node *node = new_node(ND_WHILE);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if (consume("if")) {
        Node *node = new_node(ND_IF);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else"))
            node->els = stmt();
        return node;
    }

    if (consume("for")) {
        Node *node = new_node(ND_FOR);
        expect("(");
        if (!consume(";")) {
            node->init = read_expr_stmt();
            expect(";");
        } else { /* ";"の場合空文として次に移動する */ }
        if (!consume(";")) {
            node->cond = expr(); // 評価結果を残して判定に使うためexpr()_(read_ex~()ではなく)
            expect(";");
        } else { /*空文*/ }
        if (!consume(")")) {
            node->inc = read_expr_stmt();
            expect(")");
        } else { /* 空文 */ }
        node->then = stmt();
        return node;
    }

    if (consume("{")) {
        Node head;
        head.next = NULL;
        Node *cur = &head;

        while (!consume("}")) {
            cur->next = stmt(); // ブロック内の新しい一文
            cur = cur->next;
        }

        Node *node = new_node(ND_BLOCK);
        node->body = head.next;
        return node;
    }
    
    // stmt ";" 予約語が含まれない文
    Node *node = read_expr_stmt(); // 根元のノードをND_EXPR_STMTとする。評価結果は破棄される。
    expect(";");
    return node;
}

// expr = assign
Node *expr() {
    return assign();
}

// assign = equality ("=" assign)?
Node*assign() {
    Node *node = equality();
    if (consume("="))
        node = new_binary(ND_ASSIGN, node, assign());
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
    Node *node = relational();

    for(;;) {
        if (consume("=="))
            node = new_binary(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_binary(ND_NE, node, relational());
        else
            return node;
    } 
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
    Node *node = add();

    for (;;) {
        if (consume("<"))
            node = new_binary(ND_LT, node, add());
        else if(consume("<="))
            node = new_binary(ND_LE, node, add());
        else if(consume(">"))
            node = new_binary(ND_LT, add(), node);
        else if(consume(">="))
            node = new_binary(ND_LE, add(), node);
        else
            return node;
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
    Node *node = mul();

    for(;;) {
        if (consume("+"))
            node = new_binary(ND_ADD, node, mul());
        else if (consume("-"))
            node = new_binary(ND_SUB, node, mul());
        else
            return node;
    }
}

// mul = unary ( "*" unary | "/" unary)*
Node *mul() {
    Node *node = unary();

    for (;;) {
        if (consume("*"))
            node = new_binary(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_binary(ND_DIV, node, unary());
        else
            return node;
    }
}

// unary = ("+" | "-")? unary | primary 
Node *unary() {
    if (consume("+"))
        return unary();
    if (consume("-"))
        // 0と右ノードを引き算してマイナスにする。
        return new_binary(ND_SUB, new_node_num(0), unary());
    return primary();
}

// primary = "(" expr ")" | ident args? | num
//  args = "(" ")"
Node *primary() {
    // 次のトークンが"("なら、"(" expr ")"となるはず
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    // 変数または関数　識別子
    Token *tok = consume_ident();
    if (tok) {
        // 先に関数かチェック
        if (consume("(")) {
            expect(")");
            Node *node = new_node(ND_FUNCALL);
            node->funcname = str_n_dup(tok->str, tok->len);
            return node;
        }
        Node *node = new_node(ND_VAR);
        
        // todo:push_var() new_var()を作る
        Var *lvar = find_var(tok);
        if (lvar) {
            node->var = lvar;
        } else {
            lvar = calloc(1, sizeof(Var));
            lvar->next = locals;
            lvar->name = tok->str;
            lvar->len = tok->len;
            node->var = lvar;
            locals = lvar;
        }
       
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}
