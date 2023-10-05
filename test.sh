#!/bin/bash
assert() {
    expected="$1"
    input="$2"
    
    ./5cc "$input" > tmp.s
    cc -o tmp tmp.s
    ./tmp
    actual="$?"
    
    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $expected expected, but got $actual"
        exit 1
    fi
}

assert 0 "return 0;"
assert 42 "return 42;"
assert 21 "return 5+20-4;"
assert 41 "return 12 + 34 - 5 ;"
assert 47 'return 5+6*7;'
assert 15 'return 5*(9-6);'
assert 4 'return (3+5)/2;'
assert 10 'return -10+20;'
assert 10 'return - -10;'
assert 10 'return - - +10;'
assert 27 'return + - 3 + ( 4 + 5) * 2 + -4 * - 3;'

# 1で真
assert 0 'return 0==1;'
assert 1 'return 42==42;'
assert 1 'return 0!=1;'
assert 0 'return 42!=42;'

assert 1 'return 0<1;'
assert 0 'return 1<1;'
assert 0 'return 2<1;'
assert 1 'return 0<=1;'
assert 1 'return 1<=1;'
assert 0 'return 2<=1;'

assert 1 'return 1>0;'
assert 0 'return 1>1;'
assert 0 'return 1>2;'
assert 1 'return 1>=0;'
assert 1 'return 1>=1;'
assert 0 'return 1>=2;'

assert 3 '1; 2; return 3;'
assert 8 '2;return  3+5 ;'
assert 5 'return b = 2 + 3;'
assert 11 'a = 2; z=7+a; return b = z + a;'

assert 101 'return foo=101;'
assert 15 'return foo123=15;'

assert 50 '2 + 2; 100; return 50; 200;'

assert 3 'if (0) return 2; return 3;'
assert 3 'if (1-1) return 2; return 3;'
assert 2 'if (1) return 2; return 3;'
assert 2 'if (2-1) return 2; return 3;'

assert 10 'i=0; while(i<10) i=i+1; return i;'
assert 2 'f=0; while(f<=50)  f=f+f+1; if(f>60)return 2;return 3;'

assert 55 'i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j;'
assert 3 'for (;;) return 3; return 5;'

assert 3 '{1; {2;} return 3;}'
assert 55 'i=0; j=0; while(i<=10) {j=i+j; i=i+1;} return j;'
assert 15 'j=0; for (i=0; i<10; i=i+1){ j=3; if(i>5){j=10; i=i+1; if(i>8){j = 15; }}} return j;'

echo OK
