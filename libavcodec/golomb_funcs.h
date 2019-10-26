/*
 * exp golomb vlc stuff
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2004 Alex Beregszaszi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * @brief
 *     exp golomb vlc stuff
 * @author Michael Niedermayer <michaelni@gmx.at> and Alex Beregszaszi
 */

#ifndef AVCODEC_GOLOMB_FUNCS_H
#define AVCODEC_GOLOMB_FUNCS_H

#ifdef GOLOMB_FUNCS_GB

/**
 * Read an unsigned Exp-Golomb code in the range 0 to 8190.
 *
 * @returns the read value or a negative error code.
 */
int get_ue_golomb(GetBitContext *gb);

/**
 * Read an unsigned Exp-Golomb code in the range 0 to UINT32_MAX-1.
 */
unsigned get_ue_golomb_long(GetBitContext *gb);

/**
 * read unsigned exp golomb code, constraint to a max of 31.
 * the return value is undefined if the stored value exceeds 31.
 */
int get_ue_golomb_31(GetBitContext *gb);

unsigned get_interleaved_ue_golomb(GetBitContext *gb);

/**
 * read unsigned truncated exp golomb code.
 */
int get_te0_golomb(GetBitContext *gb, int range);

/**
 * read unsigned truncated exp golomb code.
 */
int get_te_golomb(GetBitContext *gb, int range);

/**
 * read signed exp golomb code.
 */
int get_se_golomb(GetBitContext *gb);

int get_se_golomb_long(GetBitContext *gb);

int get_interleaved_se_golomb(GetBitContext *gb);

int dirac_get_se_golomb(GetBitContext *gb);

/**
 * read unsigned golomb rice code (ffv1).
 */
int get_ur_golomb(GetBitContext *gb, int k, int limit, int esc_len);

/**
 * read unsigned golomb rice code (jpegls).
 */
int get_ur_golomb_jpegls(GetBitContext *gb, int k, int limit, int esc_len);

/**
 * read signed golomb rice code (ffv1).
 */
int get_sr_golomb(GetBitContext *gb, int k, int limit, int esc_len);

/**
 * read signed golomb rice code (flac).
 */
int get_sr_golomb_flac(GetBitContext *gb, int k, int limit, int esc_len);

/**
 * read unsigned golomb rice code (shorten).
 */
unsigned int get_ur_golomb_shorten(GetBitContext *gb, int k);

/**
 * read signed golomb rice code (shorten).
 */
int get_sr_golomb_shorten(GetBitContext *gb, int k);

#ifdef TRACE

int get_ue(GetBitContext *s, const char *file, const char *func, int line);

int get_se(GetBitContext *s, const char *file, const char *func, int line);

int get_te(GetBitContext *s, int r, char *file, const char *func, int line);

#define get_ue_golomb(a) get_ue(a, __FILE__, __func__, __LINE__)
#define get_se_golomb(a) get_se(a, __FILE__, __func__, __LINE__)
#define get_te_golomb(a, r)  get_te(a, r, __FILE__, __func__, __LINE__)
#define get_te0_golomb(a, r) get_te(a, r, __FILE__, __func__, __LINE__)

#endif /* TRACE */

#endif /* GOLOMB_FUNCS_GB */


#ifdef GOLOMB_FUNCS_PB

/**
 * write unsigned exp golomb code. 2^16 - 2 at most
 */
void set_ue_golomb(PutBitContext *pb, int i);

/**
 * write unsigned exp golomb code. 2^32-2 at most.
 */
void set_ue_golomb_long(PutBitContext *pb, uint32_t i);

/**
 * write truncated unsigned exp golomb code.
 */
void set_te_golomb(PutBitContext *pb, int i, int range);

/**
 * write signed exp golomb code. 16 bits at most.
 */
void set_se_golomb(PutBitContext *pb, int i);

/**
 * write unsigned golomb rice code (ffv1).
 */
void set_ur_golomb(PutBitContext *pb, int i, int k, int limit, int esc_len);

/**
 * write unsigned golomb rice code (jpegls).
 */
void set_ur_golomb_jpegls(PutBitContext *pb, int i, int k, int limit, int esc_len);

/**
 * write signed golomb rice code (ffv1).
 */
void set_sr_golomb(PutBitContext *pb, int i, int k, int limit, int esc_len);

/**
 * write signed golomb rice code (flac).
 */
void set_sr_golomb_flac(PutBitContext *pb, int i, int k, int limit, int esc_len);

#endif /* GOLOMB_FUNCS_PB */

#endif /* AVCODEC_GOLOMB_FUNCS_H */
