#include "5cc.h"

//
// コードジェネレータ
//

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
    case ND_VAR:
        gen_addr(node);
        load();
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        gen(node->rhs);
        store();
        return;
    case ND_RETURN:
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
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

    printf("  push rax\n");
}

void codegen(Program *prog) {

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");
    
    // プロローグ
    // 変数分の領域を確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", prog->stack_size); 
    
    // 先頭のノードはprog->nodeを参照する
    for(Node *node = prog->node; node; node = node->next) {
        // 抽象構文木を下りながらコード生成
        gen(node);

        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックがあふれないようにポップしておく
        printf("  pop rax\n");
    }

    // エピローグ
    // スタックトップに式全体の値が残っているはずなので
    // それをRAXにロードして関数からの返値とする
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return;
}

// rdi 第一引数 rsi, rdx, rcx