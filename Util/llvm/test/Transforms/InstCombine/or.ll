; This test makes sure that these instructions are properly eliminated.
;
; RUN: opt < %s -instcombine -S | FileCheck %s

define i32 @test1(i32 %A) {
        %B = or i32 %A, 0
        ret i32 %B
; CHECK: @test1
; CHECK: ret i32 %A
}

define i32 @test2(i32 %A) {
        %B = or i32 %A, -1 
        ret i32 %B
; CHECK: @test2
; CHECK: ret i32 -1
}

define i8 @test2a(i8 %A) {
        %B = or i8 %A, -1  
        ret i8 %B
; CHECK: @test2a
; CHECK: ret i8 -1
}

define i1 @test3(i1 %A) {
        %B = or i1 %A, false
        ret i1 %B
; CHECK: @test3
; CHECK: ret i1 %A
}

define i1 @test4(i1 %A) {
        %B = or i1 %A, true 
        ret i1 %B
; CHECK: @test4
; CHECK: ret i1 true
}

define i1 @test5(i1 %A) {
        %B = or i1 %A, %A   
        ret i1 %B
; CHECK: @test5
; CHECK: ret i1 %A
}

define i32 @test6(i32 %A) {
        %B = or i32 %A, %A  
        ret i32 %B
; CHECK: @test6
; CHECK: ret i32 %A
}

; A | ~A == -1
define i32 @test7(i32 %A) {
        %NotA = xor i32 -1, %A
        %B = or i32 %A, %NotA
        ret i32 %B
; CHECK: @test7
; CHECK: ret i32 -1
}

define i8 @test8(i8 %A) {
        %B = or i8 %A, -2
        %C = or i8 %B, 1
        ret i8 %C
; CHECK: @test8
; CHECK: ret i8 -1
}

; Test that (A|c1)|(B|c2) == (A|B)|(c1|c2)
define i8 @test9(i8 %A, i8 %B) {
        %C = or i8 %A, 1
        %D = or i8 %B, -2
        %E = or i8 %C, %D
        ret i8 %E
; CHECK: @test9
; CHECK: ret i8 -1
}

define i8 @test10(i8 %A) {
        %B = or i8 %A, 1
        %C = and i8 %B, -2
        ; (X & C1) | C2 --> (X | C2) & (C1|C2)
        %D = or i8 %C, -2
        ret i8 %D
; CHECK: @test10
; CHECK: ret i8 -2
}

define i8 @test11(i8 %A) {
        %B = or i8 %A, -2
        %C = xor i8 %B, 13
        ; (X ^ C1) | C2 --> (X | C2) ^ (C1&~C2)
        %D = or i8 %C, 1
        %E = xor i8 %D, 12
        ret i8 %E
; CHECK: @test11
; CHECK: ret i8 -1
}

define i32 @test12(i32 %A) {
        ; Should be eliminated
        %B = or i32 %A, 4
        %C = and i32 %B, 8
        ret i32 %C
; CHECK: @test12
; CHECK: %C = and i32 %A, 8
; CHECK: ret i32 %C
}

define i32 @test13(i32 %A) {
        %B = or i32 %A, 12
        ; Always equal to 8
        %C = and i32 %B, 8
        ret i32 %C
; CHECK: @test13
; CHECK: ret i32 8
}

define i1 @test14(i32 %A, i32 %B) {
        %C1 = icmp ult i32 %A, %B
        %C2 = icmp ugt i32 %A, %B
        ; (A < B) | (A > B) === A != B
        %D = or i1 %C1, %C2
        ret i1 %D
; CHECK: @test14
; CHECK: %D = icmp ne i32 %A, %B
; CHECK: ret i1 %D
}

define i1 @test15(i32 %A, i32 %B) {
        %C1 = icmp ult i32 %A, %B
        %C2 = icmp eq i32 %A, %B
        ; (A < B) | (A == B) === A <= B
        %D = or i1 %C1, %C2
        ret i1 %D
; CHECK: @test15
; CHECK: %D = icmp ule i32 %A, %B
; CHECK: ret i1 %D
}

define i32 @test16(i32 %A) {
        %B = and i32 %A, 1
        ; -2 = ~1
        %C = and i32 %A, -2
        ; %D = and int %B, -1 == %B
        %D = or i32 %B, %C
        ret i32 %D
; CHECK: @test16
; CHECK: ret i32 %A
}

define i32 @test17(i32 %A) {
        %B = and i32 %A, 1
        %C = and i32 %A, 4
        ; %D = and int %B, 5
        %D = or i32 %B, %C
        ret i32 %D
; CHECK: @test17
; CHECK: %D = and i32 %A, 5
; CHECK: ret i32 %D
}

define i1 @test18(i32 %A) {
        %B = icmp sge i32 %A, 100
        %C = icmp slt i32 %A, 50
        ;; (A-50) >u 50
        %D = or i1 %B, %C
        ret i1 %D
; CHECK: @test18
; CHECK: add i32
; CHECK: %D = icmp ugt 
; CHECK: ret i1 %D
}

define i1 @test19(i32 %A) {
        %B = icmp eq i32 %A, 50
        %C = icmp eq i32 %A, 51
        ;; (A-50) < 2
        %D = or i1 %B, %C
        ret i1 %D
; CHECK: @test19
; CHECK: add i32
; CHECK: %D = icmp ult 
; CHECK: ret i1 %D
}

define i32 @test20(i32 %x) {
        %y = and i32 %x, 123
        %z = or i32 %y, %x
        ret i32 %z
; CHECK: @test20
; CHECK: ret i32 %x
}

define i32 @test21(i32 %tmp.1) {
        %tmp.1.mask1 = add i32 %tmp.1, 2
        %tmp.3 = and i32 %tmp.1.mask1, -2
        %tmp.5 = and i32 %tmp.1, 1
        ;; add tmp.1, 2
        %tmp.6 = or i32 %tmp.5, %tmp.3
        ret i32 %tmp.6
; CHECK: @test21
; CHECK:   add i32 %{{[^,]*}}, 2
; CHECK:   ret i32 
}

define i32 @test22(i32 %B) {
        %ELIM41 = and i32 %B, 1
        %ELIM7 = and i32 %B, -2
        %ELIM5 = or i32 %ELIM41, %ELIM7
        ret i32 %ELIM5
; CHECK: @test22
; CHECK: ret i32 %B
}

define i16 @test23(i16 %A) {
        %B = lshr i16 %A, 1
        ;; fold or into xor
        %C = or i16 %B, -32768
        %D = xor i16 %C, 8193
        ret i16 %D
; CHECK: @test23
; CHECK:   %B = lshr i16 %A, 1
; CHECK:   %D = xor i16 %B, -24575
; CHECK:   ret i16 %D
}

; PR1738
define i1 @test24(double %X, double %Y) {
        %tmp9 = fcmp uno double %X, 0.000000e+00                ; <i1> [#uses=1]
        %tmp13 = fcmp uno double %Y, 0.000000e+00               ; <i1> [#uses=1]
        %bothcond = or i1 %tmp13, %tmp9         ; <i1> [#uses=1]
        ret i1 %bothcond
        
; CHECK: @test24
; CHECK:   %bothcond = fcmp uno double %Y, %X              ; <i1> [#uses=1]
; CHECK:   ret i1 %bothcond
}

; PR3266 & PR5276
define i1 @test25(i32 %A, i32 %B) {
  %C = icmp eq i32 %A, 0
  %D = icmp eq i32 %B, 57
  %E = or i1 %C, %D
  %F = xor i1 %E, -1
  ret i1 %F

; CHECK: @test25
; CHECK: icmp ne i32 %A, 0
; CHECK-NEXT: icmp ne i32 %B, 57
; CHECK-NEXT:  %F = and i1 
; CHECK-NEXT:  ret i1 %F
}
