#include "5cc.h"

// alignは2のn乗の数を指定する
// alignで指定したサイズの倍数に切り上げた値を返す
int align_to(int n, int align) {
    // 例:n=90, align=8
    // (90 + 7)=97(0110 0001)で次にあるalignの倍数以上の数にする。
    // NOT 7(1111 1000)のANDをとり、align以下のビットを0にするとalignの倍数となる→96(0110 0000)
    return (n + align -1) & ~(align - 1);
}

Type *new_type(TypeKind kind, int align) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    ty->align = align;
    return ty;
}

Type *void_type() {
    return new_type(TY_VOID, 1);
}

Type *bool_type() {
    return new_type(TY_BOOL, 1);
}

Type *char_type() {
    return new_type(TY_CHAR, 1);
}

Type *short_type() {
    return new_type(TY_SHORT, 2);
}

Type *int_type() {
    return new_type(TY_INT, 4);
}

Type *long_type() {
    return new_type(TY_LONG, 8);
}

Type *enum_type() {
    return new_type(TY_ENUM, 4);
}

Type *func_type(Type *return_ty) {
    Type *ty = new_type(TY_FUNC, 1);
    ty->return_ty = return_ty;
    return ty;
}

Type *pointer_to(Type *base) {
    Type *ty = new_type(TY_PTR, 8);
    ty->base = base;
    return ty;
}

Type *array_of(Type *base, int size) {
    Type *ty = new_type(TY_ARRAY, base->align);
    ty->base = base;
    ty->array_size = size;
    return ty;
}

int size_of(Type *ty) {
    assert(ty->kind != TY_VOID);

    switch (ty->kind) {
    case TY_BOOL:
    case TY_CHAR:
        return 1;
    case TY_SHORT:
        return 2;
    case TY_INT:
    case TY_ENUM:
        return 4;
    case TY_LONG:
    case TY_PTR:
        return 8;
    case TY_ARRAY:
        return size_of(ty->base) * ty->array_size;
    default:
        assert(ty->kind == TY_STRUCT);
        Member *mem = ty->members;
        while (mem->next)
            mem = mem->next;
        // 構造体全体のサイズ
        int end = mem->offset + size_of(mem->ty); 
        return align_to(end, ty->align);
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
    case ND_BITAND:
    case ND_BITOR:
    case ND_BITXOR:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NOT:
    case ND_LOGOR:
    case ND_LOGAND:
        node->ty = int_type();
        return;
    case ND_NUM:
        if (node->val == (int)node->val)
            node->ty = int_type();
        else
            node->ty = long_type();
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
            error_tok(node->tok, "無効なポインタ演算 アドレス同士の加算は禁止です"); 
        node->ty = node->lhs->ty;
        return;
    case ND_SUB:
        if (node->rhs->ty->base)
            error_tok(node->tok, "無効なポインタ演算 アドレスで減算することは禁止です。右辺はarithmetic typeである必要があります。");
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
    case ND_PRE_INC:
    case ND_PRE_DEC:
    case ND_POST_INC:
    case ND_POST_DEC:
    case ND_A_ADD:
    case ND_A_SUB:
    case ND_A_MUL:
    case ND_A_DIV:
    case ND_BITNOT:
        node->ty = node->lhs->ty;
        return;
    case ND_COMMA:
        node->ty = node->rhs->ty;
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
            error_tok(node->tok, "無効なポインタ参照外し ポインタ型ではありません");
        node->ty = node->lhs->ty->base;   // lhsのポインタ型を引き継ぐ 
        if (node->ty->kind == TY_VOID)
            error_tok(node->tok, "voidポインタ型を参照外ししようとしています");
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