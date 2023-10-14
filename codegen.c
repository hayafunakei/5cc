#include "5cc.h"

//
// コードジェネレータ
//

char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

int labelseq = 0; // ラベル連番
char *funcname;

void gen(Node *node);

// 指定されたノードのアドレスをスタックにプッシュする
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
    }

    error_tok(node->tok, "変数ではありません");
}

void gen_lval(Node *node) {
    if (node->ty->kind == TY_ARRAY)
        error_tok(node->tok, "lvalueではありません");
    gen_addr(node);
}

void load() { // アドレスをpushしておく
    printf("  pop rax\n"); // 変数アドレスを取得する
    printf("  mov rax, [rax]\n"); // アドレスが指す箇所の数値を取り出す
    printf("  push rax\n");
}

void store() {
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n"); // raxのアドレス先に値をセットする
    printf("  push rdi\n");
}

void gen(Node *node) {
    switch (node->kind) {
    case ND_NULL:
        return;
    case ND_NUM:
        printf("  push %d\n", node->val);
        return;
    case ND_EXPR_STMT:
        gen(node->lhs);
        // nodeの根本 returnではない文。プッシュされたスタックトップ(計算結果)は破棄しておく。
        // 破棄しないと、文が増えるほど使わないスタックが溜まってしまうため。
        printf("  add rsp, 8\n"); // RSPは1単位64bit(8バイト) 
        return;
    case ND_VAR:
        gen_addr(node);
        if (node->ty->kind != TY_ARRAY)
            load();
        return;
    case ND_ASSIGN:
        gen_lval(node->lhs);
        gen(node->rhs);
        store();
        return;  
    case ND_ADDR: // &
        gen_addr(node->lhs); // スタックにアドレスが残る
        return;
    case ND_DEREF: // *
        gen(node->lhs);
        if (node->ty->kind != TY_ARRAY)
            load(); // lhsの結果をアドレスとして読み込む
        return;
    case ND_IF: {
        int seq = labelseq++;
        if (node->els) { // elseあり
            gen(node->cond); // 実行時スタックに計算結果が残るはず
            printf("  pop rax\n");
            printf("  cmp rax, 0\n"); // cmp 0(false)か判定 ZFにセット 同一:1　それ以外:0 
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
        printf(".Lbegin%d:\n", seq);
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", seq);
        gen(node->then);
        printf("  jmp .Lbegin%d\n", seq);
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_FOR: {
        int seq = labelseq++;
        // init → cond → then → inc → then → ...
        if (node->init)
            gen(node->init);
        printf(".Lbegin%d:\n", seq);
        if (node->cond) {
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .Lend%d\n", seq);
        }
        gen(node->then);
        if (node->inc)
            gen(node->inc);
        printf("  jmp .Lbegin%d\n", seq);
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen(n);
        return;
    case ND_FUNCALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen(arg);
            nargs++;
        }
        
        for (int i = nargs - 1; i >= 0; i--)
            printf("  pop %s\n", argreg[i]);
        
        // x86-64 ABIの要件により
        // 関数を呼び出す前にRSPを16バイトの倍数に合わせる必要がある
        // x86-64では呼び出し時のRSPが16の倍数であることを前提にしている関数があるため
        int seq = labelseq++;
        printf("  mov rax, rsp\n");
        printf("  and rax, 15\n"); // 下位4bitに1がある場合 16の倍数ではない
        printf("  jnz .Lcall%d\n", seq); // jnz ZFが0でないなら飛ぶ
        printf("  mov rax, 0\n"); // そのままでOK 
        printf("  call %s\n", node->funcname);
        printf("  jmp .Lend%d\n", seq);
        printf(".Lcall%d:\n", seq);
        printf("  sub rsp, 8\n"); // 8バイトRSPを進めて、アドレスを16の倍数に揃える(RSPのスタックアドレスは8バイト単位,
                                  // **8バイトより小さい型を追加するときは要修正**  変数は8バイト固定としているため決め打ちで良い)
        printf("  mov rax, 0\n");
        printf("  call %s\n", node->funcname);
        printf("  add rsp, 8\n"); // 戻ってきた後、進めた分を戻す
        printf(".Lend%d:\n", seq);
        printf("  push rax\n");

        return;
    }
    case ND_RETURN:
        gen(node->lhs);
        // スタックトップに式全体の値が残っているはずなので
        // それをRAXにロードして関数からの返値とする
        printf("  pop rax\n");
        printf("  jmp .Lreturn.%s\n", funcname);
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
    case ND_ADD:
        if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base));
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base));
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv rdi\n");
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
        printf("  .zero %d\n", size_of(var->ty)); // サイズ数バイト宣言し、0で埋める
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
            Var *var = vl->var;
            printf("  mov [rbp-%d], %s\n", var->offset, argreg[i++]);
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
// rdi 第一引数 rsi, rdx, rcx