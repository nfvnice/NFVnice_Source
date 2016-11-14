coreID  SVCID   INSTID  Weight  TxThrd
8       2       1       1       1
8       3       2       2       1
8       4       3       3       1
9       5       5       1       2
9       6       6       2       2
9       7       7       3       2
10      8       9       1       3
10      9       10      2       3
10      10      11      3       3


L = [2,5,8]
M = [3,6,9]
H = [4,7,10]

Selection Criteria:
L=2 => M=[6,9] and H=[7,10]
L=5 => M=[3,9] and H=[4,10]
L=8 => M=[3,6] and H=[4,7]

Selection Criteria:
M=3 => L=[5,8] and H=[7,10]
M=6 => L=[2,8] and H=[4,10]
M=9 => L=[2,5] and H=[4,7]

Selection Criteria:
H=4 => M=[6,9] and L=[5,8]
H=7 => M=[3,9] and L=[2,8]
H=10 => M=[3,6] and L=[2,5]


Ideal L-L-L SVCID Chain Configurations:
L       L       L
2       5       8
2       8       5
5       2       8
5       8       2
8       2       5
8       5       2

Ideal M-M-M SVCID Chain Configurations:
M       M       M
3       6       9
3       9       6
6       3       9
6       9       3
9       6       3
9       3       6

Ideal H-H-H SVCID Chain Configurations:
H       H       H
4       7       10
4       10      7
7       4       10
7       10      4
10      4       7
10      7       4

Ideal L-M-H SVCID Chain Configurations:
L       M       H
2       6       10
5       9       4
8       3       7
2       9       7
5       3       10
8       6       4 

Ideal H-M-L SVCID Chain Configurations
H       M       L
4       6       8
4       9       5
7       3       8
7       9       2
10      6       2
10      3       5


Ideal H-L-M SVCID Chain Configurations
H       L       M
4       8       6
4       5       9
7       8       3
7       2       9
10      2       6
10      5       3

Random Combinations:
L       L       M
2,      5,      9,      11
M       L       H
3,      8,      7,      11
H       M       H
4,      6,      10,     11
L       M       L
2,      9,      5,      11
M       H       L
3,      10,     5,      11
H       H       M
4,      7,      9,      11

