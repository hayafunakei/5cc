#include "5cc.h"

//
// パーサー
//

VarList *locals;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
Var *find_var(Token *tok) {
    for (VarList *vl = locals; vl; vl = vl->next) {
        Var *var = vl->var;
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
            return var;
    }
    return NULL;
}

Node *new_node(NodeKind kind, Token *tok){
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;   
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

Node *new_node_num(int val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

Node *new_var(Var *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

Var *push_var(char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    
    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

Function *function();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// program = function*
Function *program() {
    Function head;
    head.next = NULL;
    Function *cur = &head;

    while (!at_eof()){
        cur->next = function();
        cur = cur->next;
    }
    return head.next;
}

VarList *read_func_params() {
    if (consume(")"))
        return NULL;
    
    // 一つ以上引数がある
    VarList *head = calloc(1, sizeof(VarList));
    head->var = push_var(expect_ident());
    VarList *cur = head;

    while (!consume(")")) {
        expect(",");
        cur->next = calloc(1, sizeof(VarList));
        cur->next->var = push_var(expect_ident());
        cur = cur->next;
    }

    return head;
}


// function = ident "(" params? ")" "{" stmt* "}"
// params   = ident ("," ident)*
Function *function() {
    locals = NULL; // ここから一つのかたまり
    Function *fn = calloc(1, sizeof(Function));
    fn->name = expect_ident();

    expect("(");
    fn->params = read_func_params();
    expect("{");

    Node head;
    head.next = NULL;
    Node *cur = &head;

    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }

    fn->node = head.next;
    fn->locals = locals;
    return fn;
}

Node *read_expr_stmt() {
    Token *tok = token;
    return new_unary(ND_EXPR_STMT, expr(), tok);
}

// stmt･･･ステートメント(1文)
// stmt = "return" expr ";" 
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "while" "(" expr ")" stmt
//        | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//        | "{" stmt* "}"
//        | expr ";"  
Node *stmt() {     
    Token *tok;
    if (consume("return")) {
        Node *node = new_unary(ND_RETURN, expr(), tok); // 必ずexpr以下となる。
        expect(";"); 
        return node;
    }
    
    if (consume("while")) {
        Node *node = new_node(ND_WHILE, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if (consume("if")) {
        Node *node = new_node(ND_IF, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else"))
            node->els = stmt();
        return node;
    }

    if (consume("for")) {
        Node *node = new_node(ND_FOR, tok);
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

    if (tok = consume("{")) {
        Node head;
        head.next = NULL;
        Node *cur = &head;

        while (!consume("}")) {
            cur->next = stmt(); // ブロック内の新しい一文
            cur = cur->next;
        }

        Node *node = new_node(ND_BLOCK, tok);
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
    Token *tok;
    if (tok = consume("="))
        node = new_binary(ND_ASSIGN, node, assign(), tok);
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
    Node *node = relational();
    Token *tok;

    for(;;) {
        if (tok = consume("=="))
            node = new_binary(ND_EQ, node, relational(), tok);
        else if (tok = consume("!="))
            node = new_binary(ND_NE, node, relational(), tok);
        else
            return node;
    } 
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
    Node *node = add();
    Token *tok;

    for (;;) {
        if (tok = consume("<"))
            node = new_binary(ND_LT, node, add(), tok);
        else if(tok = consume("<="))
            node = new_binary(ND_LE, node, add(), tok);
        else if(tok = consume(">"))
            node = new_binary(ND_LT, add(), node, tok);
        else if(tok = consume(">="))
            node = new_binary(ND_LE, add(), node, tok);
        else
            return node;
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
    Node *node = mul();
    Token *tok;

    for(;;) {
        if (tok = consume("+"))
            node = new_binary(ND_ADD, node, mul(), tok);
        else if (tok = consume("-"))
            node = new_binary(ND_SUB, node, mul(), tok);
        else
            return node;
    }
}

// mul = unary ( "*" unary | "/" unary)*
Node *mul() {
    Node *node = unary();
    Token *tok;

    for (;;) {
        if (tok = consume("*"))
            node = new_binary(ND_MUL, node, unary(), tok);
        else if (tok = consume("/"))
            node = new_binary(ND_DIV, node, unary(), tok);
        else
            return node;
    }
}

// unary = ("+" | "-")? unary | primary 
Node *unary() {
    Token *tok;
    if (consume("+"))
        return unary();
    if (tok = consume("-"))
        // 0と右ノードを引き算してマイナスにする。
        return new_binary(ND_SUB, new_node_num(0, tok), unary(), tok);
    return primary();
}

// func-args = "(" (assign ("," assign)*)? ")"
Node *func_args() {
    if (consume(")"))
        return NULL;
    
    Node *head = assign();
    Node *cur = head;
    while (consume(",")) {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");
    return head;
}

// primary = "(" expr ")" | ident func-args? | num
Node *primary() {
    // 次のトークンが"("なら、"(" expr ")"となるはず
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    Token *tok;
    // 変数または関数　識別子
    if (tok = consume_ident()) {
        // 先に関数かチェック
        if (consume("(")) {
            Node *node = new_node(ND_FUNCALL, tok);
            node->funcname = str_n_dup(tok->str, tok->len);
            node->args = func_args(); // 末尾はfunc_argsでチェック
            return node;
        }
        
        Var *var = find_var(tok);
        if (!var)
            var = push_var(str_n_dup(tok->str, tok->len));
       
        return new_var(var, tok);
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number(), tok);
}
