/*
   FIPS 180-2 SHA-224/256/384/512 implementation
   Last update: 02/02/2007
   Issue date:  04/30/2005

   Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the project nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
*/

#ifndef SHA2_H
#define SHA2_H

#define SHA224_DIGEST_SIZE (224 / 8)
#define SHA256_DIGEST_SIZE (256 / 8)
#define SHA384_DIGEST_SIZE (384 / 8)
#define SHA512_DIGEST_SIZE (512 / 8)
#define SHA256_BLOCK_SIZE (512 / 8)
#define SHA512_BLOCK_SIZE (1024 / 8)
#define SHA384_BLOCK_SIZE SHA512_BLOCK_SIZE
#define SHA224_BLOCK_SIZE SHA256_BLOCK_SIZE

#ifndef SHA2_TYPES
#define SHA2_TYPES
typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned long long uint64;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int tot_len;
  unsigned int len;
  unsigned char block[2 * SHA256_BLOCK_SIZE];
  uint32 h[8];
} sha256_ctx;

typedef struct {
  unsigned int tot_len;
  unsigned int len;
  unsigned char block[2 * SHA512_BLOCK_SIZE];
  uint64 h[8];
} sha512_ctx;

typedef sha512_ctx sha384_ctx;
typedef sha256_ctx sha224_ctx;

typedef struct {
  sha224_ctx ctx_inside;
  sha224_ctx ctx_outside;
  /* for hmac_reinit */
  sha224_ctx ctx_inside_reinit;
  sha224_ctx ctx_outside_reinit;
  unsigned char block_ipad[SHA224_BLOCK_SIZE];
  unsigned char block_opad[SHA224_BLOCK_SIZE];
} hmac_sha224_ctx;

typedef struct {
  sha256_ctx ctx_inside;
  sha256_ctx ctx_outside;
  /* for hmac_reinit */
  sha256_ctx ctx_inside_reinit;
  sha256_ctx ctx_outside_reinit;
  unsigned char block_ipad[SHA256_BLOCK_SIZE];
  unsigned char block_opad[SHA256_BLOCK_SIZE];
} hmac_sha256_ctx;

typedef struct {
  sha384_ctx ctx_inside;
  sha384_ctx ctx_outside;
  /* for hmac_reinit */
  sha384_ctx ctx_inside_reinit;
  sha384_ctx ctx_outside_reinit;
  unsigned char block_ipad[SHA384_BLOCK_SIZE];
  unsigned char block_opad[SHA384_BLOCK_SIZE];
} hmac_sha384_ctx;

typedef struct {
  sha512_ctx ctx_inside;
  sha512_ctx ctx_outside;
  /* for hmac_reinit */
  sha512_ctx ctx_inside_reinit;
  sha512_ctx ctx_outside_reinit;
  unsigned char block_ipad[SHA512_BLOCK_SIZE];
  unsigned char block_opad[SHA512_BLOCK_SIZE];
} hmac_sha512_ctx;

#ifdef __cplusplus
}
#endif

#endif /* !SHA2_H */
