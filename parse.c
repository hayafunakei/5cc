#include "5cc.h"

//
// パーサー
//

VarList *locals;
VarList *globals;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
Var *find_var(Token *tok) {
    for (VarList *vl = locals; vl; vl = vl->next) {
        Var *var = vl->var;
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
            return var;
    }

    for( VarList *vl = globals; vl; vl = vl->next) {
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

Var *push_var(char *name, Type *ty, bool is_local) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;
    
    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    
    if (is_local) {
        vl->next = locals;
        locals = vl;
    } else {
        vl->next = globals;
        globals = vl;
    }

    return var;
}

char *new_label() {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return str_n_dup(buf, 20);
}

Program *program();
Function *function();
Type *basetype();
void global_var();
Node *declaration();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *postfix();
Node *primary();

bool is_function() {
    Token *tok = token;
    basetype();
    bool isfunc = consume_ident() && consume("(");
    token = tok;
    return isfunc;
}

// program = function*
Program *program() {
    Function head;
    head.next = NULL;
    Function *cur = &head;
    globals = NULL;

    while (!at_eof()){
        if (is_function()) {
            cur->next = function();
            cur = cur->next;
        } else {
            global_var();
        }
    }
    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns= head.next;
    return prog;
}

// basetype = ("char" | "int") "*"*
Type *basetype() {
    Type *ty;
     if (consume("char")) {
        ty = char_type();
     } else {
        expect("int");
        ty = int_type();
     }
     
    while (consume("*"))
        ty = pointer_to(ty);
    return ty;
}

Type *read_type_suffix(Type *base) {
    if (!consume("["))
        return base;
    int sz = expect_number();
    expect("]");
    base = read_type_suffix(base);
    return array_of(base, sz);
}

VarList *read_func_single_param() {
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = push_var(name, ty, true);
    return vl;
}

VarList *read_func_params() {
    if (consume(")"))
        return NULL;
    
    // 一つ以上引数がある
    VarList *head = read_func_single_param(); 
    VarList *cur = head;

    while (!consume(")")) {
        expect(",");
        cur->next = read_func_single_param();
        cur = cur->next;
    }

    return head;
}

// function = basetype ident "(" params? ")" "{" stmt* "}"
// params   = param ("," param)*
// param    = basetype ident
Function *function() {
    locals = NULL; // ここから一つのかたまり

    Function *fn = calloc(1, sizeof(Function));
    basetype();
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

// global-var = basetype ident ("[" num "]")* ";"
void global_var() {
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    expect(";");
    push_var(name, ty, false);
}

// declaration = basetype ident ("[" num "]")* ("=" expr) ";"
Node *declaration() {
    Token *tok = token;
    Type *ty = basetype();
    char *name = expect_ident();
    
    // 配列定義[]
    ty = read_type_suffix(ty); // 配列ならbaseの配列タイプになる。
    Var *var = push_var(name, ty, true);

    if (consume(";"))
        return new_node(ND_NULL, tok);
    
    expect("=");
    Node *lhs = new_var(var, tok);
    Node *rhs = expr();
    expect(";");
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
    return new_unary(ND_EXPR_STMT, node, tok);
}

bool is_typename() {
    return peek("char") || peek("int");
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
//        | declaration
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

    if (is_typename())
        return declaration();
    
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

// unary = ("+" | "-" | "&" | "*")? unary 
//       | postfix 
Node *unary() {
    Token *tok;
    if (consume("+")) // +(正の値) そのまま読み進める
        return unary();
    if (tok = consume("-"))
        // 0と右ノードを引き算してマイナスにする。
        return new_binary(ND_SUB, new_node_num(0, tok), unary(), tok);
    if (tok = consume("&"))
        return new_unary(ND_ADDR, unary(), tok);
    if (tok = consume("*"))
        return new_unary(ND_DEREF, unary(), tok);
    return postfix();
}

// postfix = primary ("[" expr "]")*
Node *postfix() {
    Node *node = primary();
    Token *tok;

    while (tok = consume("[")) {
        // x[y] は *(x+y) の意味
        Node *exp = new_binary(ND_ADD, node, expr(), tok);
        expect("]");
        node = new_unary(ND_DEREF, exp, tok);
    }
    return node;
}

// stmt-expr = "( "{" stmt stmt* "}" ")"
// 
// Statement expression is a GNU C extension.
// https://gcc.gnu.org/onlinedocs/gcc-4.8.3/gcc/Statement-Exprs.html#Statement-Exprs
Node *stmt_expr(Token *tok) {
    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    expect(")");

    if (cur->kind != ND_EXPR_STMT)
        error_tok(cur->tok, "voidを返すstmt_exprはサポートされていません。");
    *cur = *cur->lhs;
    return node;
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

// primary =   "(" "{" stmt-expr-tail
//            | "(" expr ")" 
//            | ident func-args? 
//            | "sieof" unary
//            | str
//            | num
Node *primary() {
    Token *tok;

    // 次のトークンが"("なら、"(" expr ")"となるはず
    if (tok = consume("(")) {
        if (consume("{"))
            return stmt_expr(tok);
        Node *node = expr();
        expect(")");
        return node;
    }

    if (tok = consume("sizeof"))
        return new_unary(ND_SIZEOF, unary(), tok);
    
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
            error_tok(tok, "未定義の変数");
       
        return new_var(var, tok);
    }

    tok = token;
    if (tok->kind == TK_STR) {
        token = token->next;

        Type *ty = array_of(char_type(), tok->cont_len);
        Var *var = push_var(new_label(), ty, false);
        var->contents = tok->contents;
        var->cont_len = tok->cont_len;
        return new_var(var, tok);
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number(), tok);
}
