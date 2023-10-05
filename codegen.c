#include "5cc.h"

//
// コードジェネレータ
//

int labelseq = 0; // ラベル連番

// 指定されたノードのアドレスをスタックにプッシュする
void gen_addr(Node *node) {
    if (node->kind == ND_VAR) {
        printf("  lea rax, [rbp-%d]\n", node->var->offset); // lea…srcの"アドレス(rbp-offset)"をdestに入れる
        printf("  push rax\n");
        return;
    }
}

void load() {
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
    case ND_NUM:
        printf("  push %d\n", node->val);
        return;
    case ND_EXPR_STMT:
        gen(node->lhs);
        // nodeの根本 returnではないためプッシュされたスタックトップ(計算結果)は破棄しておく。
        // 破棄しないと、文が増えるほど使わないスタックが溜まってしまうため。
        printf("  add rsp, 8\n"); // RSPは1単位64bit(8バイト) 
        return;
    case ND_VAR:
        gen_addr(node);
        load();
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        gen(node->rhs);
        store();
        return;
    case ND_IF: {
        int seq = labelseq++;
        if (node->els) { // elseあり
            gen(node->cond); // 実行時スタックに計算結果が残るはず
            printf("  pop rax\n");
            printf("  cmp rax, 0\n"); // cmp 0(false)と比較 ZFにセット 同一:1　それ以外:0 
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
    case ND_RETURN:
        gen(node->lhs);
        // スタックトップに式全体の値が残っているはずなので
        // それをRAXにロードして関数からの返値とする
        printf("  pop rax\n");
        printf("  jmp .Lreturn\n");
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
    case ND_ADD:
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
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

void codegen(Program *prog) {

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");
    
    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    // 変数分の領域を確保する
    printf("  sub rsp, %d\n", prog->stack_size); 
    
    // 先頭のノードはprog->nodeを参照する
    for(Node *node = prog->node; node; node = node->next) {
        // 抽象構文木を下りながらコード生成
        gen(node);
    }

    // エピローグ
    printf(".Lreturn:\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n"); // RBPを呼び出し元RBPアドレスに戻す。popすることでRSPはリターンアドレスを指す。
    printf("  ret\n"); // RSPが指すアドレスに戻る（呼び出し元アドレス）
    return;
}

// rdi 第一引数 rsi, rdx, rcx