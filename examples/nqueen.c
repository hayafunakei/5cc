// How to run:
// 
// $ make
// $ ./5cc examples/nqueen.c > tmp.s
// $ gcc -static -o tmp tmp.s
// $ ./tmp

int print_board(int (*board)[10]) {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++)
            if (board[i][j])
                printf("Q ");
            else
                printf(". ");
            printf("\n");
    }
    printf("\n\n");
}

int conflict( int (*board)[10], int row, int col) {
    for (int i = 0; i < row; i++) {
        if (board[i][col])
            return 1;
        int j = row - i; // 斜めを調べるための列オフセット値 
        if ( 0 < col - j + 1)
            if (board[i][col - j]) // 左上
                return 1;
        if (col + j < 10)
            if (board[i][col + j]) // 右上
                return 1;
    }
    return 0;
}

// 指定した行から始めて、すべてのパターンを探索します
// 確定していたら盤を描画します
int solve(int (*board)[10], int row) {
    if (row > 9) {
        print_board(board);
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        if (conflict(board, row, i)) {
            /* 何もしないで次の列に行く */
        } else {
            board[row][i] = 1;
            solve(board, row + 1); // 最後の行まで　再帰的に探索する
            board[row][i] = 0; // この列の探索は終わったので、Qをクリアして次の列に移る
        }
    }
}

int main() {
    int board[100];
    for (int i = 0; i < 100; i++)
        board[i] = 0;
    solve(board, 0);
    return 0;
}