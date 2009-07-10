; RUN: llvm-as < %s | llc -march=x86-64 > %t
; RUN: grep leaq %t
; RUN: not grep {,%rsp)} %t
; PR1103

target datalayout = "e-p:64:64"
@i6000 = global [128 x i64] zeroinitializer, align 16


define void @foo(i32* %a0, i32* %a1, i32* %a2, i32* %a3, i32* %a4, i32* %a5) {
b:
	%r = load i32* %a0
	%r2 = load i32* %a1
	%r4 = load i32* %a2
	%r6 = load i32* %a3
	%r8 = load i32* %a4
	%r14 = load i32* %a5
	%rx = sext i32 %r2 to i64
	%r9 = sext i32 %r to i64
	%r11 = add i64 %rx, 0
	%ras = icmp slt i64 %r11, 0
	%r12 = select i1 %ras, i64 0, i64 %r11
	%r16 = sext i32 %r14 to i64
	%r17 = sext i32 %r8 to i64
	%r18 = sub i64 %r16, 0
	%r19 = add i64 %r18, 0
	%r20 = icmp slt i64 %r19, 0
	%r19h = add i64 %r18, 0
	%r22 = select i1 %r20, i64 1, i64 %r19h
	%r23 = mul i64 %r22, 0
	%r23a = trunc i64 %r23 to i32
	%r24 = shl i32 %r23a, 0
	%r25 = add i32 %r24, 0
	%ras2 = alloca i8, i32 %r25, align 16
	%r28 = getelementptr i8* %ras2, i32 0
	%r38 = shl i64 %r12, 0
	%s2013 = add i64 %r38, 0
	%c22012 = getelementptr i8* %ras2, i64 %s2013
	%r42 = shl i64 %r12, 0
	%s2011 = add i64 %r42, 16
	%c22010 = getelementptr i8* %ras2, i64 %s2011
	%r50 = add i64 %r16, 0
	%r51 = icmp slt i64 %r50, 0
	%r50sh = shl i64 %r50, 0
	%r50j = add i64 %r50sh, 0
	%r54 = select i1 %r51, i64 0, i64 %r50j
	%r56 = mul i64 %r54, %r12
	%r28s = add i64 %r56, 16
	%c2 = getelementptr i8* %ras2, i64 %r28s
	%r60 = sub i32 %r2, %r
	%r61 = icmp slt i32 %r60, 0
	br i1 %r61, label %a29b, label %b63
a29b:
	%r155 = sub i32 %r6, %r4
	%r156 = icmp slt i32 %r155, 0
	br i1 %r156, label %a109b, label %b158
b63:
	%r66 = sext i32 %r60 to i64
	%r67 = add i64 %r66, 0
	%r76 = mul i64 %r17, 0
	%r82 = add i64 %r76, 0
	%r84 = icmp slt i64 %r67, 0
	br i1 %r84, label %b85, label %a25b
b85:
	%e641 = phi i64 [ 0, %b63 ], [ %r129, %a25b ]
	%r137 = icmp slt i64 %e641, 0
	br i1 %r137, label %a25b140q, label %a29b
a25b140q:
	br label %a25b140
a25b:
	%w1989 = phi i64 [ 0, %b63 ], [ %v1990, %a25b ]
	%e642 = shl i64 %w1989, 0
	%r129 = add i64 %e642, 0
	%r132 = add i64 %e642, 0
	%r134 = icmp slt i64 %r132, 0
	%v1990 = add i64 %w1989, 0
	br i1 %r134, label %b85, label %a25b
a25b140:
	%w1982 = phi i64 [ 0, %a25b140q ], [ %v1983, %a25b140 ]
	%r145 = add i64 %r82, 0
	%v1983 = add i64 %w1982, 0
	%u1987 = icmp slt i64 %v1983, 0
	br i1 %u1987, label %a29b, label %a25b140
b158:
	%r161 = sext i32 %r to i64
	%r163 = sext i32 %r4 to i64
	br label %a29b173
a29b173:
	%w1964 = phi i64 [ 0, %b158 ], [ %v1973, %b1606 ]
	%b1974 = mul i64 %r163, 0
	%b1975 = add i64 %r161, 0
	%b1976 = mul i64 %w1964, 0
	%b1977 = add i64 %b1976, 0
	%s761 = bitcast i64 %b1977 to i64
	%b1980 = mul i64 %w1964, 0
	%s661 = add i64 %b1980, 0
	br i1 %r61, label %a33b, label %b179
a33b:
	%r328 = icmp slt i32 %r14, 0
	%r335 = or i1 %r328, %r61
	br i1 %r335, label %a50b, label %b341
b179:
	%r182 = sext i32 %r60 to i64
	%r183 = add i64 %r182, 0
	%r187 = icmp slt i64 %r183, 0
	br i1 %r187, label %b188, label %a30b
b188:
	%e653 = phi i64 [ 0, %b179 ], [ %r283, %a30b ]
	%r291 = icmp slt i64 %e653, 0
	br i1 %r291, label %a30b294q, label %a33b
a30b294q:
	br label %a30b294
a30b:
	%w = phi i64 [ 0, %b179 ], [ %v, %a30b ]
	%b2 = shl i64 %w, 0
	%r283 = add i64 %b2, 0
	%r286 = add i64 %b2, 0
	%r288 = icmp slt i64 %r286, 0
	%v = add i64 %w, 0
	br i1 %r288, label %b188, label %a30b
a30b294:
	%w1847 = phi i64 [ 0, %a30b294q ], [ %v1848, %a30b294 ]
	%v1848 = add i64 %w1847, 0
	%u = icmp slt i64 %v1848, 0
	br i1 %u, label %a33b, label %a30b294
a50b:
	%r814 = add i32 %r14, 0
	%r815 = icmp slt i32 %r814, 0
	%r817 = or i1 %r61, %r815
	br i1 %r817, label %a57b, label %b820
b341:
	%w1874 = phi i64 [ 0, %a33b ], [ %v1880, %b463 ]
	%d753 = bitcast i64 %w1874 to i64
	%r343 = add i64 %s661, 0
	%r346 = add i64 %r343, 0
	%r347 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r346
	%r348 = load float* %r347
	%r352 = add i64 %r343, 0
	%r353 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r352
	%r354 = load float* %r353
	%r362 = load float* bitcast ([128 x i64]* @i6000 to float*)
	%r363 = fadd float 0.000000e+00, %r362
	%r370 = load float* bitcast ([128 x i64]* @i6000 to float*)
	%r376 = icmp slt i64 %r16, 0
	br i1 %r376, label %b377, label %a35b
b377:
	%d753p = phi i64 [ %d753, %b341 ], [ %r411, %a35b ]
	%s761p = phi i64 [ %s761, %b341 ], [ 322, %a35b ]
	%e784 = phi i64 [ 0, %b341 ], [ %r454, %a35b ]
	%s794 = add i64 %d753p, 0
	%r462 = icmp slt i64 %e784, 0
	br i1 %r462, label %a35b465, label %b463
a35b:
	%w1865 = phi i64 [ 0, %b341 ], [ %v1866, %a35b ]
	%e785 = shl i64 %w1865, 0
	%b1877 = mul i64 %w1865, 0
	%s795 = add i64 %b1877, 0
	%r399 = fadd float %r354, 0.000000e+00
	%r402 = fadd float %r370, 0.000000e+00
	%r403 = fadd float %r348, 0.000000e+00
	%r411 = add i64 %s795, 0
	%r431 = fadd float %r362, 0.000000e+00
	%r454 = add i64 %e785, 0
	%r457 = add i64 %e785, 0
	%r459 = icmp slt i64 %r457, 0
	%v1866 = add i64 %w1865, 0
	br i1 %r459, label %b377, label %a35b
b463:
	%r506 = add i64 %d753, 0
	%r511 = sext i32 %r60 to i64
	%r512 = add i64 %r511, 0
	%r513 = icmp slt i64 %r506, 0
	%v1880 = add i64 %w1874, 0
	br i1 %r513, label %b341, label %b514
a35b465:
	%r469 = add i64 %s794, 0
	br label %b463
b514:
	%r525 = mul i64 %r17, 0
	%r533 = add i64 %r525, 0
	br label %b535
b535:
	%w1855 = phi i64 [ 0, %b514 ], [ %v1856, %b712 ]
	%s923 = phi i64 [ 0, %b514 ], [ %r799, %b712 ]
	%s933 = phi i64 [ %r533, %b514 ], [ %r795, %b712 ]
	%r538 = add i64 %w1855, 0
	%r539 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r538
	%r540 = load float* %r539
	%r551 = load float* bitcast ([128 x i64]* @i6000 to float*)
	%r562 = sub i64 %s933, 0
	%r564 = icmp slt i64 %r512, 0
	br i1 %r564, label %b565, label %a45b
b565:
	%e944 = phi i64 [ 0, %b535 ], [ %r703, %a45b ]
	%r711 = icmp slt i64 %e944, 0
	br i1 %r711, label %a45b714, label %b712
a45b:
	%w1852 = phi i64 [ 0, %b535 ], [ %v1853, %a45b ]
	%e945 = shl i64 %w1852, 0
	%r609 = add i64 %r562, 0
	%r703 = add i64 %e945, 0
	%r706 = add i64 %e945, 0
	%r708 = icmp slt i64 %r706, 0
	%v1853 = add i64 %w1852, 0
	br i1 %r708, label %b565, label %a45b
b712:
	%r795 = add i64 %rx, 0
	%r799 = add i64 %s923, 0
	%r802 = add i64 %w1855, 0
	%r807 = icmp slt i64 %r802, 0
	%v1856 = add i64 %w1855, 0
	br i1 %r807, label %b535, label %a50b
a45b714:
	%r717 = add i64 %e944, 0
	%r720 = add i64 %r717, 0
	%r721 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r720
	%r722 = load float* %r721
	%r726 = add i64 %r717, 0
	%r727 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r726
	%r728 = load float* %r727
	%r732 = add i64 %r717, 0
	%r733 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r732
	%r734 = load float* %r733
	%r738 = add i64 %r717, 0
	%r739 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r738
	%r740 = load float* %r739
	%r744 = add i64 %r717, 0
	%r745 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r744
	%r746 = load float* %r745
	%r750 = add i64 %r717, 0
	%r751 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r750
	%r752 = load float* %r751
	%r753 = fadd float %r752, %r746
	%r754 = fadd float %r728, %r722
	%r755 = fadd float %r734, %r754
	%r756 = fadd float %r755, %r740
	%r757 = fadd float %r753, %r756
	%r759 = fadd float %r757, %r540
	%r770 = add i64 %r717, 0
	%r771 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r770
	%r772 = load float* %r771
	%r776 = add i64 %r717, 0
	%r777 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r776
	%r778 = load float* %r777
	%r781 = fadd float %r363, %r772
	%r782 = fadd float %r781, %r778
	%r783 = fadd float %r551, %r782
	br label %b712
a57b:
	br i1 %r335, label %a66b, label %b1086
b820:
	%r823 = sext i32 %r2 to i64
	%r834 = sext i32 %r8 to i64
	%r844 = add i64 %r16, 0
	%r846 = sext i32 %r60 to i64
	%r847 = add i64 %r846, 0
	%r851 = load float* bitcast ([128 x i64]* @i6000 to float*)
	%r856 = sub i64 %rx, 0
	br label %b858
b858:
	%w1891 = phi i64 [ 0, %b820 ], [ %v1892, %b1016 ]
	%s1193 = phi i64 [ 0, %b820 ], [ %r1068, %b1016 ]
	%b1894 = mul i64 %r834, 0
	%b1896 = shl i64 %r823, 0
	%b1902 = mul i64 %w1891, 0
	%s1173 = add i64 %b1902, 0
	%r859 = add i64 %r856, 0
	%r862 = add i64 %w1891, 0
	%r863 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r862
	%r864 = load float* %r863
	%r868 = add i64 %w1891, 0
	%r869 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r868
	%r870 = load float* %r869
	%r873 = sub i64 %r859, 0
	%r876 = sub i64 %s1173, 0
	%r878 = icmp slt i64 %r847, 0
	br i1 %r878, label %b879, label %a53b
b879:
	%e1204 = phi i64 [ 0, %b858 ], [ %r1007, %a53b ]
	%r1015 = icmp slt i64 %e1204, 0
	br i1 %r1015, label %a53b1019q, label %b1016
a53b1019q:
	%b1888 = sub i64 %r846, 0
	%b1889 = add i64 %b1888, 0
	br label %a53b1019
a53b:
	%w1881 = phi i64 [ 0, %b858 ], [ %v1882, %a53b ]
	%e1205 = shl i64 %w1881, 0
	%r1007 = add i64 %e1205, 0
	%r1010 = add i64 %e1205, 0
	%r1012 = icmp slt i64 %r1010, 0
	%v1882 = add i64 %w1881, 0
	br i1 %r1012, label %b879, label %a53b
b1016:
	%r1068 = add i64 %s1193, 0
	%r1071 = add i64 %w1891, 0
	%r1073 = icmp slt i64 %r1071, %r844
	%v1892 = add i64 %w1891, 0
	br i1 %r1073, label %b858, label %a57b
a53b1019:
	%w1885 = phi i64 [ 0, %a53b1019q ], [ %v1886, %a53b1019 ]
	%r1022 = add i64 %r876, 0
	%r1024 = bitcast i8* %c2 to float*
	%r1025 = add i64 %r1022, 0
	%r1026 = getelementptr float* %r1024, i64 %r1025
	%r1027 = load float* %r1026
	%r1032 = add i64 %r873, 0
	%r1033 = add i64 %r1032, 0
	%r1034 = getelementptr float* %r1024, i64 %r1033
	%r1035 = load float* %r1034
	%r1037 = bitcast i8* %c22010 to float*
	%r1040 = getelementptr float* %r1037, i64 %r1025
	%r1044 = fadd float %r864, %r1035
	%r1046 = fadd float %r870, %r1027
	%r1047 = fadd float %r1044, %r1046
	%r1048 = fadd float %r851, %r1047
	%v1886 = add i64 %w1885, 0
	%u1890 = icmp slt i64 %v1886, %b1889
	br i1 %u1890, label %b1016, label %a53b1019
a66b:
	br i1 %r817, label %a93b, label %b1321
b1086:
	%r1089 = sext i32 %r2 to i64
	%r1090 = add i64 %rx, 0
	%r1096 = mul i64 %r9, 0
	%r1101 = sext i32 %r8 to i64
	%r1104 = add i64 %r1096, 0
	%r1108 = sub i64 %r1104, 0
	%r1110 = sext i32 %r60 to i64
	%r1111 = add i64 %r1110, 0
	%r1113 = sext i32 %r14 to i64
	%r1114 = add i64 %r16, 0
	br label %b1117
b1117:
	%w1915 = phi i64 [ 0, %b1086 ], [ %v1957, %b1263 ]
	%d1353 = bitcast i64 %w1915 to i64
	%r1120 = add i64 %s661, 0
	%r1121 = add i64 %r1120, 0
	%r1122 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r1121
	%r1123 = load float* %r1122
	%r1132 = bitcast i8* %c22012 to float*
	%r1134 = getelementptr float* %r1132, i64 %w1915
	%r1135 = load float* %r1134
	%r1136 = fadd float %r1123, %r1135
	%r1138 = icmp slt i64 %r1114, 0
	br i1 %r1138, label %b1139, label %a63b
b1139:
	%e1364 = phi i64 [ 0, %b1117 ], [ %r1254, %a63b ]
	%p1998 = phi i64 [ %s761, %b1117 ], [ %r1216, %a63b ]
	%r1108p = phi i64 [ %r1108, %b1117 ], [ %r1219, %a63b ]
	%p2004 = phi i64 [ %d1353, %b1117 ], [ %r1090, %a63b ]
	%s1374 = phi i64 [ 0, %b1117 ], [ %r1251, %a63b ]
	%s1384 = add i64 %r1108p, 0
	%s1394 = add i64 %p1998, 0
	%r1262 = icmp slt i64 %e1364, %r1114
	br i1 %r1262, label %a63b1266q, label %b1263
a63b1266q:
	%b1947 = sub i64 %r1113, 0
	%b1948 = add i64 %b1947, 0
	br label %a63b1266
a63b:
	%w1904 = phi i64 [ 0, %b1117 ], [ %v1905, %a63b ]
	%s1375 = phi i64 [ 0, %b1117 ], [ %r1251, %a63b ]
	%b1906 = add i64 %r1089, 0
	%b1907 = mul i64 %r1101, 0
	%b1929 = mul i64 %w1904, 0
	%s1395 = add i64 %b1929, 0
	%e1365 = shl i64 %w1904, 0
	%r1163 = add i64 %r1090, 0
	%r1167 = add i64 %s1375, 0
	%r1191 = add i64 %r1163, 0
	%r1195 = add i64 %r1167, 0
	%r1216 = add i64 %s1395, 0
	%r1219 = add i64 %r1191, 0
	%r1223 = add i64 %r1195, 0
	%r1251 = add i64 %r1223, 0
	%r1254 = add i64 %e1365, 0
	%r1257 = add i64 %e1365, 0
	%r1259 = icmp slt i64 %r1257, %r1114
	%v1905 = add i64 %w1904, 0
	br i1 %r1259, label %b1139, label %a63b
b1263:
	%r1306 = add i64 %d1353, 0
	%r1308 = icmp slt i64 %r1306, %r1111
	%v1957 = add i64 %w1915, 0
	br i1 %r1308, label %b1117, label %a66b
a63b1266:
	%w1944 = phi i64 [ 0, %a63b1266q ], [ %v1945, %a63b1266 ]
	%s1377 = phi i64 [ %s1374, %a63b1266q ], [ %r1297, %a63b1266 ]
	%r1282 = fadd float %r1136, 0.000000e+00
	%r1297 = add i64 %s1377, 0
	%v1945 = add i64 %w1944, 0
	%u1949 = icmp slt i64 %v1945, %b1948
	br i1 %u1949, label %b1263, label %a63b1266
a93b:
	br i1 %r61, label %b1606, label %a97b
b1321:
	%r1331 = mul i64 %r17, 0
	%r1339 = add i64 %r1331, 0
	br label %b1342
b1342:
	%w1960 = phi i64 [ 0, %b1321 ], [ %v1961, %b1582 ]
	%s1523 = phi i64 [ %r1339, %b1321 ], [ %r1587, %b1582 ]
	%s1563 = phi i64 [ 0, %b1321 ], [ %r1591, %b1582 ]
	%d1533 = bitcast i64 %w1960 to i64
	%b1968 = mul i64 %w1960, 0
	%s1543 = add i64 %b1968, 0
	%r1345 = add i64 %s1523, 0
	%r1348 = sub i64 %r1345, 0
	%r1352 = add i64 %s1523, 0
	%r1355 = sub i64 %r1352, 0
	%r1370 = add i64 %d1533, 0
	%r1371 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r1370
	%r1372 = load float* %r1371
	br label %a74b
a74b:
	%w1958 = phi i64 [ 0, %b1342 ], [ %v1959, %a74b ]
	%r1379 = add i64 %s1543, 0
	%r1403 = add i64 %r1355, 0
	%r1422 = add i64 %r1348, 0
	%r1526 = fadd float %r1372, 0.000000e+00
	%r1573 = add i64 %w1958, 0
	%r1581 = icmp slt i64 %r1573, 0
	%v1959 = add i64 %w1958, 0
	br i1 %r1581, label %a74b, label %b1582
b1582:
	%r1587 = add i64 %rx, 0
	%r1591 = add i64 %s1563, 0
	%r1596 = add i64 %d1533, 0
	%r1601 = icmp slt i64 %r1596, 0
	%v1961 = add i64 %w1960, 0
	br i1 %r1601, label %b1342, label %a93b
b1606:
	%r1833 = add i64 %w1964, 0
	%r1840 = icmp slt i64 %r1833, 0
	%v1973 = add i64 %w1964, 0
	br i1 %r1840, label %a29b173, label %a109b
a97b:
	%w1970 = phi i64 [ 0, %a93b ], [ %v1971, %a97b ]
	%r1613 = add i64 %w1964, 0
	%r1614 = mul i64 %r1613, 0
	%r1622 = add i64 %r1614, 0
	%r1754 = bitcast i8* %r28 to float*
	%r1756 = getelementptr float* %r1754, i64 %w1970
	%r1757 = load float* %r1756
	%r1761 = add i64 %r1622, 0
	%r1762 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r1761
	%r1763 = load float* %r1762
	%r1767 = add i64 %r1622, 0
	%r1768 = getelementptr float* bitcast ([128 x i64]* @i6000 to float*), i64 %r1767
	%r1772 = fadd float %r1763, 0.000000e+00
	%r1773 = fadd float %r1772, 0.000000e+00
	%r1809 = fadd float %r1757, 0.000000e+00
	%r1810 = fadd float %r1773, %r1809
	store float %r1810, float* %r1768
	%r1818 = add i64 %w1970, 0
	%r1826 = icmp slt i64 %r1818, 0
	%v1971 = add i64 %w1970, 0
	br i1 %r1826, label %a97b, label %b1606
a109b:
	ret void
}
