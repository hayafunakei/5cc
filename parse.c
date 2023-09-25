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

Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
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
    prog->node = head.next;
    prog->localValues = locals;
    return prog;
}

// stmt･･･ステートメント(1文)
// stmt = expr ";"
Node *stmt() {     
    Node *node = expr();
    expect(";"); // 必ず区切りとなる
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

// primary = "(" expr ")" | ident | num
Node *primary() {
    // 次のトークンが"("なら、"(" expr ")"となるはず
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    // 変数
    Token *tok = consume_ident();
    if (tok) {
        Node *node = new_node(ND_VAR);
        
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
