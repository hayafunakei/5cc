#include "5cc.h"

// alignで指定したサイズの倍数に切り上げて整列します
int align_to(int n, int align) {
    // 例:n=90, align=8
    // (90 + 7)=97(0110 0001)で次にあるalignの倍数以上の数にする。
    // NOT 7(1111 1000)のANDをとり、align以下のビットを0にするとalignの倍数となる→96
    return (n + align -1) & ~(align - 1);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "引数の個数が正しくありません\n");
        return 1;
    }
    
    // トークナイズしてパースする
    user_input = argv[1];
    token = tokenize();
    Program *prog = program();
    add_type(prog);
    
    for (Function *fn = prog->fns; fn; fn = fn->next) {
        // ローカル変数のオフセット値を設定する。
        //　一つあたりの領域はsize_ofで型サイズを確認して個別に求める。
        int offset = 0;
        for (VarList *vl = fn->locals; vl; vl = vl->next) {
            Var *var = vl->var;
            offset += size_of(var->ty);
            var->offset = offset;
        }
        fn->stack_size = align_to(offset, 8);

    }

    // コードを生成する
    codegen(prog);

    return 0;
}
