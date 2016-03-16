/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.
These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashbig(), mix(), and final() are externally
useful functions.  Routines to test the hash are included if SELF_TEST
is defined.  You can use this free for any purpose.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().

Code changed for ANSI C compilers and autotools by Łukasz Cieśnik.
-------------------------------------------------------------------------------
*/
#ifdef HAVE_CONFIG_H
# include <pbs_config.h>
#endif

#include <stddef.h>

#ifdef HAVE_STDINT_H
# include <stdint.h>
#else
# ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
# endif
#endif


#ifdef SELF_TEST
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#endif

#ifndef lint
static char rcsid[]
# ifdef __GNUC__

# endif
= "$Id: lookup3.c,v 1.2 2006/09/08 18:18:08 ciesnik Exp $";
#endif

#ifdef __cplusplus
extern "C"
{

typedef uint32_t uint32;   /* unsigned 4-byte quantities */
typedef uint16_t uint16;   /* unsigned 2-byte quantities */
typedef uint8_t  uint8;    /* unsigned 1-byte quantities */

#if BYTEORDER == 1234 /* little endian */
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN    0
#endif

#if BYTEORDER == 4321  /* big endian */
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN    1
#endif

#if 0
/*
 * My best guess at if you are big-endian or little-endian.  This may
 * need adjustment.
 */
#if defined(i386) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL)
#  define HASH_LITTLE_ENDIAN 1
#  define HASH_BIG_ENDIAN 0
#elif defined(sparc)
#  define HASH_LITTLE_ENDIAN 0
#  define HASH_BIG_ENDIAN 1
#else
#  define HASH_LITTLE_ENDIAN 0
#  define HASH_BIG_ENDIAN 0
#endif
#endif

#define hashsize(n) ((uint32)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) ^ ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
    { \
    a -= c;  a ^= rot(c, 4);  c += b; \
    b -= a;  b ^= rot(a, 6);  a += c; \
    c -= b;  c ^= rot(b, 8);  b += a; \
    a -= c;  a ^= rot(c,16);  c += b; \
    b -= a;  b ^= rot(a,19);  a += c; \
    c -= b;  c ^= rot(b, 4);  b += a; \
    }

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
    { \
    c ^= b; c -= rot(b,14); \
    a ^= c; a -= rot(c,11); \
    b ^= a; b -= rot(a,25); \
    c ^= b; c -= rot(b,16); \
    a ^= c; a -= rot(c,4);  \
    b ^= a; b -= rot(a,14); \
    c ^= b; c -= rot(b,24); \
    }

/**
 * This works on all machines.  To be useful, it requires
 * -- that the key be an array of uint32's, and
 * -- that all your machines have the same endianness, and
 * -- that the length be the number of uint32's in the key

 * The function hashword() is identical to hashlittle() on little-endian
 * machines, and identical to hashbig() on big-endian machines,
 * except that the length has to be measured in uint32s rather than in
 * bytes.  hashlittle() is more complicated than hashword() only because
 * hashlittle() has to dance around fitting the key bytes into registers.
 *
 * @param k          the key, an array of uint32 values
 * @param length     the length of the key, in uint32s
 * @param initval    the previous hash, or an arbitrary value
 */
uint32 hashword(const uint32 *k, size_t length, uint32 initval)
  {
  uint32 a, b, c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + (((uint32)length) << 2) + initval;

  /*------------------------------------------------- handle most of the key */

  while (length > 3)
    {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a, b, c);
    length -= 3;
    k += 3;
    }

  /*--------------------------------------------- handle the last 3 uint32's */
  switch (length)                    /* all the case statements fall through */
    {

    case 3 :
      c += k[2];

    case 2 :
      b += k[1];

    case 1 :
      a += k[0];
      final(a, b, c);

    case 0:     /* case 0: nothing left to add */
      break;
    }

  /*------------------------------------------------------ report the result */
  return c;
  }


/**
 * hashlittle() -- hash a variable-length key into a 32-bit value
 *  @param key       the key (the unaligned variable-length array of bytes)
 *  @param length    the length of the key, counting by bytes
 *  @param initval   can be any 4-byte value
 *
 * Returns a 32-bit value.  Every bit of the key affects every bit of
 * the return value.  Two keys differing by one or two bits will have
 * totally different hash values.
 *
 * The best hash table sizes are powers of 2.  There is no need to do
 * mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 * use a bitmask.  For example, if you need only 10 bits, do
 *   h = (h & hashmask(10));
 * In which case, the hash table should have hashsize(10) elements.
 *
 * If you are hashing n strings (uint8 **)k, do it like this:
 *   for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);
 *
 * By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
 * code any way you wish, private, educational, or commercial.  It's free.
 *
 * Use for hash table lookup, or anything where one collision in 2^^32 is
 * acceptable.  Do NOT use for cryptographic purposes.
 */
uint32 hashlittle(const void *key, size_t length, uint32 initval)
  {
  uint32 a, b, c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)length) + initval;

  if (HASH_LITTLE_ENDIAN && !((((uint8 *)key) - (uint8 *)0) & 0x3))
    {
    const uint32 *k = (uint32 *)key;                                 /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */

    while (length > 12)
      {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
      }

    /*----------------------------- handle the last (probably partial) block */
    switch (length)
      {

      case 12:
        c += k[2];
        b += k[1];
        a += k[0];
        break;

      case 11:
        c += k[2] & 0xffffff;
        b += k[1];
        a += k[0];
        break;

      case 10:
        c += k[2] & 0xffff;
        b += k[1];
        a += k[0];
        break;

      case 9 :
        c += k[2] & 0xff;
        b += k[1];
        a += k[0];
        break;

      case 8 :
        b += k[1];
        a += k[0];
        break;

      case 7 :
        b += k[1] & 0xffffff;
        a += k[0];
        break;

      case 6 :
        b += k[1] & 0xffff;
        a += k[0];
        break;

      case 5 :
        b += k[1] & 0xff;
        a += k[0];
        break;

      case 4 :
        a += k[0];
        break;

      case 3 :
        a += k[0] & 0xffffff;
        break;

      case 2 :
        a += k[0] & 0xffff;
        break;

      case 1 :
        a += k[0] & 0xff;
        break;

      case 0 :
        return c;              /* zero length strings require no mixing */
      }

    }
  else if (HASH_LITTLE_ENDIAN && !((((uint8 *)key) - (uint8 *)0) & 0x1))
    {
    const uint16 *k = (uint16 *)key;                                   /* read 16-bit chunks */

    /*--------------- all but last block: aligned reads and different mixing */

    while (length > 12)
      {
      a += k[0] + (((uint32)k[1]) << 16);
      b += k[2] + (((uint32)k[3]) << 16);
      c += k[4] + (((uint32)k[5]) << 16);
      mix(a, b, c);
      length -= 12;
      k += 6;
      }

    /*----------------------------- handle the last (probably partial) block */
    switch (length)
      {

      case 12:
        c += k[4] + (((uint32)k[5]) << 16);
        b += k[2] + (((uint32)k[3]) << 16);
        a += k[0] + (((uint32)k[1]) << 16);
        break;

      case 11:
        c += ((uint32)(k[5] & 0xff)) << 16;/* fall through */

      case 10:
        c += k[4];
        b += k[2] + (((uint32)k[3]) << 16);
        a += k[0] + (((uint32)k[1]) << 16);
        break;

      case 9 :
        c += k[4] & 0xff;            /* fall through */

      case 8 :
        b += k[2] + (((uint32)k[3]) << 16);
        a += k[0] + (((uint32)k[1]) << 16);
        break;

      case 7 :
        b += ((uint32)(k[3] & 0xff)) << 16;/* fall through */

      case 6 :
        b += k[2];
        a += k[0] + (((uint32)k[1]) << 16);
        break;

      case 5 :
        b += k[2] & 0xff;            /* fall through */

      case 4 :
        a += k[0] + (((uint32)k[1]) << 16);
        break;

      case 3 :
        a += ((uint32)(k[1] & 0xff)) << 16;/* fall through */

      case 2 :
        a += k[0];
        break;

      case 1 :
        a += k[0] & 0xff;
        break;

      case 0 :
        return c;                     /* zero length requires no mixing */
      }

    }
  else                          /* need to read the key one byte at a time */
    {
    const uint8 *k = (uint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */

    while (length > 12)
      {
      a += k[0];
      a += ((uint32)k[1]) << 8;
      a += ((uint32)k[2]) << 16;
      a += ((uint32)k[3]) << 24;
      b += k[4];
      b += ((uint32)k[5]) << 8;
      b += ((uint32)k[6]) << 16;
      b += ((uint32)k[7]) << 24;
      c += k[8];
      c += ((uint32)k[9]) << 8;
      c += ((uint32)k[10]) << 16;
      c += ((uint32)k[11]) << 24;
      mix(a, b, c);
      length -= 12;
      k += 12;
      }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length)                  /* all the case statements fall through */
      {

      case 12:
        c += ((uint32)k[11]) << 24;

      case 11:
        c += ((uint32)k[10]) << 16;

      case 10:
        c += ((uint32)k[9]) << 8;

      case 9 :
        c += k[8];

      case 8 :
        b += ((uint32)k[7]) << 24;

      case 7 :
        b += ((uint32)k[6]) << 16;

      case 6 :
        b += ((uint32)k[5]) << 8;

      case 5 :
        b += k[4];

      case 4 :
        a += ((uint32)k[3]) << 24;

      case 3 :
        a += ((uint32)k[2]) << 16;

      case 2 :
        a += ((uint32)k[1]) << 8;

      case 1 :
        a += k[0];
        break;

      case 0 :
        return c;
      }
    }

  final(a, b, c);

  return c;
  }



/**
 * hashbig():
 * This is the same as hashword() on big-endian machines.  It is different
 * from hashlittle() on all machines.  hashbig() takes advantage of
 * big-endian byte ordering.
 */
uint32 hashbig(const void *key, size_t length, uint32 initval)
  {
  uint32 a, b, c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)length) + initval;

  if (HASH_BIG_ENDIAN && !((((uint8 *)key) - (uint8 *)0) & 0x3))
    {
    const uint32 *k = (uint32 *)key;                                 /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */

    while (length > 12)
      {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
      }

    /*----------------------------- handle the last (probably partial) block */
    switch (length)
      {

      case 12:
        c += k[2];
        b += k[1];
        a += k[0];
        break;

      case 11:
        c += k[2] << 8;
        b += k[1];
        a += k[0];
        break;

      case 10:
        c += k[2] << 16;
        b += k[1];
        a += k[0];
        break;

      case 9 :
        c += k[2] << 24;
        b += k[1];
        a += k[0];
        break;

      case 8 :
        b += k[1];
        a += k[0];
        break;

      case 7 :
        b += k[1] << 8;
        a += k[0];
        break;

      case 6 :
        b += k[1] << 16;
        a += k[0];
        break;

      case 5 :
        b += k[1] << 24;
        a += k[0];
        break;

      case 4 :
        a += k[0];
        break;

      case 3 :
        a += k[0] << 8;
        break;

      case 2 :
        a += k[0] << 16;
        break;

      case 1 :
        a += k[0] << 24;
        break;

      case 0 :
        return c;              /* zero length strings require no mixing */
      }

    }
  else                          /* need to read the key one byte at a time */
    {
    const uint8 *k = (uint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */

    while (length > 12)
      {
      a += ((uint32)k[0]) << 24;
      a += ((uint32)k[1]) << 16;
      a += ((uint32)k[2]) << 8;
      a += ((uint32)k[3]);
      b += ((uint32)k[4]) << 24;
      b += ((uint32)k[5]) << 16;
      b += ((uint32)k[6]) << 8;
      b += ((uint32)k[7]);
      c += ((uint32)k[8]) << 24;
      c += ((uint32)k[9]) << 16;
      c += ((uint32)k[10]) << 8;
      c += ((uint32)k[11]);
      mix(a, b, c);
      length -= 12;
      k += 12;
      }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length)                  /* all the case statements fall through */
      {

      case 12:
        c += ((uint32)k[11]) << 24;

      case 11:
        c += ((uint32)k[10]) << 16;

      case 10:
        c += ((uint32)k[9]) << 8;

      case 9 :
        c += k[8];

      case 8 :
        b += ((uint32)k[7]) << 24;

      case 7 :
        b += ((uint32)k[6]) << 16;

      case 6 :
        b += ((uint32)k[5]) << 8;

      case 5 :
        b += k[4];

      case 4 :
        a += ((uint32)k[3]) << 24;

      case 3 :
        a += ((uint32)k[2]) << 16;

      case 2 :
        a += ((uint32)k[1]) << 8;

      case 1 :
        a += k[0];
        break;

      case 0 :
        return c;
      }
    }

  final(a, b, c);

  return c;
  }


#ifdef SELF_TEST

/* used for timings */
void driver1(void)
  {
  uint8 buf[256];
  uint32 i;
  uint32 h = 0;
  time_t a, z;

  time(&a);

  for (i = 0; i < 256; ++i) buf[i] = 'x';

  for (i = 0; i < 1; ++i)
    {
    h = hashlittle(&buf[0], 1, h);
    }

  time(&z);

  if (z - a > 0) printf("time %ld %.8lx\n", z - a, h);
  }

/* check that every input bit changes every output bit half the time */
#define HASHSTATE 1
#define HASHLEN   1
#define MAXPAIR 60
#define MAXLEN  70
void driver2(void)
  {
  uint8 qa[MAXLEN+1], qb[MAXLEN+2], *a = &qa[0], *b = &qb[1];
  uint32 c[HASHSTATE], d[HASHSTATE], i, j = 0, k, l, m, z;
  uint32 e[HASHSTATE], f[HASHSTATE], g[HASHSTATE], h[HASHSTATE];
  uint32 x[HASHSTATE], y[HASHSTATE];
  uint32 hlen;

  printf("No more than %d trials should ever be needed \n", MAXPAIR / 2);

  for (hlen = 0; hlen < MAXLEN; ++hlen)
    {
    z = 0;

    for (i = 0; i < hlen; ++i)  /*----------------------- for each input byte, */
      {
      for (j = 0; j < 8; ++j)   /*------------------------ for each input bit, */
        {
        for (m = 1; m < 8; ++m) /*------------ for serveral possible initvals, */
          {
          for (l = 0; l < HASHSTATE; ++l) e[l] = f[l] = g[l] = h[l] = x[l] = y[l] = ~((uint32)0);

          /*---- check that every output bit is affected by that input bit */
          for (k = 0; k < MAXPAIR; k += 2)
            {
            uint32 finished = 1;
            /* keys have one bit different */

            for (l = 0; l < hlen + 1; ++l)
              {
              a[l] = b[l] = (uint8)0;
              }

            /* have a and b be two keys differing in only one bit */
            a[i] ^= (k << j);

            a[i] ^= (k >> (8 - j));

            c[0] = hashlittle(a, hlen, m);

            b[i] ^= ((k + 1) << j);

            b[i] ^= ((k + 1) >> (8 - j));

            d[0] = hashlittle(b, hlen, m);

            /* check every bit is 1, 0, set, and not set at least once */
            for (l = 0; l < HASHSTATE; ++l)
              {
              e[l] &= (c[l] ^ d[l]);
              f[l] &= ~(c[l] ^ d[l]);
              g[l] &= c[l];
              h[l] &= ~c[l];
              x[l] &= d[l];
              y[l] &= ~d[l];

              if (e[l] | f[l] | g[l] | h[l] | x[l] | y[l]) finished = 0;
              }

            if (finished) break;
            }

          if (k > z) z = k;

          if (k == MAXPAIR)
            {
            printf("Some bit didn't change: ");
            printf("%.8lx %.8lx %.8lx %.8lx %.8lx %.8lx  ",
                   e[0], f[0], g[0], h[0], x[0], y[0]);
            printf("i %ld j %ld m %ld len %ld\n", i, j, m, hlen);
            }

          if (z == MAXPAIR) goto done;
          }
        }
      }

done:

    if (z < MAXPAIR)
      {
      printf("Mix success  %2ld bytes  %2ld initvals  ", i, m);
      printf("required  %ld  trials\n", z / 2);
      }
    }

  printf("\n");
  }

/* Check for reading beyond the end of the buffer and alignment problems */
void driver3(void)
  {
  uint8 buf[MAXLEN+20], *b;
  uint32 len;
  uint8 q[] = "This is the time for all good men to come to the aid of their country...";
  uint32 dummy1;
  uint8 qq[] = "xThis is the time for all good men to come to the aid of their country...";
  uint32 dummy2;
  uint8 qqq[] = "xxThis is the time for all good men to come to the aid of their country...";
  uint32 dummy3;
  uint8 qqqq[] = "xxxThis is the time for all good men to come to the aid of their country...";
  uint32 h, i, j, ref, x, y;
  uint8 *p;

  printf("Endianness.  These lines should all be the same (for values filled in):\n");
  printf("%.8lx                            %.8lx                            %.8lx\n",
         hashword((uint32 *)q, (sizeof(q) - 1) / 4, 13),
         hashword((uint32 *)q, (sizeof(q) - 5) / 4, 13),
         hashword((uint32 *)q, (sizeof(q) - 9) / 4, 13));
  p = q;
  printf("%.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx\n",
         hashlittle(p, sizeof(q) - 1, 13), hashlittle(p, sizeof(q) - 2, 13),
         hashlittle(p, sizeof(q) - 3, 13), hashlittle(p, sizeof(q) - 4, 13),
         hashlittle(p, sizeof(q) - 5, 13), hashlittle(p, sizeof(q) - 6, 13),
         hashlittle(p, sizeof(q) - 7, 13), hashlittle(p, sizeof(q) - 8, 13),
         hashlittle(p, sizeof(q) - 9, 13), hashlittle(p, sizeof(q) - 10, 13),
         hashlittle(p, sizeof(q) - 11, 13), hashlittle(p, sizeof(q) - 12, 13));
  p = &qq[1];
  printf("%.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx\n",
         hashlittle(p, sizeof(q) - 1, 13), hashlittle(p, sizeof(q) - 2, 13),
         hashlittle(p, sizeof(q) - 3, 13), hashlittle(p, sizeof(q) - 4, 13),
         hashlittle(p, sizeof(q) - 5, 13), hashlittle(p, sizeof(q) - 6, 13),
         hashlittle(p, sizeof(q) - 7, 13), hashlittle(p, sizeof(q) - 8, 13),
         hashlittle(p, sizeof(q) - 9, 13), hashlittle(p, sizeof(q) - 10, 13),
         hashlittle(p, sizeof(q) - 11, 13), hashlittle(p, sizeof(q) - 12, 13));
  p = &qqq[2];
  printf("%.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx\n",
         hashlittle(p, sizeof(q) - 1, 13), hashlittle(p, sizeof(q) - 2, 13),
         hashlittle(p, sizeof(q) - 3, 13), hashlittle(p, sizeof(q) - 4, 13),
         hashlittle(p, sizeof(q) - 5, 13), hashlittle(p, sizeof(q) - 6, 13),
         hashlittle(p, sizeof(q) - 7, 13), hashlittle(p, sizeof(q) - 8, 13),
         hashlittle(p, sizeof(q) - 9, 13), hashlittle(p, sizeof(q) - 10, 13),
         hashlittle(p, sizeof(q) - 11, 13), hashlittle(p, sizeof(q) - 12, 13));
  p = &qqqq[3];
  printf("%.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx\n",
         hashlittle(p, sizeof(q) - 1, 13), hashlittle(p, sizeof(q) - 2, 13),
         hashlittle(p, sizeof(q) - 3, 13), hashlittle(p, sizeof(q) - 4, 13),
         hashlittle(p, sizeof(q) - 5, 13), hashlittle(p, sizeof(q) - 6, 13),
         hashlittle(p, sizeof(q) - 7, 13), hashlittle(p, sizeof(q) - 8, 13),
         hashlittle(p, sizeof(q) - 9, 13), hashlittle(p, sizeof(q) - 10, 13),
         hashlittle(p, sizeof(q) - 11, 13), hashlittle(p, sizeof(q) - 12, 13));
  printf("\n");

  for (h = 0, b = buf + 1; h < 8; ++h, ++b)
    {
    for (i = 0; i < MAXLEN; ++i)
      {
      len = i;

      for (j = 0; j < i; ++j) *(b + j) = 0;

      /* these should all be equal */
      ref = hashlittle(b, len, (uint32)1);

      *(b + i) = (uint8)~0;

      *(b - 1) = (uint8)~0;

      x = hashlittle(b, len, (uint32)1);

      y = hashlittle(b, len, (uint32)1);

      if ((ref != x) || (ref != y))
        {
        printf("alignment error: %.8lx %.8lx %.8lx %ld %ld\n", ref, x, y, h, i);
        }
      }
    }
  }

/* check for problems with nulls */
void driver4(void)
  {
  uint8 buf[1];
  uint32 h, i, state[HASHSTATE];


  buf[0] = ~0;

  for (i = 0; i < HASHSTATE; ++i) state[i] = 1;

  printf("These should all be different\n");

  for (i = 0, h = 0; i < 8; ++i)
    {
    h = hashlittle(buf, (uint32)0, h);
    printf("%2ld  0-byte strings, hash is  %.8lx\n", i, h);
    }
  }


int
main(void)
  {
  driver1();   /* test that the key is hashed: used for timings */
  driver2();   /* test that whole key is hashed thoroughly */
  driver3();   /* test that nothing but the key is hashed */
  driver4();   /* test hashing multiple buffers (all buffers are null) */
  return 1;
  }

#endif  /* SELF_TEST */

}
#endif // #ifdef __cplusplus
