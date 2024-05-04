#include "5cc.h"

// 指定したファイルの内容を返す
char *read_file(char *path) {
    // ファイルを開いて読み込む
    FILE *fp = fopen(path, "r");
    if(!fp)
        error("\"%s\"は開けませんでした: %s", path, strerror(errno));
    
    int filemax = 10 * 1024 * 1024;
    char *buf = malloc(filemax);
    int size = fread(buf, 1/*byte size*/, filemax - 2, fp);
    if (!feof(fp))
        error("%s: ファイルが大きすぎます");
    
    // ファイルが必ず"\n\0"で終わっているようにする
    if (size == 0 || buf[size - 1] != '\n')
        buf[size++] = '\n';
    buf[size] = '\0';
    return buf;
}


int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "引数の個数が正しくありません\n");
        return 1;
    }
    
    // トークナイズしてパースする
    filename = argv[1];
    user_input = read_file(argv[1]);
    token = tokenize();
    Program *prog = program();
    add_type(prog);
    
    for (Function *fn = prog->fns; fn; fn = fn->next) {
        // ローカル変数のオフセット値を設定する。
        //　一つあたりの領域はsize_ofで型サイズを確認して個別に求める。
        int offset = 0;
        for (VarList *vl = fn->locals; vl; vl = vl->next) {
            Var *var = vl->var;
            offset = align_to(offset, var->ty->align); // タイプに合わせて開始位置を調整する
            offset += size_of(var->ty, var->tok);
            var->offset = offset;
            // RBPから見て後にした宣言→先にした宣言の順に並ぶ
        }
        fn->stack_size = align_to(offset, 8);

    }

    // コードを生成する
    codegen(prog);

    return 0;
}
