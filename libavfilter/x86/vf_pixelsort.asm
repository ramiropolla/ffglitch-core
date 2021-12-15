;*****************************************************************************
;* x86-optimized functions for pixelsort filter
;*
;* Copyright (C) 2022 Ramiro Polla
;*
;* This file is part of FFmpeg.
;*
;* FFmpeg is free software; you can redistribute it and/or
;* modify it under the terms of the GNU Lesser General Public
;* License as published by the Free Software Foundation; either
;* version 2.1 of the License, or (at your option) any later version.
;*
;* FFmpeg is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;* Lesser General Public License for more details.
;*
;* You should have received a copy of the GNU Lesser General Public
;* License along with FFmpeg; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
;******************************************************************************

%include "libavutil/x86/x86util.asm"

SECTION_RODATA

_1f: times 8 dd 1.
_2f: times 8 dd 2.
_4f: times 8 dd 4.
_6f: times 8 dd 6.
_510f: times 8 dd 510.
_if: dd 0, 1, 2, 3, 4, 5, 6, 7

SECTION .text

;------------------------------------------------------------------------------
; void rgb_to_hsv(hsvl_base_t *hsvl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length);
; void rgb_to_hsl(hsvl_base_t *hsvl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length);
;------------------------------------------------------------------------------

%macro COPY_FROM_TMP_TO_HSV 2
    ; %1 hsvl offset
    ; %2 rsp offset
%if do_h
    mov r32d, [rsp + %2 + 0*mmsize] ; r32 = H
    mov [hsvlq + %1 +  0], r32d     ; hsvl->h = H
%endif ; do_h
%if do_s
    mov r32d, [rsp + %2 + 1*mmsize] ; r32 = S
    mov [hsvlq + %1 +  4], r32d     ; hsvl->v = S
%endif ; do_s
%if do_vl
    mov r32d, [rsp + %2 + 2*mmsize] ; r32 = V
    mov [hsvlq + %1 +  8], r32d     ; hsvl->v = V
%endif ; do_vl
    mov [hsvlq + %1 + 12], id       ; hsvl->i = I
    inc iq
%endmacro

%macro PMOVZXBD 3
    ; %1 dst
    ; %2 src
    ; %3 zero reg
%if cpuflag(sse4)
    pmovzxbd  %1, %2
%else
    movd      %1, %2
    punpcklbw %1, %3
    punpcklwd %1, %3
%endif ; cpuflag(sse4)
%endmacro

%macro rgb_to_hsvl_xxx_fn 4

%define do_h  %2
%define do_s  %3
%define do_vl %4

%ifidn %1, hsl
%define mmregnum 16
%else ; hsl
%define mmregnum 15
%endif ; hsl

%if cpuflag(sse4)
cglobal rgb_to_%1_%2%3%4, 5,7,mmregnum, 0, hsvl, r, g, b, length, i, r32
%else
cglobal rgb_to_%1_%2%3%4, 5,7,mmregnum, 0 - 3 * mmsize, hsvl, r, g, b, length, i, r32
%endif ; cpuflag(sse4)
%if WIN64
    movsxd   lengthq, lengthd
%endif
    xor      iq,  iq        ; i   = 0
%if do_h
    mova     m9,  [_1f]     ; m9  = float(1, 1, 1, 1)
    mova     m10, [_2f]     ; m10 = float(2, 2, 2, 2)
    mova     m11, [_4f]     ; m11 = float(4, 4, 4, 4)
    mova     m12, [_6f]     ; m12 = float(6, 6, 6, 6)
%endif ; do_h
%if do_s
%ifidn %1, hsl
    mova     m15, [_510f]   ; m15 = float(510, 510, 510, 510)
%endif ; hsl
%endif ; do_s

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- -- -- -- -- -- -- -- 1  2   4   6

.loop:
    pxor     m6,  m6        ; m6  = 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    pcmpeqw  m8,  m8        ; m8  = FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- -- -- -- -- 00 -- FF 1  2   4   6

    ; uint8_t r = *src_r;
    ; uint8_t g = *src_g;
    ; uint8_t b = *src_b;
    PMOVZXBD  m0, [rq + iq], m6 ; m0  = R0 00 00 00 R1 00 00 00 R2 00 00 00 R3 00 00 00
    PMOVZXBD  m1, [gq + iq], m6 ; m1  = G0 00 00 00 G1 00 00 00 G2 00 00 00 G3 00 00 00
    PMOVZXBD  m2, [bq + iq], m6 ; m2  = B0 00 00 00 B1 00 00 00 B2 00 00 00 B3 00 00 00

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; ir ig ib -- -- -- 00 -- FF 1  2   4   6

    cvtdq2ps  m3,  m0       ; m3  = float(R0, R1, R2, R3)
    cvtdq2ps  m4,  m1       ; m4  = float(G0, G1, G2, G3)
    cvtdq2ps  m5,  m2       ; m5  = float(B0, B1, B2, B3)

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; ir ig ib r  g  b  00 -- FF 1  2   4   6
    ; -- -- -- r  g  b  00 -- FF 1  2   4   6

    ; uint8_t m = FFMIN3(r, g, b);
    minps     m0,  m4,  m3  ; m0  = min(r, g)
    minps     m0,  m5       ; m0  = min(r, g, b)

    ; uint8_t v = FFMAX3(r, g, b);
    maxps     m7,  m4,  m3  ; m7  = max(r, g)
    maxps     m7,  m5       ; m7  = max(r, g, b)

%ifidn %1, hsl
    ; uint16_t _2l = (m + v);
    addps     m2,  m0,  m7  ; m2  = m + v
%if do_vl
    ; tmp[2] = _2l
    cvtps2dq  m13, m2       ; m13 = int32(L0 L1 L2 L3)
%endif ; do_vl

%else ; hsl

%if do_vl
    ; tmp[2] = v
    cvtps2dq  m13, m7       ; m13 = int32(V0 V1 V2 V3)
%endif ; do_vl

%endif ; hsl

%if notcpuflag(sse4)
    mova [rsp+2*mmsize], m13 ; tmp[2] = int32(V0 V1 V2 V3) or int32(L0 L1 L2 L3)
%endif

    ; uint8_t c = v - m;
    subps     m1,  m7,  m0  ; m1  = v - m

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; m  c  2l r  g  b  00 v  FF 1  2   4   6
    ; -- c  2l r  g  b  00 v  FF 1  2   4   6

%if do_s

%ifidn %1, hsl
    ; float s = (float) c / FFMIN(_2l, 510-_2l);
    subps     m0,  m15, m2  ; m0  = 510-_2l
    minps     m0,  m2       ; m0  = min(510-_2l, _2l)
    divps     m14, m1,  m0  ; m14 = c / min(510-_2l, _2l)
%else ; hsl
    ; float s = (float) c / v;
    divps     m14, m1,  m7  ; m14 = c / v
%endif ; hsl

    ; if ( c == 0 )
    ;     s = 0;
    cmpeqps   m0,  m1,  m6  ; m0  = (c == 0)
    pxor      m0,  m8       ; m0  = ~(c == 0)
    pand      m14, m0       ; m14 = (c == 0) ? 0 : s;

    ; tmp[1] = s
%if notcpuflag(sse4)
    movaps [rsp+1*mmsize], m14 ; tmp[1] = S0 S1 S2 S3
%endif

%endif ; do_s

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- c  -- r  g  b  00 v  FF 1  2   4   6

%if do_h
    ; start calculating h

    ; float invC = 1. / c;
    divps     m2,  m9,  m1  ; m2  = 1 / c

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- c  _c r  g  b  h  v  mk 1  2   4   6

    ; if ( c == 0 )
    ;     h = 0;
    cmpeqps   m1,  m6       ; m1  = (c == 0)
    pxor      m8,  m1       ; m8  = (c != 0)

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- _c r  g  b  h  v  mk 1  2   4   6

    ; else if ( v == r )
    ;     h =       (float) (g - b) * invC;
    cmpeqps   m0,  m7,  m3  ; m0  = (v == r)
    pand      m0,  m8       ; m0  = (c != 0) && (v == r)
    pxor      m8,  m0       ; m8  = (c != 0) && (v != r)
    subps     m1,  m4,  m5  ; m1  = g - b
    mulps     m1,  m2       ; m1  = (g - b) * invC;
    pand      m1,  m0       ; m1  = (g - b) * invC; [for v == r]
    por       m6,  m1       ; m6  |= m1

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- _c r  g  b  h  v  mk 1  2   4   6

    ; else if ( v == g )
    ;     h = 2.0 + (float) (b - r) * invC;
    cmpeqps   m0,  m7,  m4  ; m0  = (v == g)
    pand      m0,  m8       ; m0  = (c != 0) && (v != r) && (v == g)
    pxor      m8,  m0       ; m8  = (c != 0) && (v != r) && (v != g)
    subps     m5,  m3       ; m5  = b - r
    mulps     m5,  m2       ; m5  = (b - r) * invC;
    addps     m5,  m10      ; m5  += 2.
    pand      m5,  m0       ; m5  = (b - r) * invC; [for v == g]
    por       m6,  m5       ; m6  |= m5

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- _c r  g  -- h  v  mk 1  2   4   6

    ; else /* if ( v == b ) */
    ;     h = 4.0 + (float) (r - g) * invC;
    subps     m3,  m4       ; m3  = r - g
    mulps     m3,  m2       ; m3  = (r - g) * invC;
    addps     m3,  m11      ; m3  += 4.
    pand      m3,  m8       ; m3  = (r - g) * invC; [for v == b]
    por       m6,  m3       ; m6  |= m3

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- -- -- -- -- h  v  -- 1  2   4   6

    ; h = h / 6.;
    divps     m6,  m12      ; m6  = h

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- -- -- -- -- h  v  -- 1  2   4   6

    ; if ( h < 0 )
    ;     h += 1;
    pxor      m0,  m0       ; m0  = 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    cmpnleps  m0,  m6       ; m0  = (h < 0)
    pand      m1,  m0,  m9  ; m1  = 1.; [for h < 0]
    addps     m6,  m1       ; m6  += m1;

    ; m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12
    ; -- -- -- -- -- -- h  v  -- 1  2   4   6

    ; finished calculating h
%endif ; do_h

    ; m6  = H0 H1 H2 H3 H4 H5 H6 H7
    ; m14 = S0 S1 S2 S3 S4 S5 S6 S7
    ; m13 = V0 V1 V2 V3 V4 V5 V6 V7
    ; m4  = I0 I1 I2 I3 I4 I5 I6 I7

%if 0
    ; NOTE: this is *not* faster than extractps or COPY_FROM_TMP_TO_HSV

    movh         m3,  iq    ; m3  = i   XX  XX  XX  XX  XX  XX  XX
    VBROADCASTSS m4,  m3    ; m4  = i   i   i   i   i   i   i   i
    addps        m4,  [_if] ; m4  = i+0 i+1 i+2 i+3 i+4 i+5 i+6 i+7

    unpcklps m0, m6,  m14   ; m0 = H0 S0 H1 S1 H2 S2 H3 S3
    unpckhps m1, m6,  m14   ; m1 = H4 S4 H5 S5 H6 S6 H7 S7
    unpcklps m2, m13, m4    ; m2 = V0 I0 V1 I1 V2 I2 V3 I3
    unpckhps m3, m13, m4    ; m3 = V4 I4 V5 I5 V6 I6 V7 I7
    unpcklpd m4, m0,  m2    ; m4 = H0 S0 V0 I0 H1 S1 V1 I1
    unpckhpd m5, m0,  m2    ; m5 = H2 S2 V2 I2 H3 S3 V3 I3
    unpcklpd m6, m1,  m3    ; m4 = H4 S4 V4 I4 H5 S5 V5 I5
    unpckhpd m7, m1,  m3    ; m5 = H6 S6 V6 I6 H7 S7 V7 I7

    mova [hsvlq + 0*mmsize], m4
    mova [hsvlq + 1*mmsize], m5
    mova [hsvlq + 2*mmsize], m6
    mova [hsvlq + 3*mmsize], m7

    add iq, mmsize/4
%endif

%if cpuflag(sse4)

%if mmsize == 32
    ; NOTE: this a fart faster than COPY_FROM_TMP_TO_HSV

%if do_h
    extractps [hsvlq +   0], xm6,  0
    extractps [hsvlq +  16], xm6,  1
    extractps [hsvlq +  32], xm6,  2
    extractps [hsvlq +  48], xm6,  3
%endif ; do_h

%if do_s
    extractps [hsvlq +   4], xm14, 0
    extractps [hsvlq +  20], xm14, 1
    extractps [hsvlq +  36], xm14, 2
    extractps [hsvlq +  52], xm14, 3
%endif ; do_s

%if do_vl
    extractps [hsvlq +   8], xm13, 0
    extractps [hsvlq +  24], xm13, 1
    extractps [hsvlq +  40], xm13, 2
    extractps [hsvlq +  56], xm13, 3
%endif ; do_vl

%if do_h
    vextracti128 xm6,  m6,  1
%endif ; do_h
%if do_s
    vextracti128 xm14, m14, 1
%endif ; do_s
%if do_vl
    vextracti128 xm13, m13, 1
%endif ; do_vl

%if do_h
    extractps [hsvlq +  64], xm6,  0
    extractps [hsvlq +  80], xm6,  1
    extractps [hsvlq +  96], xm6,  2
    extractps [hsvlq + 112], xm6,  3
%endif ; do_h

%if do_s
    extractps [hsvlq +  68], xm14, 0
    extractps [hsvlq +  84], xm14, 1
    extractps [hsvlq + 100], xm14, 2
    extractps [hsvlq + 116], xm14, 3
%endif ; do_s

%if do_vl
    extractps [hsvlq +  72], xm13, 0
    extractps [hsvlq +  88], xm13, 1
    extractps [hsvlq + 104], xm13, 2
    extractps [hsvlq + 120], xm13, 3
%endif ; do_vl

%else ; mmsize == 32
    ; NOTE: this doesn't seem to be faster than COPY_FROM_TMP_TO_HSV

%if do_h
    extractps [hsvlq +   0], m6,  0
    extractps [hsvlq +  16], m6,  1
    extractps [hsvlq +  32], m6,  2
    extractps [hsvlq +  48], m6,  3
%endif ; do_h

%if do_s
    extractps [hsvlq +   4], m14, 0
    extractps [hsvlq +  20], m14, 1
    extractps [hsvlq +  36], m14, 2
    extractps [hsvlq +  52], m14, 3
%endif ; do_s

%if do_vl
    extractps [hsvlq +   8], m13, 0
    extractps [hsvlq +  24], m13, 1
    extractps [hsvlq +  40], m13, 2
    extractps [hsvlq +  56], m13, 3
%endif ; do_vl

%endif ; mmsize == 32

    mov       [hsvlq +  12], id
    inc iq
    mov       [hsvlq +  28], id
    inc iq
    mov       [hsvlq +  44], id
    inc iq
    mov       [hsvlq +  60], id
    inc iq
%if mmsize == 32
    mov       [hsvlq +  76], id
    inc iq
    mov       [hsvlq +  92], id
    inc iq
    mov       [hsvlq + 108], id
    inc iq
    mov       [hsvlq + 124], id
    inc iq
%endif ; mmsize == 32

%else ; cpuflag(sse4)

%if do_h
    ; store h in tmp[0]
    movaps    [rsp +  0], m6   ; tmp[0] = H0 H1 H2 H3
%endif ; do_h

    ; copy from tmp to hsvl
    COPY_FROM_TMP_TO_HSV (16*0),  0
    COPY_FROM_TMP_TO_HSV (16*1),  4
    COPY_FROM_TMP_TO_HSV (16*2),  8
    COPY_FROM_TMP_TO_HSV (16*3), 12
%if mmsize == 32
    COPY_FROM_TMP_TO_HSV (16*4), 16
    COPY_FROM_TMP_TO_HSV (16*5), 20
    COPY_FROM_TMP_TO_HSV (16*6), 24
    COPY_FROM_TMP_TO_HSV (16*7), 28
%endif ; mmsize == 32

%endif ; cpuflag(sse4)

    add hsvlq, 4 * mmsize

    cmp iq, lengthq
    jb .loop
    REP_RET
%endmacro

%macro rgb_to_hsvl_fn 1
rgb_to_hsvl_xxx_fn %1, 0, 0, 1
rgb_to_hsvl_xxx_fn %1, 0, 1, 0
rgb_to_hsvl_xxx_fn %1, 0, 1, 1
rgb_to_hsvl_xxx_fn %1, 1, 0, 0
rgb_to_hsvl_xxx_fn %1, 1, 0, 1
rgb_to_hsvl_xxx_fn %1, 1, 1, 0
%endmacro

INIT_XMM sse2
rgb_to_hsvl_fn hsv
rgb_to_hsvl_fn hsl

INIT_XMM sse4
rgb_to_hsvl_fn hsv
rgb_to_hsvl_fn hsl

INIT_XMM avx
rgb_to_hsvl_fn hsv
rgb_to_hsvl_fn hsl

INIT_YMM avx2
rgb_to_hsvl_fn hsv
rgb_to_hsvl_fn hsl
