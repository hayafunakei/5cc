#include "5cc.h"

//
// コードジェネレータ
//

char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
char *argreg2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
char *argreg4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

int labelseq = 1; // ラベル連番
int brkseq;
int contseq;
char *funcname;

void gen(Node *node);

// 指定されたノードのアドレスをスタックにプッシュする
// ┌スタック
// [] → [アドレス]
void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR: {
        Var *var = node->var;
        if (var->is_local) {
            printf("  lea rax, [rbp-%d]\n", var->offset); // lea…srcの"アドレス"(rbp-offset)をdest(rax)に入れる
            printf("  push rax\n");
        } else {
            printf("  push offset %s\n", var->name);
        }
        return;
    }
    case ND_DEREF:
        gen(node->lhs);
        return;
    case ND_MEMBER:
        gen_addr(node->lhs); // 構造体のスタート位置
        printf("  pop rax\n");
        printf("  add rax, %d\n", node->member->offset);
        printf("  push rax\n");
        return;
    }

    error_tok(node->tok, "変数ではありません");
}

// 指定されたノードのlvalueのアドレスをプッシュする
// ┌スタック
// [] → [アドレス]
void gen_lval(Node *node) {
    if (node->ty->kind == TY_ARRAY)
        error_tok(node->tok, "lvalueではありません");
    gen_addr(node);
}

// 値を取り出す
// ┌スタック
// [アドレス] → [値]
void load(Type *ty) { // アドレスをpushしておく
    printf("  pop rax\n"); // 変数アドレスを取得する
    
    int sz = size_of(ty, NULL);
    if (sz == 1) {
        printf("  movsx rax, byte ptr [rax]\n");
    } else if (sz == 2) {
        printf("  movsx rax, word ptr [rax]\n");
    } else if (sz == 4) {
        printf("  movsxd rax, dword ptr [rax]\n");
    } else {
        printf("  mov rax, [rax]\n"); // 第二が[]…アドレスが指す箇所の数値を取り出す
    }

    printf("  push rax\n");
}

// 値を保存する
// ┌スタック
// [アドレス,値] → [値]
void store(Type *ty) {
    printf("  pop rdi\n");
    printf("  pop rax\n");

    if (ty->kind == TY_BOOL) {    // ZF(ゼロフラグ)･･･演算結果が0ならZFは1
        printf("  cmp rdi, 0\n"); // false(0)か判定　0ならZFは1 それ以外ZFは0
        printf("  setne dil\n");  // ZFが0ならdilに1をセットする　そうでなければ0をセット
        printf("  movzb rdi, dil\n");
    }

    int sz = size_of(ty, NULL);

    if (sz == 1 ) {
        printf("  mov [rax], dil\n"); 
    } else if (sz == 2) {
        printf("  mov [rax], di\n");
    } else if (sz == 4) {
        printf("  mov [rax], edi\n");
    } else {
        assert(sz == 8);
        printf("  mov [rax], rdi\n"); // raxのアドレス先に値をセットする
    }

    printf("  push rdi\n");
}

void truncate(Type *ty) {
    printf("  pop rax\n");

    if (ty->kind == TY_BOOL) {
        printf("  cmp rax, 0\n");
        printf("  setne al\n");
    }

    int sz = size_of(ty, NULL);
    if (sz == 1) {
        printf("  movsx rax, al\n");
    } else if (sz == 2) {
        printf("  movsx rax, ax\n");
    } else if (sz == 4) {
        printf("  movsxd rax, eax\n");
    }
    printf("  push rax\n");
}

// スタックトップの値をインクリメント
// ┌スタック
// [値] → [値+1]
void inc(Node *node) {
    int sz = node->ty->base ? size_of(node->ty->base, node->tok) : 1;
    printf("  pop rax\n");
    printf("  add rax, %d\n", sz);
    printf("  push rax\n");
}

// スタックトップの値をデクリメント
// ┌スタック
// [値] → [値-1]
void dec(Node *node) {
    int sz = node->ty->base ? size_of(node->ty->base, node->tok) : 1;
    printf("  pop rax\n");
    printf("  sub rax, %d\n", sz);
    printf("  push rax\n");
}

void gen(Node *node) {
    switch (node->kind) {
    case ND_NULL:
        return;
    case ND_NUM:
        if (node->val == (int)node->val) {
            printf("  push %ld\n", node->val);
        } else {
            printf("  movabs rax, %ld\n", node->val);
            printf("  push rax\n");
        }
        return;
    case ND_EXPR_STMT:
        gen(node->lhs);
        // nodeの根本 returnではない文。プッシュされたスタックトップ(計算結果)は破棄しておく。
        // 破棄しないと、文が増えるほど使わないスタックが溜まってしまうため。
        printf("  add rsp, 8\n"); // RSPは1単位64bit(8バイト) 
        return;
    case ND_VAR:
    case ND_MEMBER:
        gen_addr(node);
        if (node->ty->kind != TY_ARRAY)
            load(node->ty);
        return;
    case ND_ASSIGN:
        gen_lval(node->lhs);
        gen(node->rhs);
        store(node->ty);
        return;  
    case ND_TERNARY: {
        int seq = labelseq++;
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lelse%d\n", seq);
        gen(node->then);
        printf("  jmp .Lend%d\n",  seq);
        printf(".Lelse%d:\n", seq);
        gen(node->els);
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_PRE_INC:              // []
        gen_lval(node->lhs);      // → [変数アドレス]
        printf("  push [rsp]\n"); // → [変数アドレス,変数アドレス] ↑と同じ変数アドレスをプッシュ[rsp]とすれば再計算は不要
        load(node->ty);           // → [変数アドレス,値]
        inc(node);            // → [変数アドレス,値+1]
        store(node->ty);          // → [値+1] 加算後の値が次の計算に渡される
        return;
    case ND_PRE_DEC:
        gen_lval(node->lhs); 
        printf("  push [rsp]\n");
        load(node->ty);
        dec(node);
        store(node->ty);
        return;
    case ND_POST_INC:             
        gen_lval(node->lhs);      
        printf("  push [rsp]\n");
        load(node->ty);
        inc(node);
        store(node->ty);
        dec(node); // 加算"前"の値が次の計算に渡される
        return;
    case ND_POST_DEC:
        gen_lval(node->lhs);
        printf("  push [rsp]\n");
        load(node->ty);
        dec(node);
        store(node->ty);
        inc(node);
        return;
    case ND_A_ADD:
    case ND_A_SUB:
    case ND_A_MUL:
    case ND_A_DIV: 
    case ND_A_SHL:
    case ND_A_SHR: {
        gen_lval(node->lhs);
        printf("  push [rsp]\n");
        load(node->lhs->ty);
        gen(node->rhs);
        printf("  pop rdi\n");
        printf("  pop rax\n");

        switch (node->kind) {
        case ND_A_ADD:
            if (node->ty->base)
                printf("  imul rdi, %d\n", size_of(node->ty->base, node->tok));
            printf("  add rax, rdi\n");
            break;
        case ND_A_SUB:
            if (node->ty->base)
                printf("  imul rdi, %d\n", size_of(node->ty->base, node->tok));
            printf("  sub rax, rdi\n");
            break;
        case ND_A_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_A_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case ND_A_SHL:
            printf("  mov cl, dil\n"); // clを使うためrdiから下位8bitをコピー
            printf("  shl rax, cl\n"); // shl左にビットシフト シフトするビット数はclレジスタで指定
            break;
        case ND_A_SHR:
            printf("  mov cl, dil\n");
            printf("  sar rax, cl\n");
            break;
        }

        printf("  push rax\n");
        store(node->ty);
        return;
    }

    case ND_COMMA:
        gen(node->lhs);
        gen(node->rhs);
        return;
    case ND_ADDR: // &
        gen_addr(node->lhs); // スタックにアドレスが残る
        return;
    case ND_DEREF: // *
        gen(node->lhs);
        if (node->ty->kind != TY_ARRAY)
            load(node->ty); // lhsの結果をアドレスとして読み込む
        return;
    case ND_NOT:
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");    // raxと0を比較 rax(0)→ZFが1, rax(0以外)→ZFが0
        printf("  sete al\n");       // ZFが1なら1, 0なら0をalにセット
        printf("  movzb rax, al\n");  
        printf("  push rax\n");
        return;
    case ND_BITNOT:
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  not rax\n");
        printf("  push rax\n");
        return;
    case ND_LOGAND: {
        int seq = labelseq++;
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lfalse%d\n", seq);
        gen(node->rhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lfalse%d\n", seq);
        printf("  push 1\n");
        printf("  jmp .Lend%d\n", seq);
        printf(".Lfalse%d:\n", seq);
        printf("  push 0\n");
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_LOGOR: {
        int seq = labelseq++;
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", seq);
        gen(node->rhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", seq);
        printf("  push 0\n");
        printf("  jmp .Lend%d\n", seq);
        printf(".Ltrue%d:\n", seq);
        printf("  push 1\n");
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_IF: {
        int seq = labelseq++;
        if (node->els) { // elseあり
            gen(node->cond); // 実行時スタックに計算結果が残るはず
            printf("  pop rax\n");
            printf("  cmp rax, 0\n"); // cmp 0(false)か判定 結果はZFを見る 同一:1　それ以外:0 
            printf("  je  .Lelse%d\n", seq); // (ZF == 1)なら
            gen(node->then);
            printf("  jmp .Lend%d\n", seq);

            printf(".Lelse%d:\n", seq);
            gen(node->els);

            printf(".Lend%d:\n", seq);
        } else {
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .Lend%d\n", seq);
            gen(node->then);

            printf(".Lend%d:\n", seq);
        }
        return;
    }
    case ND_WHILE: {
        int seq = labelseq++;
        int brk = brkseq;
        int cont = contseq;
        brkseq = contseq = seq;

        printf(".L.continue.%d:\n", seq);
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L.break.%d\n", seq);
        gen(node->then);
        printf("  jmp .L.continue.%d\n", seq);
        printf(".L.break.%d:\n", seq);

        brkseq = brk;
        contseq = cont;
        return;
    }
    case ND_FOR: {
        int seq = labelseq++;
        int brk = brkseq;
        int cont = contseq;
        brkseq = contseq = seq;

        // init → cond → then → inc → then → ...
        if (node->init)
            gen(node->init);
        printf(".Lbegin%d:\n", seq);
        if (node->cond) {
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .L.break.%d\n", seq);
        }
        gen(node->then);
        printf(".L.continue.%d:\n", seq);
        if (node->inc)
            gen(node->inc);
        printf("  jmp .Lbegin%d\n", seq);
        printf(".L.break.%d:\n", seq);

        brkseq = brk;
        contseq = cont;
        return;
    }
    case ND_SWITCH: {
        int seq = labelseq++;
        int brk = brkseq;
        brkseq = seq;
        node->case_label = seq;

        gen(node->cond);
        printf("  pop rax\n");

        for (Node *n = node->case_next; n; n = n->case_next) {
            n->case_label = labelseq++;
            n->case_end_label = seq;
            printf("  cmp rax, %ld\n", n->val);
            printf("  je .L.case.%d\n", n->case_label);
        }

        if (node->default_case) {
            int i = labelseq++;
            node->default_case->case_end_label = seq;
            node->default_case->case_label = i;
            printf("  jmp .L.case.%d\n", i);
        }

        printf("  jmp .L.break.%d\n", seq);
        gen(node->then);
        printf(".L.break.%d:\n", seq);

        brkseq = brk;
        return;
    }
    case ND_CASE:
        printf(".L.case.%d:\n", node->case_label);
        gen(node->lhs);
        printf("  jmp .L.break.%d\n", node->case_end_label);
        return;
    case ND_BLOCK:
    case ND_STMT_EXPR:
        for (Node *n = node->body; n; n = n->next)
            gen(n);
        return;
    case ND_BREAK:
        if (brkseq == 0)
            error_tok(node->tok, "ループ内に無いbreak");
        printf("  jmp .L.break.%d\n", brkseq);
        return;
    case ND_CONTINUE:
        if (contseq == 0)
            error_tok(node->tok, "ループ内に無いcontinue");
        printf("  jmp .L.continue.%d\n", contseq);
        return;
    case ND_GOTO:
        printf("  jmp .L.label.%s.%s\n", funcname, node->label_name);
        return;
    case ND_LABEL:
        printf(".L.label.%s.%s:\n", funcname, node->label_name);
        gen(node->lhs);
        return;
    case ND_FUNCALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen(arg);
            nargs++;
        }
        
        for (int i = nargs - 1; i >= 0; i--)
            printf("  pop %s\n", argreg8[i]);
        
        // x86-64 ABIの要件により
        // 関数を呼び出す前にRSPを16バイトの倍数に合わせる必要がある
        // x86-64では呼び出し時のRSPが16の倍数であることを前提にしている命令を含む関数があるため
        int seq = labelseq++;
        printf("  mov rax, rsp\n");
        printf("  and rax, 15\n"); // 下位4bitに1がある場合 16の倍数ではない
        printf("  jnz .Lcall%d\n", seq); // jnz ZFが0でないなら飛ぶ
        printf("  mov rax, 0\n"); // そのままでOK 
        printf("  call %s\n", node->funcname);
        printf("  jmp .Lend%d\n", seq);
        printf(".Lcall%d:\n", seq);
        printf("  sub rsp, 8\n"); // 8バイトRSPを進めて、アドレスを16の倍数に揃える(前提としてlocal変数のoffsetを8バイトの倍数に整列しておく)
        printf("  mov rax, 0\n");
        printf("  call %s\n", node->funcname);
        printf("  add rsp, 8\n"); // 戻ってきた後、進めた分を戻す
        printf(".Lend%d:\n", seq);
        printf("  push rax\n");

        if (node->ty->kind != TY_VOID)
            truncate(node->ty);
        return;
    }
    case ND_RETURN:
        gen(node->lhs);
        // スタックトップに式全体の値が残っているはずなので
        // それをRAXにロードして関数からの返値とする
        printf("  pop rax\n");
        printf("  jmp .Lreturn.%s\n", funcname);
        return;
    case ND_CAST:
        gen(node->lhs);
        truncate(node->ty);
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
    case ND_ADD:
        if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base, node->tok));
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base, node->tok));
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv rdi\n");
        break;
    case ND_BITAND:
        printf("  and rax, rdi\n");
        break;
    case ND_BITOR:
        printf("  or rax, rdi\n");
        break;
    case ND_BITXOR:
        printf("  xor rax, rdi\n");
        break;
    case ND_SHL:
        printf("  mov cl, dil\n");
        printf("  shl rax, cl\n");
        break;
    case ND_SHR:
        printf("  mov cl, dil\n");
        printf("  sar rax, cl\n");
        break;
    case ND_EQ:
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_NE:
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LT:
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LE:
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        break;
    }

    printf("  push rax\n"); // 計算した結果がRAXに残るはずなのでpushする。
}

void emit_data(Program *prog) {
    printf(".data\n"); // データセクション グローバル変数や文字リテラルなど

    for (VarList *vl = prog->globals; vl; vl = vl->next) {
        Var *var = vl->var;
        printf("%s:\n", var->name);

        if (!var->contents) {
            printf("  .zero %d\n", size_of(var->ty, var->tok)); // サイズ数バイト宣言し、0で埋める
            continue;
        }

        for (int i = 0; i < var->cont_len; i++)
            printf("  .byte %d\n", var->contents[i]);
    }
}

void load_arg(Var *var, int idx) {
    int sz = size_of(var->ty, var->tok);
    if (sz == 1) {
        printf("  mov [rbp-%d], %s\n", var->offset, argreg1[idx]);
    } else if (sz == 2) {
        printf("  mov [rbp-%d], %s\n", var->offset, argreg2[idx]);
    } else if (sz == 4) { 
        printf("  mov [rbp-%d], %s\n", var->offset, argreg4[idx]);
    }else {
        assert(sz == 8);
        printf("  mov [rbp-%d], %s\n", var->offset, argreg8[idx]);
    }
}

void emit_text(Program *prog) {
    printf(".text\n");
    
    for (Function *fn = prog->fns; fn; fn = fn->next) {
        printf(".globl %s\n", fn->name);
        printf("%s:\n", fn->name);
        funcname = fn->name;

        // プロローグ
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        // 変数分の領域を確保する
        printf("  sub rsp, %d\n", fn->stack_size); 
        
        // 引数の値をセットする
        int i = 0;
        for (VarList *vl = fn->params; vl; vl = vl->next) {
            load_arg(vl->var, i++);
        }

        for(Node *node = fn->node; node; node = node->next) {
            // 抽象構文木を下りながら一文ごとコード生成
            gen(node);
        }

        // エピローグ
        printf(".Lreturn.%s:\n", funcname);
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n"); // RBPを呼び出し元RBPアドレスに戻す。popすることでRSPはリターンアドレスを指す。
        printf("  ret\n"); // RSPが指すアドレスに戻る（呼び出し元アドレス）
    }
}

void codegen(Program *prog) {
    printf(".intel_syntax noprefix\n");
    emit_data(prog);
    emit_text(prog);
}
// rdi 第一引数() rsi, rdx, rcx
// edi 第一引数(32bit)