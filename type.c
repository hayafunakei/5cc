#include "5cc.h"

Type *int_type() {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_INT;
    return ty;
}

Type *pointer_to(Type *base) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_PTR;
    ty->base = base;
    return ty;
}

Type *array_of(Type *base, int size) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_ARRAY;
    ty->base = base;
    ty->array_size = size;
    return ty;
}

int size_of(Type *ty) {
    if (ty->kind == TY_INT || ty->kind == TY_PTR)
        return 8;
    assert(ty->kind == TY_ARRAY);
    return size_of(ty->base) * ty->array_size;
}

void visit(Node *node) {
    if (!node)
        return;
    
    visit(node->lhs);
    visit(node->rhs);
    visit(node->cond);
    visit(node->then);
    visit(node->els);
    visit(node->init);
    visit(node->inc);

    for (Node *n = node->body; n; n = n->next)
        visit(n);
    for (Node *n = node->args; n; n = n->next)
        visit(n);
    
    // 識別子・リテラルノードだけではなく、
    // 演算子ノードにも何の型の計算かセットする。
    switch (node->kind) {
    case ND_MUL:
    case ND_DIV:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_FUNCALL:
    case ND_NUM:
        node->ty = int_type();
        return;
    case ND_VAR:
        node->ty = node->var->ty;
        return;
    case ND_ADD:
        if (node->rhs->ty->base) { // rhsがポインタ型のとき、左右を入れ替えて通ればOK　
            Node *tmp = node->lhs;           // 入れ替えても足し算なので結果は変わらない
            node->lhs = node->rhs;
            node->rhs = tmp;
        }
        if (node->rhs->ty->base)
            error_tok(node->tok, "無効なポインタ演算子 アドレス同士の加算は禁止です"); 
        node->ty = node->lhs->ty;
        return;
    case ND_SUB:
        if (node->rhs->ty->base)
            error_tok(node->tok, "無効なポインタ演算子 アドレスで減算することは禁止です");
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_ADDR: // &
        if (node->lhs->ty->kind == TY_ARRAY) {           // lhsノードのポインタ型を設定する。
            node->ty = pointer_to(node->lhs->ty->base);  // 例:int型であればこのノードはint型ポインタになる。
        } else {
            node->ty = pointer_to(node->lhs->ty); 
        }                                         
        return;
    case ND_DEREF: // *
        if (!node->lhs->ty->base) 
            error_tok(node->tok, "無効なポインタ参照 ポインタ型ではありません");
        node->ty = node->lhs->ty->base;   // lhsのポインタ型を引き継ぐ 
        return;
    case ND_SIZEOF:
        node->kind = ND_NUM;
        node->ty = int_type();
        node->val = size_of(node->lhs->ty);
        node->lhs = NULL;
        return;
    }
     

}

void add_type(Function *prog) {
    for (Function *fn = prog; fn; fn = fn->next)
        for (Node *node = fn->node; node; node = node->next)
            visit(node);
}