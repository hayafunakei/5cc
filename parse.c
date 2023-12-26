#include "5cc.h"

//
// パーサー
//

// ローカル変数・グローバル変数・typedefのスコープ
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
    Var *var;
    Type *type_def;
};

// 構造体タグスコープ
typedef struct TagScope TagScope;
struct TagScope {
    TagScope *next;
    char *name;
    Type *ty;
};

VarList *locals;
VarList *globals;

VarScope *var_scope;
TagScope *tag_scope;

// 変数・typedefを名前で検索する。
VarScope *find_var(Token *tok) {
    for (VarScope *sc = var_scope; sc; sc = sc->next) {
        if (strlen(sc->name) == tok-> len && !memcmp(tok->str, sc->name, tok->len))
            return sc;
    }
    return NULL;
}

TagScope *find_tag(Token *tok) {
    for (TagScope *sc = tag_scope; sc; sc = sc->next)
        if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len))
            return sc;
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

Node *new_num(long val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

Node *new_var(Var *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

VarScope *push_scope(char *name) {
    VarScope *sc = calloc(1, sizeof(VarScope));
    sc->name = name;
    sc->next = var_scope;
    var_scope = sc;
    return sc;
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
    } else if (ty->kind != TY_FUNC) {
        vl->next = globals;
        globals = vl;
    }

    push_scope(name)->var = var;
    return var;
}

Type *find_typedef(Token *tok) {
    if (tok->kind == TK_IDENT) {
        VarScope *sc = find_var(token);
        if (sc)
            return sc->type_def;
    }
    return NULL;
}

char *new_label() {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return str_n_dup(buf, 20);
}

Program *program();
Function *function();
Type *type_specifier();
Type *declarator(Type *ty, char **name);
Type *abstract_declarator(Type *ty);
Type *type_suffix(Type *ty);
Type *type_name();
Type *struct_decl();
Member *struct_member();
void global_var();
Node *declaration();
bool is_typename();
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
    
    Type *ty = type_specifier();
    char *name = NULL;
    declarator(ty, &name);
    bool isfunc = name && consume("(");

    token = tok;
    return isfunc;
}

// program = (global-var | function)*
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

// type-specifier = builtin-type | struct-decl | typedef-name
// builtin-type = "void"
//              | "_Bool"
//              | "char"
//              | "short" | "short" "int" | "int" "short"
//              | "int"
//              | "long" | "long" "int" | "int" "long"   
// dypedefはtype-specifier(型指定子)のどこにでも出現することに注意
Type *type_specifier() {
    if (!is_typename(token))
        error_tok(token, "typename expected");

    Type *ty = NULL;

    enum {
        VOID = 1 << 1,
        BOOL = 1 << 3,
        CHAR = 1 << 5,
        SHORT = 1 << 7,
        INT = 1 << 9,
        LONG = 1 << 11,
    };

    int base_type = 0;
    Type *user_type = NULL;

    bool is_typedef = false;

    for (;;) {
        // トークンを一つずつ読む
        Token *tok = token;
        if (consume("typedef")) {
            is_typedef = true;
        } else if (consume("void")) {
            is_typedef += VOID;
        } else if (consume("_Bool")) {
            base_type += BOOL;
        } else if (consume("char")) {
            base_type += CHAR;
        } else if (consume("short")) {
            base_type += SHORT;
        } else if (consume("int")) {
            base_type += INT;
        } else if (consume("long")) {
            base_type += LONG;
        } else if (peek("struct")) {
            if (base_type || user_type)
                break;
            user_type = struct_decl();
        } else {
            if (base_type || user_type)
                break;
            Type *ty = find_typedef(token);
            if (!ty)
                break;
            // typedef定義済み
            token = token->next;
            user_type = ty;
        }

        switch (base_type) {
        case VOID:
            ty = void_type();
            break;
        case BOOL:
            ty = bool_type();
            break;
        case CHAR:
            ty = char_type();
            break;
        case SHORT:
        case SHORT + INT:
            ty = short_type();
            break;
        case INT:
            ty = int_type();
            break;
        case LONG:
        case LONG + INT:
            ty = long_type();
            break;
        case 0:
            // 指定子がない場合はintになる。
            // 例えば"typedef x"とすると、「xをintの別名として定義」を意味する
            ty = user_type ? user_type : int_type();
            break;
        default:
            error_tok(tok, "無効な型");
        }
    }

    ty->is_typedef = is_typedef;
    return ty;
}

// https://www.sigbus.info/compilerbook#type
//                      ネストしている型
// declarator = "*" ( "(" declarator ")" | ident) type-suffix
Type *declarator(Type *ty, char **name) {
    while (consume("*"))
        ty = pointer_to(ty);

    if (consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = declarator(placeholder, name);
        expect(")");
        *placeholder = *type_suffix(ty);
        return new_ty;
    }

    *name = expect_ident();
    return type_suffix(ty);
}

// abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
Type *abstract_declarator(Type *ty) {
    while (consume("*"))
        ty = pointer_to(ty);
    
    if (consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = abstract_declarator(placeholder);
        expect(")");
        *placeholder = *type_suffix(ty);
        return new_ty;
    }
    return type_suffix(ty);
}

// type-suffix = ("[" num "]" type-suffix)?
Type *type_suffix(Type *ty) {
    if (!consume("["))
        return ty;
    int sz = expect_number();
    expect("]");
    ty = type_suffix(ty);
    return array_of(ty, sz);
}

// type-name = type-specifier abstract-declarator type-suffix
Type *type_name() {
    Type *ty = type_specifier();
    ty = abstract_declarator(ty);
    return type_suffix(ty);
}

void push_tag_scope(Token *tok, Type *ty) {
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = str_n_dup(tok->str, tok->len);
    sc->ty = ty;
    tag_scope = sc;
}

// struct-decl = "struct" ident
//             | "struct" "{" struct-member "}"
Type *struct_decl() {
    // タグがすでにある場合は読み込み
    expect("struct");
    Token *tag = consume_ident();
    if (tag && !peek("{")) {
        TagScope *sc = find_tag(tag);
        if (!sc)
            error_tok(tag, "不明な構造体タイプ");
        return sc->ty;
    }

    expect("{");

    // 構造体メンバー読み込み
    Member head;
    head.next = NULL;
    Member *cur = &head;
    
    while (!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
    ty->members = head.next;

    // 構造体内のオフセットをメンバに割り当てる
    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next) {
        offset = align_to(offset, mem->ty->align); // offset位置調整
        mem->offset = offset;
        offset += size_of(mem->ty);

        if (ty->align < mem->ty->align)
            ty->align = mem->ty->align;
    }
    
    // tag名があれば登録する
    if (tag)
        push_tag_scope(tag, ty);
    return ty;
}

// struct-member = type-specifier declarator type-suffix ";"
Member *struct_member() {
    Type *ty = type_specifier();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name = name;
    mem->ty = ty;
    return mem;
}

VarList *read_func_single_param() {
    Type *ty = type_specifier();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

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

// function = type-specifier declarator  "(" params? ")" "{" stmt* "}"
// params   = param ("," param)*
// param    = type-specifier declarator type-suffix 
Function *function() {
    locals = NULL; // ここから一つのかたまり
    
    Type *ty = type_specifier();
    char *name = NULL;
    ty = declarator(ty, &name);

    // スコープに関数型を追加する
    push_var(name, func_type(ty), false);

    // 関数オブジェクトの用意
    Function *fn = calloc(1, sizeof(Function));
    fn->name = name;

    expect("(");
    fn->params = read_func_params();
    expect("{");

    // 関数の内容を読む
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

// global-var = type-specifier declarator type-suffix ";"
void global_var() {
    Type *ty = type_specifier();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");
    push_var(name, ty, false);
}

// declaration…宣言
// declaration = type-specifier declarator type-suffix  ("=" expr)? ";"
//             | type-specifier ";"
Node *declaration() {
    Token *tok = token;
    Type *ty = type_specifier();

    if (consume(";"))
        return new_node(ND_NULL, tok);
    
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    if (ty->is_typedef) {
        expect(";");
        ty->is_typedef = false;
        push_scope(name)->type_def = ty;
        return new_node(ND_NULL, tok);
    }

    if (ty->kind == TY_VOID)
        error_tok(tok, "void型で宣言された変数");

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
    return peek("void") || peek("_Bool") || peek("char") || peek("short") || 
           peek("int") || peek("long") || peek("struct") || peek("typedef") ||
           find_typedef(token);
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
        
        VarScope *sc1 = var_scope;
        TagScope *sc2 = tag_scope;
        while (!consume("}")) {
            cur->next = stmt(); // ブロック内の新しい一文
            cur = cur->next;
        }
        var_scope = sc1; // ブロックスコープの外側
        tag_scope = sc2;

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
        return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
    if (tok = consume("&"))
        return new_unary(ND_ADDR, unary(), tok);
    if (tok = consume("*"))
        return new_unary(ND_DEREF, unary(), tok);
    return postfix();
}

// postfix = primary ("[" expr "]" "." ident | "->" ident)*
Node *postfix() {
    Node *node = primary();
    Token *tok;
    
    for (;;) {
        if (tok = consume("[")) {
            // x[y] は *(x+y) の意味
            Node *exp = new_binary(ND_ADD, node, expr(), tok);
            expect("]");
            node = new_unary(ND_DEREF, exp, tok);
            continue;
        }

        if (tok = consume(".")) {
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident();
            continue;
        }

        if (tok = consume("->")) {
            // x->y は　(*x).yの意味
            node = new_unary(ND_DEREF, node, tok);
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident();
            continue;
        }

        return node;
    }
}

// stmt-expr = "( "{" stmt stmt* "}" ")"
// 
// Statement expression is a GNU C extension.
// https://gcc.gnu.org/onlinedocs/gcc-4.8.3/gcc/Statement-Exprs.html#Statement-Exprs
Node *stmt_expr(Token *tok) {
    VarScope *sc1 = var_scope;
    TagScope *sc2 = tag_scope;

    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    expect(")");

    var_scope = sc1;
    tag_scope = sc2;

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
//            | "sizeof" "(" type-name ")"
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

    if (tok = consume("sizeof")) {
        if (consume("(")) {
            if (is_typename()) {
                Type *ty = type_name();
                expect(")");
                return new_num(size_of(ty), tok);
            }
            token = tok->next;
        }
        return new_unary(ND_SIZEOF, unary(), tok);
    }
    
    // 変数または関数　識別子
    if (tok = consume_ident()) {
        // 先に関数かチェック
        if (consume("(")) {
            Node *node = new_node(ND_FUNCALL, tok);
            node->funcname = str_n_dup(tok->str, tok->len);
            node->args = func_args(); // 末尾はfunc_argsでチェック

            VarScope *sc = find_var(tok);
            if (sc) {
                if (!sc->var || sc->var->ty->kind != TY_FUNC)
                    error_tok(tok, "関数ではありません");
                node->ty = sc->var->ty->return_ty;
            } else {
                node->ty = int_type();
            }
            return node;
        }
        
        VarScope *sc = find_var(tok);
        if (sc && sc->var)
            return new_var(sc->var, tok);
        error_tok(tok, "未定義の変数");
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
    return new_num(expect_number(), tok);
}
