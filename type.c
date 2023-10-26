#include "5cc.h"

Type *new_type(TypeKind kind) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    return ty;
}

Type *char_type() {
    return new_type(TY_CHAR);
}

Type *int_type() {
    return new_type(TY_INT);
}

Type *pointer_to(Type *base) {
    Type *ty = new_type(TY_PTR);
    ty->base = base;
    return ty;
}

Type *array_of(Type *base, int size) {
    Type *ty = new_type(TY_ARRAY);
    ty->base = base;
    ty->array_size = size;
    return ty;
}

int size_of(Type *ty) {
    switch (ty->kind) {
    case TY_CHAR:
        return 1;
    case TY_INT:
    case TY_PTR:
        return 8;
    case TY_ARRAY:
        return size_of(ty->base) * ty->array_size;
    default:
        assert(ty->kind == TY_STRUCT);
        Member *mem = ty->members;
        while (mem->next)
            mem = mem->next;
        return mem->offset + size_of(mem->ty);
    }
}

Member *find_member(Type *ty, char *name) {
    assert(ty->kind == TY_STRUCT);
    for (Member *mem = ty->members; mem; mem = mem->next)
        if (!strcmp(mem->name, name))
            return mem;
    return NULL;
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
            error_tok(node->tok, "無効なポインタ演算子 アドレスで減算することは禁止です。右辺はarithmetic typeである必要があります。");
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_MEMBER: {
        if (node->lhs->ty->kind != TY_STRUCT)
            error_tok(node->tok, "構造体ではありません");
        node->member = find_member(node->lhs->ty, node->member_name);
        if (!node->member)
            error_tok(node->tok, "指定されたメンバーが存在しません");
        node->ty = node->member->ty;
        return;
    }
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
    case ND_STMT_EXPR: {
        Node *last = node->body;
        while (last->next)
            last = last->next;
        node->ty = last->ty;
        return;
    }
    }
     

}

void add_type(Program *prog) {
    for (Function *fn = prog->fns; fn; fn = fn->next)
        for (Node *node = fn->node; node; node = node->next)
            visit(node);
}