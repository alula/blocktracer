/* An implementation of the MT19937 Algorithm for the Mersenne Twister
 * by Evan Sultanik.  Based upon the pseudocode in: M. Matsumoto and
 * T. Nishimura, "Mersenne Twister: A 623-dimensionally
 * equidistributed uniform pseudorandom number generator," ACM
 * Transactions on Modeling and Computer Simulation Vol. 8, No. 1,
 * January pp.3-30 1998.
 *
 * http://www.sultanik.com/Mersenne_twister
 */

#define UPPER_MASK 0x80000000
#define LOWER_MASK 0x7fffffff
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000

#define STATE_VECTOR_LENGTH 624
#define STATE_VECTOR_M 397 /* changes to STATE_VECTOR_LENGTH also require changes to this */

typedef struct tagMTRand
{
    unsigned long mt[STATE_VECTOR_LENGTH];
    int index;
} MTRand;

void seedRand(MTRand *rand, unsigned long seed);
unsigned long genRandLong(MTRand *rand);
float genRand(MTRand *rand);

void seedRand(MTRand *rand, unsigned long seed)
{
    // set initial seeds to mt[STATE_VECTOR_LENGTH] using the generator
    // from Line 25 of Table 1 in: Donald Knuth, "The Art of Computer
    // Programming," Vol. 2 (2nd Ed.) pp.102.
    rand->mt[0] = seed & 0xffffffff;
    for (rand->index = 1; rand->index < STATE_VECTOR_LENGTH; rand->index++)
    {
        rand->mt[rand->index] = (6069 * rand->mt[rand->index - 1]) & 0xffffffff;
    }
}

/**
 * Generates a pseudo-randomly generated long.
 */
unsigned long genRandLong(MTRand *rand)
{

    unsigned long y;
    if (rand->index >= STATE_VECTOR_LENGTH || rand->index < 0)
    {
        /* generate STATE_VECTOR_LENGTH words at a time */
        int kk;
        if (rand->index >= STATE_VECTOR_LENGTH + 1 || rand->index < 0)
        {
            seedRand(rand, 4357);
        }
        for (kk = 0; kk < STATE_VECTOR_LENGTH - STATE_VECTOR_M; kk++)
        {
            y = (rand->mt[kk] & UPPER_MASK) | (rand->mt[kk + 1] & LOWER_MASK);
            rand->mt[kk] = rand->mt[kk + STATE_VECTOR_M] ^ (y >> 1) ^ ((y & 0x1) ? 0x9908b0df : 0);
        }
        for (; kk < STATE_VECTOR_LENGTH - 1; kk++)
        {
            y = (rand->mt[kk] & UPPER_MASK) | (rand->mt[kk + 1] & LOWER_MASK);
            rand->mt[kk] = rand->mt[kk + (STATE_VECTOR_M - STATE_VECTOR_LENGTH)] ^ (y >> 1) ^ ((y & 0x1) ? 0x9908b0df : 0);
        }
        y = (rand->mt[STATE_VECTOR_LENGTH - 1] & UPPER_MASK) | (rand->mt[0] & LOWER_MASK);
        rand->mt[STATE_VECTOR_LENGTH - 1] = rand->mt[STATE_VECTOR_M - 1] ^ (y >> 1) ^ ((y & 0x1) ? 0x9908b0df : 0);
        rand->index = 0;
    }
    y = rand->mt[rand->index++];
    y ^= (y >> 11);
    y ^= (y << 7) & TEMPERING_MASK_B;
    y ^= (y << 15) & TEMPERING_MASK_C;
    y ^= (y >> 18);
    return y;
}

/**
 * Generates a pseudo-randomly generated double in the range [0..1].
 */
float genRand(MTRand *rand)
{
    return ((float)genRandLong(rand) / (unsigned long)0xffffffff);
}