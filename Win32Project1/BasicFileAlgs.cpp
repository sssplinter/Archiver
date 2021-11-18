#include "stdafx.h"
#include "BasicFileAlgs.h"

using namespace BFA;

BasicFileAlgs::BasicFileAlgs(void)
{
}

static systimer_t global_timer;/* Global timer resource */

void InitTimer(void)
{
	#if defined(WINDOWS_TIMER)

	  __int64 freq;

	  global_timer.period = 0.0;
	  if( QueryPerformanceFrequency( (LARGE_INTEGER *)&freq ) )
	  {
		global_timer.period = 1.0 / (double) freq;
		QueryPerformanceCounter( (LARGE_INTEGER *)&global_timer.t0 );
	  }
	  else
		global_timer.t0 = (__int64) GetTickCount();

	#elif defined(DOS_TIMER)

	  global_timer.t0 = uclock();

	#elif defined(GTOD_TIMER)

	  gettimeofday( &global_timer.t0, NULL );

	#endif
}

double GetTime(void)
{
	#if defined(WINDOWS_TIMER)

	  __int64 t;

	  if( global_timer.period > 0.0 )
	  {
		QueryPerformanceCounter( (LARGE_INTEGER *)&t );
		return global_timer.period * (double) (t - global_timer.t0);
	  }
	  else
		return 0.001 * (double) (GetTickCount() - (int) global_timer.t0);

	#elif defined(DOS_TIMER)

	  return (1.0 / UCLOCKS_PER_SEC) * (double) (uclock() - global_timer.t0);

	#elif defined(GTOD_TIMER)

	  struct timeval tv;

	  gettimeofday( &tv, NULL );

	  tv.tv_sec -= global_timer.t0.tv_sec;
	  tv.tv_usec -= global_timer.t0.tv_usec;
	  if( tv.tv_usec < 0 )
	  {
		--tv.tv_sec;
		tv.tv_usec += 1000000;
	  }

	  return (double) tv.tv_sec + 0.000001 * (double) tv.tv_usec;

	#else

	  return 0.0;

	#endif
}

static void _Huffman_InitBitstream(huff_bitstream_t *stream, unsigned char *buf)
{
	stream->BytePtr = buf;
	stream->BitPos = 0;
}

static unsigned int _Huffman_ReadBit(huff_bitstream_t *stream)
{
	unsigned int  x, bit;
	unsigned char *buf;

	/* Get current stream state */
	buf = stream->BytePtr;
	bit = stream->BitPos;

	/* Extract bit */
	x = (*buf & (1<<(7-bit))) ? 1 : 0;
	bit = (bit+1) & 7;
	if( !bit )
	{
	++ buf;
	}

	/* Store new stream state */
	stream->BitPos = bit;
	stream->BytePtr = buf;

	return x;
}

static unsigned int _Huffman_Read8Bits(huff_bitstream_t *stream)
{
	unsigned int  x, bit;
	unsigned char *buf;

	/* Get current stream state */
	buf = stream->BytePtr;
	bit = stream->BitPos;

	/* Extract byte */
	x = (*buf << bit) | (buf[1] >> (8-bit));
	++ buf;

	/* Store new stream state */
	stream->BytePtr = buf;

	return x;
}

static void _Huffman_WriteBits(huff_bitstream_t *stream, unsigned int x, unsigned int bits)
{
	unsigned int  bit, count;
	unsigned char *buf;
	unsigned int  mask;

	/* Get current stream state */
	buf = stream->BytePtr;
	bit = stream->BitPos;

	/* Append bits */
	mask = 1 << (bits-1);
	for( count = 0; count < bits; ++ count )
	{
	*buf = (*buf & (0xff^(1<<(7-bit)))) +
			((x & mask ? 1 : 0) << (7-bit));
	x <<= 1;
	bit = (bit+1) & 7;
	if( !bit )
	{
		++ buf;
	}
	}

	/* Store new stream state */
	stream->BytePtr = buf;
	stream->BitPos  = bit;
}

//_Huffman_Hist() - Calculate (sorted) histogram for a block of data.
static void _Huffman_Hist(unsigned char *in, huff_sym_t *sym, unsigned int size)
{
	int k;

	/* Clear/init histogram */
	for( k = 0; k < 256; ++ k )
	{
	sym[k].Symbol = k;
	sym[k].Count  = 0;
	sym[k].Code   = 0;
	sym[k].Bits   = 0;
	}

	/* Build histogram */
	for( k = size; k; -- k )
	{
	sym[*in ++].Count ++;
	}
}

//_Huffman_StoreTree() - Store a Huffman tree in the output stream and in a look-up-table (a symbol array).
static void _Huffman_StoreTree( huff_encodenode_t *node, huff_sym_t *sym, huff_bitstream_t *stream, unsigned int code, unsigned int bits )
{
	unsigned int sym_idx;

	/* Is this a leaf node? */
	if( node->Symbol >= 0 )
	{
	/* Append symbol to tree description */
	_Huffman_WriteBits( stream, 1, 1 );
	_Huffman_WriteBits( stream, node->Symbol, 8 );

	/* Find symbol index */
	for( sym_idx = 0; sym_idx < 256; ++ sym_idx )
	{
		if( sym[sym_idx].Symbol == node->Symbol ) break;
	}

	/* Store code info in symbol array */
	sym[sym_idx].Code = code;
	sym[sym_idx].Bits = bits;
	return;
	}
	else
	{
	/* This was not a leaf node */
	_Huffman_WriteBits( stream, 0, 1 );
	}

	/* Branch A */
	_Huffman_StoreTree( node->ChildA, sym, stream, (code<<1)+0, bits+1 );

	/* Branch B */
	_Huffman_StoreTree( node->ChildB, sym, stream, (code<<1)+1, bits+1 );
}

static void _Huffman_MakeTree( huff_sym_t *sym, huff_bitstream_t *stream )
{
	huff_encodenode_t nodes[MAX_TREE_NODES], *node_1, *node_2, *root;
	unsigned int k, num_symbols, nodes_left, next_idx;

	/* Initialize all leaf nodes */
	num_symbols = 0;
	for( k = 0; k < 256; ++ k )
	{
		if( sym[k].Count > 0 )
		{
			nodes[num_symbols].Symbol = sym[k].Symbol;
			nodes[num_symbols].Count = sym[k].Count;
			nodes[num_symbols].ChildA = (huff_encodenode_t *) 0;
			nodes[num_symbols].ChildB = (huff_encodenode_t *) 0;
			++ num_symbols;
		}
	}

	/* Build tree by joining the lightest nodes until there is only
		one node left (the root node). */
	root = (huff_encodenode_t *) 0;
	nodes_left = num_symbols;
	next_idx = num_symbols;
	while( nodes_left > 1 )
	{
	/* Find the two lightest nodes */
	node_1 = (huff_encodenode_t *) 0;
	node_2 = (huff_encodenode_t *) 0;
	for( k = 0; k < next_idx; ++ k )
	{
		if( nodes[k].Count > 0 )
		{
			if( !node_1 || (nodes[k].Count <= node_1->Count) )
			{
				node_2 = node_1;
				node_1 = &nodes[k];
			}
			else 
				if( !node_2 || (nodes[k].Count <= node_2->Count) )
				{
					node_2 = &nodes[k];
				}
		}
	}

	/* Join the two nodes into a new parent node */
	root = &nodes[next_idx];
	root->ChildA = node_1;
	root->ChildB = node_2;
	root->Count = node_1->Count + node_2->Count;
	root->Symbol = -1;
	node_1->Count = 0;
	node_2->Count = 0;
	++ next_idx;
	-- nodes_left;
	}

	/* Store the tree in the output stream, and in the sym[] array (the
		latter is used as a look-up-table for faster encoding) */
	if( root )
	{
		_Huffman_StoreTree( root, sym, stream, 0, 0 );
	}
	else
	{
		/* Special case: only one symbol => no binary tree */
		root = &nodes[0];
		_Huffman_StoreTree( root, sym, stream, 0, 1 );
	}
}

//_Huffman_RecoverTree() - Recover a Huffman tree from a bitstream.
static huff_decodenode_t * _Huffman_RecoverTree( huff_decodenode_t *nodes, huff_bitstream_t *stream, unsigned int *nodenum )
{
	huff_decodenode_t * this_node;

	/* Pick a node from the node array */
	this_node = &nodes[*nodenum];
	*nodenum = *nodenum + 1;

	/* Clear the node */
	this_node->Symbol = -1;
	this_node->ChildA = (huff_decodenode_t *) 0;
	this_node->ChildB = (huff_decodenode_t *) 0;

	/* Is this a leaf node? */
	if( _Huffman_ReadBit( stream ) )
	{
	/* Get symbol from tree description and store in lead node */
	this_node->Symbol = _Huffman_Read8Bits( stream );

	return this_node;
	}

	/* Get branch A */
	this_node->ChildA = _Huffman_RecoverTree( nodes, stream, nodenum );

	/* Get branch B */
	this_node->ChildB = _Huffman_RecoverTree( nodes, stream, nodenum );

	return this_node;
}

int Huffman_Compress(unsigned char *in, unsigned char *out, unsigned int insize)
{
  huff_sym_t       sym[256], tmp;
  huff_bitstream_t stream;
  unsigned int     k, total_bytes, swaps, symbol;

  /* Do we have anything to compress? */
  if( insize < 1 ) return 0;

  /* Initialize bitstream */
  _Huffman_InitBitstream( &stream, out );

  /* Calculate and sort histogram for input data */
  _Huffman_Hist( in, sym, insize );

  /* Build Huffman tree */
  _Huffman_MakeTree( sym, &stream );

  /* Sort histogram - first symbol first (bubble sort) */
  do
  {
    swaps = 0;
    for( k = 0; k < 255; ++ k )
    {
      if( sym[k].Symbol > sym[k+1].Symbol )
      {
        tmp      = sym[k];
        sym[k]   = sym[k+1];
        sym[k+1] = tmp;
        swaps    = 1;
      }
    }
  }
  while( swaps );

  /* Encode input stream */
  for( k = 0; k < insize; ++ k )
  {
    symbol = in[k];
    _Huffman_WriteBits( &stream, sym[symbol].Code,
                        sym[symbol].Bits );
  }

  /* Calculate size of output data */
  total_bytes = (int)(stream.BytePtr - out);
  if( stream.BitPos > 0 )
  {
    ++ total_bytes;
  }

  return total_bytes;
}

void Huffman_Uncompress( unsigned char *in, unsigned char *out, unsigned int insize, unsigned int outsize )
{
  huff_decodenode_t nodes[MAX_TREE_NODES], *root, *node;
  huff_bitstream_t  stream;
  unsigned int      k, node_count;
  unsigned char     *buf;

  /* Do we have anything to decompress? */
  if( insize < 1 ) return;

  /* Initialize bitstream */
  _Huffman_InitBitstream( &stream, in );

  /* Recover Huffman tree */
  node_count = 0;
  root = _Huffman_RecoverTree( nodes, &stream, &node_count );

  /* Decode input stream */
  buf = out;
  for( k = 0; k < outsize; ++ k )
  {
    /* Traverse tree until we find a matching leaf node */
    node = root;
    while( node->Symbol < 0 )
    {
      /* Get next node */
      if( _Huffman_ReadBit( &stream ) )
        node = node->ChildB;
      else
        node = node->ChildA;
    }

    /* We found the matching leaf node and have the symbol */
    *buf ++ = (unsigned char) node->Symbol;
  }
}

static unsigned int _LZ_StringCompare( unsigned char * str1, unsigned char * str2, unsigned int minlen, unsigned int maxlen )
{
    unsigned int len;

    for( len = minlen; (len < maxlen) && (str1[len] == str2[len]); ++ len );

    return len;
}

//_LZ_WriteVarSize() - Write unsigned integer with variable number of bytes depending on value.
static int _LZ_WriteVarSize( unsigned int x, unsigned char * buf )
{
    unsigned int y;
    int num_bytes, i, b;

    /* Determine number of bytes needed to store the number x */
    y = x >> 3;
    for( num_bytes = 5; num_bytes >= 2; -- num_bytes )
    {
        if( y & 0xfe000000 ) break;
        y <<= 7;
    }

    /* Write all bytes, seven bits in each, with 8:th bit set for all */
    /* but the last byte. */
    for( i = num_bytes-1; i >= 0; -- i )
    {
        b = (x >> (i*7)) & 0x0000007f;
        if( i > 0 )
        {
            b |= 0x00000080;
        }
        *buf ++ = (unsigned char) b;
    }

    /* Return number of bytes written */
    return num_bytes;
}

//_LZ_ReadVarSize() - Read unsigned integer with variable number of bytes depending on value.
static int _LZ_ReadVarSize( unsigned int * x, unsigned char * buf )
{
    unsigned int y, b, num_bytes;

    /* Read complete value (stop when byte contains zero in 8:th bit) */
    y = 0;
    num_bytes = 0;
    do
    {
        b = (unsigned int) (*buf ++);
        y = (y << 7) | (b & 0x0000007f);
        ++ num_bytes;
    }
    while( b & 0x00000080 );

    /* Store value in x */
    *x = y;

    /* Return number of bytes read */
    return num_bytes;
}

int LZ_Compress( unsigned char *in, unsigned char *out, unsigned int insize )
{
    unsigned char marker, symbol;
    unsigned int  inpos, outpos, bytesleft, i;
    unsigned int  maxoffset, offset, bestoffset;
    unsigned int  maxlength, length, bestlength;
    unsigned int  histogram[ 256 ];
    unsigned char *ptr1, *ptr2;

    /* Do we have anything to compress? */
    if( insize < 1 )
    {
        return 0;
    }

    /* Create histogram */
    for( i = 0; i < 256; ++ i )
    {
        histogram[ i ] = 0;
    }
    for( i = 0; i < insize; ++ i )
    {
        ++ histogram[ in[ i ] ];
    }

    /* Find the least common byte, and use it as the marker symbol */
    marker = 0;
    for( i = 1; i < 256; ++ i )
    {
        if( histogram[ i ] < histogram[ marker ] )
        {
            marker = i;
        }
    }

    /* Remember the marker symbol for the decoder */
    out[ 0 ] = marker;

    /* Start of compression */
    inpos = 0;
    outpos = 1;

    /* Main compression loop */
    bytesleft = insize;
    do
    {
        /* Determine most distant position */
        if( inpos > LZ_MAX_OFFSET ) maxoffset = LZ_MAX_OFFSET;
        else                        maxoffset = inpos;

        /* Get pointer to current position */
        ptr1 = &in[ inpos ];

        /* Search history window for maximum length string match */
        bestlength = 3;
        bestoffset = 0;
        for( offset = 3; offset <= maxoffset; ++ offset )
        {
            /* Get pointer to candidate string */
            ptr2 = &ptr1[ -(int)offset ];

            /* Quickly determine if this is a candidate (for speed) */
            if( (ptr1[ 0 ] == ptr2[ 0 ]) &&
                (ptr1[ bestlength ] == ptr2[ bestlength ]) )
            {
                /* Determine maximum length for this offset */
                maxlength = (bytesleft < offset ? bytesleft : offset);

                /* Count maximum length match at this offset */
                length = _LZ_StringCompare( ptr1, ptr2, 0, maxlength );

                /* Better match than any previous match? */
                if( length > bestlength )
                {
                    bestlength = length;
                    bestoffset = offset;
                }
            }
        }

        /* Was there a good enough match? */
        if( (bestlength >= 8) ||
            ((bestlength == 4) && (bestoffset <= 0x0000007f)) ||
            ((bestlength == 5) && (bestoffset <= 0x00003fff)) ||
            ((bestlength == 6) && (bestoffset <= 0x001fffff)) ||
            ((bestlength == 7) && (bestoffset <= 0x0fffffff)) )
        {
            out[ outpos ++ ] = (unsigned char) marker;
            outpos += _LZ_WriteVarSize( bestlength, &out[ outpos ] );
            outpos += _LZ_WriteVarSize( bestoffset, &out[ outpos ] );
            inpos += bestlength;
            bytesleft -= bestlength;
        }
        else
        {
            /* Output single byte (or two bytes if marker byte) */
            symbol = in[ inpos ++ ];
            out[ outpos ++ ] = symbol;
            if( symbol == marker )
            {
                out[ outpos ++ ] = 0;
            }
            -- bytesleft;
        }
    }
    while( bytesleft > 3 );

    /* Dump remaining bytes, if any */
    while( inpos < insize )
    {
        if( in[ inpos ] == marker )
        {
            out[ outpos ++ ] = marker;
            out[ outpos ++ ] = 0;
        }
        else
        {
            out[ outpos ++ ] = in[ inpos ];
        }
        ++ inpos;
    }

    return outpos;
}

int LZ_CompressFast( unsigned char *in, unsigned char *out, unsigned int insize, unsigned int *work )
{
    unsigned char marker, symbol;
    unsigned int  inpos, outpos, bytesleft, i, index, symbols;
    unsigned int  offset, bestoffset;
    unsigned int  maxlength, length, bestlength;
    unsigned int  histogram[ 256 ], *lastindex, *jumptable;
    unsigned char *ptr1, *ptr2;

    /* Do we have anything to compress? */
    if( insize < 1 )
    {
        return 0;
    }

    /* Assign arrays to the working area */
    lastindex = work;
    jumptable = &work[ 65536 ];

    /* Build a "jump table". Here is how the jump table works:
       jumptable[i] points to the nearest previous occurrence of the same
       symbol pair as in[i]:in[i+1], so in[i] == in[jumptable[i]] and
       in[i+1] == in[jumptable[i]+1], and so on... Following the jump table
       gives a dramatic boost for the string search'n'match loop compared
       to doing a brute force search. The jump table is built in O(n) time,
       so it is a cheap operation in terms of time, but it is expensice in
       terms of memory consumption. */
    for( i = 0; i < 65536; ++ i )
    {
        lastindex[ i ] = 0xffffffff;
    }
    for( i = 0; i < insize-1; ++ i )
    {
        symbols = (((unsigned int)in[i]) << 8) | ((unsigned int)in[i+1]);
        index = lastindex[ symbols ];
        lastindex[ symbols ] = i;
        jumptable[ i ] = index;
    }
    jumptable[ insize-1 ] = 0xffffffff;

    /* Create histogram */
    for( i = 0; i < 256; ++ i )
    {
        histogram[ i ] = 0;
    }
    for( i = 0; i < insize; ++ i )
    {
        ++ histogram[ in[ i ] ];
    }

    /* Find the least common byte, and use it as the marker symbol */
    marker = 0;
    for( i = 1; i < 256; ++ i )
    {
        if( histogram[ i ] < histogram[ marker ] )
        {
            marker = i;
        }
    }

    /* Remember the marker symbol for the decoder */
    out[ 0 ] = marker;

    /* Start of compression */
    inpos = 0;
    outpos = 1;

    /* Main compression loop */
    bytesleft = insize;
    do
    {
        /* Get pointer to current position */
        ptr1 = &in[ inpos ];

        /* Search history window for maximum length string match */
        bestlength = 3;
        bestoffset = 0;
        index = jumptable[ inpos ];
        while( (index != 0xffffffff) && ((inpos - index) < LZ_MAX_OFFSET) )
        {
            /* Get pointer to candidate string */
            ptr2 = &in[ index ];

            /* Quickly determine if this is a candidate (for speed) */
            if( ptr2[ bestlength ] == ptr1[ bestlength ] )
            {
                /* Determine maximum length for this offset */
                offset = inpos - index;
                maxlength = (bytesleft < offset ? bytesleft : offset);

                /* Count maximum length match at this offset */
                length = _LZ_StringCompare( ptr1, ptr2, 2, maxlength );

                /* Better match than any previous match? */
                if( length > bestlength )
                {
                    bestlength = length;
                    bestoffset = offset;
                }
            }

            /* Get next possible index from jump table */
            index = jumptable[ index ];
        }

        /* Was there a good enough match? */
        if( (bestlength >= 8) ||
            ((bestlength == 4) && (bestoffset <= 0x0000007f)) ||
            ((bestlength == 5) && (bestoffset <= 0x00003fff)) ||
            ((bestlength == 6) && (bestoffset <= 0x001fffff)) ||
            ((bestlength == 7) && (bestoffset <= 0x0fffffff)) )
        {
            out[ outpos ++ ] = (unsigned char) marker;
            outpos += _LZ_WriteVarSize( bestlength, &out[ outpos ] );
            outpos += _LZ_WriteVarSize( bestoffset, &out[ outpos ] );
            inpos += bestlength;
            bytesleft -= bestlength;
        }
        else
        {
            /* Output single byte (or two bytes if marker byte) */
            symbol = in[ inpos ++ ];
            out[ outpos ++ ] = symbol;
            if( symbol == marker )
            {
                out[ outpos ++ ] = 0;
            }
            -- bytesleft;
        }
    }
    while( bytesleft > 3 );

    /* Dump remaining bytes, if any */
    while( inpos < insize )
    {
        if( in[ inpos ] == marker )
        {
            out[ outpos ++ ] = marker;
            out[ outpos ++ ] = 0;
        }
        else
        {
            out[ outpos ++ ] = in[ inpos ];
        }
        ++ inpos;
    }

    return outpos;
}

void LZ_Uncompress( unsigned char *in, unsigned char *out,
    unsigned int insize )
{
    unsigned char marker, symbol;
    unsigned int  i, inpos, outpos, length, offset;

    /* Do we have anything to uncompress? */
    if( insize < 1 )
    {
        return;
    }

    /* Get marker symbol from input stream */
    marker = in[ 0 ];
    inpos = 1;

    /* Main decompression loop */
    outpos = 0;
    do
    {
        symbol = in[ inpos ++ ];
        if( symbol == marker )
        {
            /* We had a marker byte */
            if( in[ inpos ] == 0 )
            {
                /* It was a single occurrence of the marker byte */
                out[ outpos ++ ] = marker;
                ++ inpos;
            }
            else
            {
                /* Extract true length and offset */
                inpos += _LZ_ReadVarSize( &length, &in[ inpos ] );
                inpos += _LZ_ReadVarSize( &offset, &in[ inpos ] );

                /* Copy corresponding data from history window */
                for( i = 0; i < length; ++ i )
                {
                    out[ outpos ] = out[ outpos - offset ];
                    ++ outpos;
                }
            }
        }
        else
        {
            /* No marker, plain copy */
            out[ outpos ++ ] = symbol;
        }
    }
    while( inpos < insize );
}

//_Rice_NumBits() - Determine number of information bits in a word.
static int _Rice_NumBits( unsigned int x )
{
    int n;
    for( n = 32; !(x & 0x80000000) && (n > 0); -- n ) x <<= 1;
    return n;
}

static void _Rice_InitBitstream( rice_bitstream_t *stream, void *buf, unsigned int bytes )
{
    stream->BytePtr  = (unsigned char *) buf;
    stream->BitPos   = 0;
    stream->NumBytes = bytes;
}

static int _Rice_ReadBit( rice_bitstream_t *stream )
{
    unsigned int x, bit, idx;

    idx = stream->BitPos >> 3;
    if( idx < stream->NumBytes )
    {
        bit = 7 - (stream->BitPos & 7);
        x = (stream->BytePtr[ idx ] >> bit) & 1;
        ++ stream->BitPos;
    }
    else
    {
        x = 0;
    }
    return x;
}

static void _Rice_WriteBit( rice_bitstream_t *stream, int x )
{
    unsigned int bit, idx, mask, set;

    idx = stream->BitPos >> 3;
    if( idx < stream->NumBytes )
    {
        bit  = 7 - (stream->BitPos & 7);
        mask = 0xff ^ (1 << bit);
        set  = (x & 1) << bit;
        stream->BytePtr[ idx ] = (stream->BytePtr[ idx ] & mask) | set;
        ++ stream->BitPos;
    }
}

static void _Rice_EncodeWord( unsigned int x, int k, rice_bitstream_t *stream )
{
    unsigned int q, i;
    int          j, o;

    /* Determine overflow */
    q = x >> k;

    /* Too large rice code? */
    if( q > RICE_THRESHOLD )
    {
        /* Write Rice code (except for the final zero) */
        for( j = 0; j < RICE_THRESHOLD; ++ j )
        {
            _Rice_WriteBit( stream, 1 );
        }

        /* Encode the overflow with alternate coding */
        q -= RICE_THRESHOLD;

        /* Write number of bits needed to represent the overflow */
        o = _Rice_NumBits( q );
        for( j = 0; j < o; ++ j )
        {
            _Rice_WriteBit( stream, 1 );
        }
        _Rice_WriteBit( stream, 0 );

        /* Write the o-1 least significant bits of q "as is" */
        for( j = o-2; j >= 0; -- j )
        {
            _Rice_WriteBit( stream, (q >> j) & 1 );
        }
    }
    else
    {
        /* Write Rice code */
        for( i = 0; i < q; ++ i )
        {
            _Rice_WriteBit( stream, 1 );
        }
        _Rice_WriteBit( stream, 0 );
    }

    /* Encode the rest of the k bits */
    for( j = k-1; j >= 0; -- j )
    {
        _Rice_WriteBit( stream, (x >> j) & 1 );
    }
}

static unsigned int _Rice_DecodeWord( int k, rice_bitstream_t *stream )
{
    unsigned int x, q;
    int          i, o;

    /* Decode Rice code */
    q = 0;
    while( _Rice_ReadBit( stream ) )
    {
        ++ q;
    }

    /* Too large Rice code? */
    if( q > RICE_THRESHOLD )
    {
        /* Bits needed for the overflow part... */
        o = q - RICE_THRESHOLD;

        /* Read additional bits (MSB is always 1) */
        x = 1;
        for( i = 0; i < o-1; ++ i )
        {
            x = (x<<1) | _Rice_ReadBit( stream );
        }

        /* Add Rice code */
        x += RICE_THRESHOLD;
    }
    else
    {
        x = q;
    }

    /* Decode the rest of the k bits */
    for( i = k-1; i >= 0; -- i )
    {
        x = (x<<1) | _Rice_ReadBit( stream );
    }

    return x;
}

//_Rice_ReadWord() - Read a word from the input stream, and convert it to a signed magnitude 32-bit representation (regardless of input format).
static unsigned int _Rice_ReadWord( void *ptr, unsigned int idx, int format )
{
    int            sx;
    unsigned int   x;

    /* Read a word with the appropriate format from the stream */
    switch( format )
    {
        case RICE_FMT_INT8:
            sx = (int)((signed char *) ptr)[ idx ];
            x = sx < 0 ? -1-(sx<<1) : sx<<1;
            break;
        case RICE_FMT_UINT8:
            x = (unsigned int)((unsigned char *) ptr)[ idx ];
            break;

        case RICE_FMT_INT16:
            sx = (int)((signed short *) ptr)[ idx ];
            x = sx < 0 ? -1-(sx<<1) : sx<<1;
            break;
        case RICE_FMT_UINT16:
            x = (unsigned int)((unsigned short *) ptr)[ idx ];
            break;

        case RICE_FMT_INT32:
            sx = ((int *) ptr)[ idx ];
            x = sx < 0 ? -1-(sx<<1) : sx<<1;
            break;
        case RICE_FMT_UINT32:
            x = ((unsigned int *) ptr)[ idx ];
            break;

        default:
            x = 0;
    }

    return x;
}


//_Rice_WriteWord() - Convert a signed magnitude 32-bit word to the given format, and write it to the otuput stream.
static void _Rice_WriteWord( void *ptr, unsigned int idx, int format, unsigned int x )
{
    int sx;

    /* Write a word with the appropriate format to the stream */
    switch( format )
    {
        case RICE_FMT_INT8:
            sx = (x & 1) ? -(int)((x+1)>>1) : (int)(x>>1);
            ((signed char *) ptr)[ idx ] = sx;
            break;
        case RICE_FMT_UINT8:
            ((unsigned char *) ptr)[ idx ] = x;
            break;

        case RICE_FMT_INT16:
            sx = (x & 1) ? -(int)((x+1)>>1) : (int)(x>>1);
            ((signed short *) ptr)[ idx ] = sx;
            break;
        case RICE_FMT_UINT16:
            ((unsigned short *) ptr)[ idx ] = x;
            break;

        case RICE_FMT_INT32:
            sx = (x & 1) ? -(int)((x+1)>>1) : (int)(x>>1);
            ((int *) ptr)[ idx ] = sx;
            break;
        case RICE_FMT_UINT32:
            ((unsigned int *) ptr)[ idx ] = x;
            break;
    }
}

int Rice_Compress( void *in, void *out, unsigned int insize, int format )
{
    rice_bitstream_t stream;
    unsigned int     i, x, k, n, wordsize, incount;
    unsigned int     hist[ RICE_HISTORY ];
    int              j;

    /* Calculate number of input words */
    switch( format )
    {
        case RICE_FMT_INT8:
        case RICE_FMT_UINT8:  wordsize = 8; break;
        case RICE_FMT_INT16:
        case RICE_FMT_UINT16: wordsize = 16; break;
        case RICE_FMT_INT32:
        case RICE_FMT_UINT32: wordsize = 32; break;
        default: return 0;
    }
    incount = insize / (wordsize>>3);

    /* Do we have anything to compress? */
    if( incount == 0 )
    {
        return 0;
    }

    /* Initialize output bitsream */
    _Rice_InitBitstream( &stream, out, insize+1 );

    /* Determine a good initial k */
    k = 0;
    for( i = 0; (i < RICE_HISTORY) && (i < incount); ++ i )
    {
        n = _Rice_NumBits( _Rice_ReadWord( in, i, format ) );
        k += n;
    }
    k = (k + (i>>1)) / i;
    if( k == 0 ) k = 1;

    /* Write k to the output stream (the decoder needs it) */
    ((unsigned char *) out)[0] = k;
    stream.BitPos = 8;

    /* Encode input stream */
    for( i = 0; (i < incount) && ((stream.BitPos>>3) <= insize); ++ i )
    {
        /* Revise optimum k? */
        if( i >= RICE_HISTORY )
        {
            k = 0;
            for( j = 0; j < RICE_HISTORY; ++ j )
            {
                k += hist[ j ];
            }
            k = (k + (RICE_HISTORY>>1)) / RICE_HISTORY;
        }

        /* Read word from input buffer */
        x = _Rice_ReadWord( in, i, format );

        /* Encode word to output buffer */
        _Rice_EncodeWord( x, k, &stream );

        /* Update history */
        hist[ i % RICE_HISTORY ] = _Rice_NumBits( x );
    }

    /* Was there a buffer overflow? */
    if( i < incount )
    {
        /* Indicate that the buffer was not compressed */
        ((unsigned char *) out)[0] = 0;

        /* Rewind bitstream and fill it with raw words */
        stream.BitPos = 8;
        for( i = 0; i < incount; ++ i )
        {
            x = _Rice_ReadWord( in, i, format );
            for( j = wordsize-1; j >= 0; -- j )
            {
                _Rice_WriteBit( &stream, (x >> j) & 1 );
            }
        }
    }

    return (stream.BitPos+7) >> 3;
}

void Rice_Uncompress( void *in, void *out, unsigned int insize, unsigned int outsize, int format )
{
    rice_bitstream_t stream;
    unsigned int     i, x, k, wordsize, outcount;
    unsigned int     hist[ RICE_HISTORY ];
    int              j;

    /* Calculate number of output words */
    switch( format )
    {
        case RICE_FMT_INT8:
        case RICE_FMT_UINT8:  wordsize = 8; break;
        case RICE_FMT_INT16:
        case RICE_FMT_UINT16: wordsize = 16; break;
        case RICE_FMT_INT32:
        case RICE_FMT_UINT32: wordsize = 32; break;
        default: return;
    }
    outcount = outsize / (wordsize>>3);

    /* Do we have anything to decompress? */
    if( outcount == 0 )
    {
        return;
    }

    /* Initialize input bitsream */
    _Rice_InitBitstream( &stream, in, insize );

    /* Get initial k */
    k = ((unsigned char *) in)[0];
    stream.BitPos = 8;

    /* Was the buffer not compressed */
    if( k == 0 )
    {
        /* Copy raw words from input stream */
        for( i = 0; i < outcount; ++ i )
        {
            x = 0;
            for( j = wordsize-1; j >= 0; -- j )
            {
                x = (x<<1) | _Rice_ReadBit( &stream );
            }
            _Rice_WriteWord( out, i, format, x );
        }
    }
    else
    {
        /* Decode input stream */
        for( i = 0; i < outcount; ++ i )
        {
            /* Revise optimum k? */
            if( i >= RICE_HISTORY )
            {
                k = 0;
                for( j = 0; j < RICE_HISTORY; ++ j )
                {
                    k += hist[ j ];
                }
                k = (k + (RICE_HISTORY>>1)) / RICE_HISTORY;
            }

            /* Decode word from input buffer */
            x = _Rice_DecodeWord( k, &stream );

            /* Write word to output buffer */
            _Rice_WriteWord( out, i, format, x );

            /* Update history */
            hist[ i % RICE_HISTORY ] = _Rice_NumBits( x );
        }
    }
}

//_RLE_WriteRep() - Encode a repetition of 'symbol' repeated 'count' times.
static void _RLE_WriteRep( unsigned char *out, unsigned int *outpos, unsigned char marker, unsigned char symbol, unsigned int count )
{
    unsigned int i, idx;

    idx = *outpos;
    if( count <= 3 )
    {
        if( symbol == marker )
        {
            out[ idx ++ ] = marker;
            out[ idx ++ ] = count-1;
        }
        else
        {
            for( i = 0; i < count; ++ i )
            {
                out[ idx ++ ] = symbol;
            }
        }
    }
    else
    {
        out[ idx ++ ] = marker;
        -- count;
        if( count >= 128 )
        {
            out[ idx ++ ] = (count >> 8) | 0x80;
        }
        out[ idx ++ ] = count & 0xff;
        out[ idx ++ ] = symbol;
    }
    *outpos = idx;
}

/*_RLE_WriteNonRep() - Encode a non-repeating symbol, 'symbol'. 'marker' is the marker symbol,
and special care has to be taken for the case when 'symbol' == 'marker'.*/
static void _RLE_WriteNonRep( unsigned char *out, unsigned int *outpos, unsigned char marker, unsigned char symbol )
{
    unsigned int idx;

    idx = *outpos;
    if( symbol == marker )
    {
        out[ idx ++ ] = marker;
        out[ idx ++ ] = 0;
    }
    else
    {
        out[ idx ++ ] = symbol;
    }
    *outpos = idx;
}

int RLE_Compress( unsigned char *in, unsigned char *out, unsigned int insize )
{
    unsigned char byte1, byte2, marker;
    unsigned int  inpos, outpos, count, i, histogram[ 256 ];

    /* Do we have anything to compress? */
    if( insize < 1 )
    {
        return 0;
    }

    /* Create histogram */
    for( i = 0; i < 256; ++ i )
    {
        histogram[ i ] = 0;
    }
    for( i = 0; i < insize; ++ i )
    {
        ++ histogram[ in[ i ] ];
    }

    /* Find the least common byte, and use it as the repetition marker */
    marker = 0;
    for( i = 1; i < 256; ++ i )
    {
        if( histogram[ i ] < histogram[ marker ] )
        {
            marker = i;
        }
    }

    /* Remember the repetition marker for the decoder */
    out[ 0 ] = marker;
    outpos = 1;

    /* Start of compression */
    byte1 = in[ 0 ];
    inpos = 1;
    count = 1;

    /* Are there at least two bytes? */
    if( insize >= 2 )
    {
        byte2 = in[ inpos ++ ];
        count = 2;

        /* Main compression loop */
        do
        {
            if( byte1 == byte2 )
            {
                /* Do we meet only a sequence of identical bytes? */
                while( (inpos < insize) && (byte1 == byte2) &&
                       (count < 32768) )
                {
                    byte2 = in[ inpos ++ ];
                    ++ count;
                }
                if( byte1 == byte2 )
                {
                    _RLE_WriteRep( out, &outpos, marker, byte1, count );
                    if( inpos < insize )
                    {
                        byte1 = in[ inpos ++ ];
                        count = 1;
                    }
                    else
                    {
                        count = 0;
                    }
                }
                else
                {
                    _RLE_WriteRep( out, &outpos, marker, byte1, count-1 );
                    byte1 = byte2;
                    count = 1;
                }
            }
            else
            {
                /* No, then don't handle the last byte */
                _RLE_WriteNonRep( out, &outpos, marker, byte1 );
                byte1 = byte2;
                count = 1;
            }
            if( inpos < insize )
            {
                byte2 = in[ inpos ++ ];
                count = 2;
            }
        }
        while( (inpos < insize) || (count >= 2) );
    }

    /* One byte left? */
    if( count == 1 )
    {
        _RLE_WriteNonRep( out, &outpos, marker, byte1 );
    }

    return outpos;
}

void RLE_Uncompress( unsigned char *in, unsigned char *out, unsigned int insize )
{
    unsigned char marker, symbol;
    unsigned int  i, inpos, outpos, count;

    /* Do we have anything to uncompress? */
    if( insize < 1 )
    {
        return;
    }

    /* Get marker symbol from input stream */
    inpos = 0;
    marker = in[ inpos ++ ];

    /* Main decompression loop */
    outpos = 0;
    do
    {
        symbol = in[ inpos ++ ];
        if( symbol == marker )
        {
            /* We had a marker byte */
            count = in[ inpos ++ ];
            if( count <= 2 )
            {
                /* Counts 0, 1 and 2 are used for marker byte repetition
                   only */
                for( i = 0; i <= count; ++ i )
                {
                    out[ outpos ++ ] = marker;
                }
            }
            else
            {
                if( count & 0x80 )
                {
                    count = ((count & 0x7f) << 8) + in[ inpos ++ ];
                }
                symbol = in[ inpos ++ ];
                for( i = 0; i <= count; ++ i )
                {
                    out[ outpos ++ ] = symbol;
                }
            }
        }
        else
        {
            /* No marker, plain copy */
            out[ outpos ++ ] = symbol;
        }
    }
    while( inpos < insize );
}

static void _SF_InitBitstream( sf_bitstream_t *stream, unsigned char *buf )
{
    stream->BytePtr  = buf;
    stream->BitPos   = 0;
}

static unsigned int _SF_ReadBit( sf_bitstream_t *stream )
{
    unsigned int  x, bit;
    unsigned char *buf;

    /* Get current stream state */
    buf = stream->BytePtr;
    bit = stream->BitPos;

    /* Extract bit */
    x = (*buf & (1<<(7-bit))) ? 1 : 0;
    bit = (bit+1) & 7;
    if( !bit )
    {
        ++ buf;
    }

    /* Store new stream state */
    stream->BitPos = bit;
    stream->BytePtr = buf;

    return x;
}

static unsigned int _SF_Read8Bits( sf_bitstream_t *stream )
{
    unsigned int  x, bit;
    unsigned char *buf;

    /* Get current stream state */
    buf = stream->BytePtr;
    bit = stream->BitPos;

    /* Extract byte */
    x = (*buf << bit) | (buf[1] >> (8-bit));
    ++ buf;

    /* Store new stream state */
    stream->BytePtr = buf;

    return x;
}

static void _SF_WriteBits( sf_bitstream_t *stream, unsigned int x, unsigned int bits )
{
    unsigned int  bit, count;
    unsigned char *buf;
    unsigned int  mask;

    /* Get current stream state */
    buf = stream->BytePtr;
    bit = stream->BitPos;

    /* Append bits */
    mask = 1 << (bits-1);
    for( count = 0; count < bits; ++ count )
    {
        *buf = (*buf & (0xff^(1<<(7-bit)))) +
               ((x & mask ? 1 : 0) << (7-bit));
        x <<= 1;
        bit = (bit+1) & 7;
        if( !bit )
        {
            ++ buf;
        }
    }

    /* Store new stream state */
    stream->BytePtr = buf;
    stream->BitPos  = bit;
}

//_SF_Hist() - Calculate (sorted) histogram for a block of data.
static void _SF_Hist( unsigned char *in, sf_sym_t *sym, unsigned int size )
{
    int k, swaps;
    sf_sym_t tmp;

    /* Clear/init histogram */
    for( k = 0; k < 256; ++ k )
    {
        sym[k].Symbol = k;
        sym[k].Count  = 0;
        sym[k].Code   = 0;
        sym[k].Bits   = 0;
    }

    /* Build histogram */
    for( k = size; k; -- k )
    {
        sym[*in ++].Count ++;
    }

    /* Sort histogram - most frequent symbol first (bubble sort) */
    do
    {
        swaps = 0;
        for( k = 0; k < 255; ++ k )
        {
            if( sym[k].Count < sym[k+1].Count )
            {
                tmp      = sym[k];
                sym[k]   = sym[k+1];
                sym[k+1] = tmp;
                swaps    = 1;
            }
        }
    }
    while( swaps );
}

static void _SF_MakeTree( sf_sym_t *sym, sf_bitstream_t *stream, unsigned int code, unsigned int bits, unsigned int first, unsigned int last )
{
    unsigned int k, size, size_a, size_b, last_a, first_b;

    /* Is this a leaf node? */
    if( first == last )
    {
        /* Append symbol to tree description */
        _SF_WriteBits( stream, 1, 1 );
        _SF_WriteBits( stream, sym[first].Symbol, 8 );

        /* Store code info in symbol array */
        sym[first].Code = code;
        sym[first].Bits = bits;
        return;
    }
    else
    {
        /* This was not a leaf node */
        _SF_WriteBits( stream, 0, 1 );
    }

    /* Total size of interval */
    size = 0;
    for( k = first; k <= last; ++ k )
    {
        size += sym[k].Count;
    }

    /* Find size of branch a */
    size_a = 0;
    for( k = first; size_a < ((size+1)>>1) && k < last; ++ k )
    {
        size_a += sym[k].Count;
    }

    /* Non-empty branch? */
    if( size_a > 0 )
    {
        /* Continue branching */
        _SF_WriteBits( stream, 1, 1 );

        /* Branch a cut in histogram */
        last_a  = k-1;

        /* Create branch a */
        _SF_MakeTree( sym, stream, (code<<1)+0, bits+1,
                               first, last_a );
    }
    else
    {
        /* This was an empty branch */
        _SF_WriteBits( stream, 0, 1 );
    }

    /* Size of branch b */
    size_b = size - size_a;

    /* Non-empty branch? */
    if( size_b > 0 )
    {
        /* Continue branching */
        _SF_WriteBits( stream, 1, 1 );

        /* Branch b cut in histogram */
        first_b = k;

        /* Create branch b */
        _SF_MakeTree( sym, stream, (code<<1)+1, bits+1,
                               first_b, last );
    }
    else
    {
        /* This was an empty branch */
        _SF_WriteBits( stream, 0, 1 );
    }
}

//_SF_RecoverTree() - Recover a Shannon-Fano tree from a bitstream.
static sf_treenode_t * _SF_RecoverTree( sf_treenode_t *nodes,sf_bitstream_t *stream, unsigned int *nodenum )
{
    sf_treenode_t * this_node;

    /* Pick a node from the node array */
    this_node = &nodes[*nodenum];
    *nodenum = *nodenum + 1;

    /* Clear the node */
    this_node->Symbol = -1;
    this_node->ChildA = (sf_treenode_t *) 0;
    this_node->ChildB = (sf_treenode_t *) 0;

    /* Is this a leaf node? */
    if( _SF_ReadBit( stream ) )
    {
        /* Get symbol from tree description and store in lead node */
        this_node->Symbol = _SF_Read8Bits( stream );

        return this_node;
    }

    /* Non-empty branch A? */
    if( _SF_ReadBit( stream ) )
    {
        /* Create branch A */
        this_node->ChildA = _SF_RecoverTree( nodes, stream, nodenum );
    }

    /* Non-empty branch B? */
    if( _SF_ReadBit( stream ) )
    {
        /* Create branch B */
        this_node->ChildB = _SF_RecoverTree( nodes, stream, nodenum );
    }

    return this_node;
}

int SF_Compress( unsigned char *in, unsigned char *out,unsigned int insize )
{
    sf_sym_t       sym[256], tmp;
    sf_bitstream_t stream;
    unsigned int     k, total_bytes, swaps, symbol, last_symbol;

    /* Do we have anything to compress? */
    if( insize < 1 ) return 0;

    /* Initialize bitstream */
    _SF_InitBitstream( &stream, out );

    /* Calculate and sort histogram for input data */
    _SF_Hist( in, sym, insize );

    /* Find number of used symbols */
    for( last_symbol = 255; sym[last_symbol].Count == 0; -- last_symbol );

    /* Special case: In order to build a correct tree, we need at least
       two symbols (otherwise we get zero-bit representations). */
    if( last_symbol == 0 ) ++ last_symbol;

    /* Build Shannon-Fano tree */
    _SF_MakeTree( sym, &stream, 0, 0, 0, last_symbol );

    /* Sort histogram - first symbol first (bubble sort) */
    do
    {
        swaps = 0;
        for( k = 0; k < 255; ++ k )
        {
            if( sym[k].Symbol > sym[k+1].Symbol )
            {
                tmp      = sym[k];
                sym[k]   = sym[k+1];
                sym[k+1] = tmp;
                swaps    = 1;
            }
        }
    }
    while( swaps );

    /* Encode input stream */
    for( k = 0; k < insize; ++ k )
    {
        symbol = in[k];
        _SF_WriteBits( &stream, sym[symbol].Code,
                            sym[symbol].Bits );
    }

    /* Calculate size of output data */
    total_bytes = (int)(stream.BytePtr - out);
    if( stream.BitPos > 0 )
    {
        ++ total_bytes;
    }

    return total_bytes;
}

void SF_Uncompress( unsigned char *in, unsigned char *out,unsigned int insize, unsigned int outsize )
{
    sf_treenode_t  nodes[MAX_TREE_NODES], *root, *node;
    sf_bitstream_t stream;
    unsigned int     k, node_count;
    unsigned char    *buf;

    /* Do we have anything to decompress? */
    if( insize < 1 ) return;

    /* Initialize bitstream */
    _SF_InitBitstream( &stream, in );

    /* Recover Shannon-Fano tree */
    node_count = 0;
    root = _SF_RecoverTree( nodes, &stream, &node_count );

    /* Decode input stream */
    buf = out;
    for( k = 0; k < outsize; ++ k )
    {
        /* Traverse tree until we find a matching leaf node */
        node = root;
        while( node->Symbol < 0 )
        {
          /* Get next node */
          if( _SF_ReadBit( &stream ) )
            node = node->ChildB;
          else
            node = node->ChildA;
        }

        /* We found the matching leaf node and have the symbol */
        *buf ++ = (unsigned char) node->Symbol;
    }
}

int ReadWord32(FILE *f)
{
	unsigned char buf[4];
	fread( buf, 4, 1, f );
	return (((unsigned int)buf[0])<<24) + (((unsigned int)buf[1])<<16) +
		(((unsigned int)buf[2])<<8) + (unsigned int)buf[3];
}

void WriteWord32(int x, FILE *f)
{
	fputc((x>>24)&255,f);
	fputc((x>>16)&255,f);
	fputc((x>>8)&255,f);
	fputc(x&255,f);
}

long GetFileSize(FILE *f)
{
    long pos, size;

    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, pos, SEEK_SET);

    return size;
}

void Help(char *prgname)
{
    printf("Usage: %s command [algo] infile outfile\n\n", prgname);
    printf("Commands:\n");
    printf("  c       Compress\n");
    printf("  d       Deompress\n\n");
    printf("Algo (only specify for compression):\n");
    printf("  rle     RLE Compression\n");
    printf("  sf      Shannon-Fano compression\n");
    printf("  huff    Huffman compression\n");
    printf("  lz      LZ77 Compression\n");
    printf("  rice8   Rice compresison of 8-bit data\n");
    printf("  rice16  Rice compresison of 16-bit data\n");
    printf("  rice32  Rice compresison of 32-bit data\n");
    printf("  rice8s  Rice compresison of 8-bit signed data\n");
    printf("  rice16s Rice compresison of 16-bit signed data\n");
    printf("  rice32s Rice compresison of 32-bit signed data\n");
}

int BasicFileAlgs::BasicFile(int argsc, char *args[])
{
    FILE *f;
    unsigned char *in, *out, command, algo=0;
    unsigned int  insize, outsize=0, *work;
    char *inname, *outname;

    /* Check arguments */
    if(argsc < 4)
    {
        Help(args[0]);
        return 0;
    }

    /* Get command */
    command = args[1][0];
    if((command != 'c') && (command != 'd'))
    {
        Help(args[0]);
        return 0;
    }

    /* Get algo */
    if(argsc == 5 && command == 'c')
    {
        algo = 0;
        if(strcmp(args[2], "rle")== 0)     
			algo = 1;
        if(strcmp(args[2], "huff")== 0)    
			algo = 2;
        if(strcmp(args[2], "rice8")== 0)   
			algo = 3;
        if(strcmp(args[2], "rice16")== 0)  
			algo = 4;
        if(strcmp(args[2], "rice32")== 0)  
			algo = 5;
        if(strcmp(args[2], "rice8s")== 0)  
			algo = 6;
        if(strcmp(args[2], "rice16s")== 0) 
			algo = 7;
        if(strcmp(args[2], "rice32s")== 0) 
			algo = 8;
        if(strcmp(args[2], "lz")== 0)      
			algo = 9;
        if(strcmp(args[2], "sf")== 0)      
			algo = 10;
        if(!algo)
        {
            Help(args[0]);
            return 0;
        }
        inname = args[3];
        outname = args[4];
    }
    else 
		if(argsc == 4 && command == 'd')
		{
			inname = args[2];
			outname = args[3];
		}
		else
		{
			Help(args[0]);
			return 0;
		}

    /* Open input file */
    f = fopen(inname, "rb");
    if(!f)
    {
        printf( "Unable to open input file \"%s\".\n", inname);
        return 0;
    }

    /* Get input file size */
    insize = GetFileSize(f);

    /* Decompress */
    if(command == 'd')
    {
        /* Read header */
        algo = ReadWord32( f );  /* Dummy */
        algo = ReadWord32( f );
        outsize = ReadWord32( f );
        insize -= 12;
    }

    /* Print operation... */
    switch( algo )
    {
        case 1: printf( "RLE " ); break;
        case 2: printf( "Huffman " ); break;
        case 3: printf( "Rice 8-bit " ); break;
        case 4: printf( "Rice 16-bit " ); break;
        case 5: printf( "Rice 32-bit " ); break;
        case 6: printf( "Rice 8-bit signed " ); break;
        case 7: printf( "Rice 16-bit signed " ); break;
        case 8: printf( "Rice 32-bit signed " ); break;
        case 9: printf( "LZ77 " ); break;
        case 10: printf( "Shannon-Fano " ); break;
    }
    switch( command )
    {
        case 'c': printf( "compress " ); break;
        case 'd': printf( "decompress " ); break;
    }
    printf( "%s to %s...\n", inname, outname );

    /* Read input file */
    printf( "Input file: %d bytes\n", insize );
    in = (unsigned char *) malloc( insize );
    if( !in )
    {
        printf( "Not enough memory\n" );
        fclose( f );
        return 0;
    }
    fread( in, insize, 1, f );
    fclose( f );

    /* Show output file size for decompression */
    if( command == 'd' )
    {
        printf( "Output file: %d bytes\n", outsize );
    }

    /* Open output file */
    f = fopen( outname, "wb" );
    if( !f )
    {
        printf( "Unable to open output file \"%s\".\n", outname );
        free( in );
        return 0;
    }

    /* Compress */
    if( command == 'c' )
    {
        /* Write header */
        fwrite( "BCL1", 4, 1, f );
        WriteWord32( algo, f );
        WriteWord32( insize, f );

        /* Worst case buffer size */
        outsize = (insize*104+50)/100 + 384;
    }

    /* Allocate memory for output buffer */
    out = (unsigned char*)malloc( outsize );
    if( !out )
    {
        printf( "Not enough memory\n" );
        fclose( f );
        free( in );
        return 0;
    }

    /* Compress or decompress */
    if( command == 'c' )
    {
        switch( algo )
        {
            case 1:
                outsize = RLE_Compress( in, out, insize );
                break;
            case 2:
                outsize = Huffman_Compress( in, out, insize );
                break;
            case 3:
                outsize = Rice_Compress( in, out, insize, RICE_FMT_UINT8 );
                break;
            case 4:
                outsize = Rice_Compress( in, out, insize, RICE_FMT_UINT16 );
                break;
            case 5:
                outsize = Rice_Compress( in, out, insize, RICE_FMT_UINT32 );
                break;
            case 6:
                outsize = Rice_Compress( in, out, insize, RICE_FMT_INT8 );
                break;
            case 7:
                outsize = Rice_Compress( in, out, insize, RICE_FMT_INT16 );
                break;
            case 8:
                outsize = Rice_Compress( in, out, insize, RICE_FMT_INT32 );
                break;
            case 9:
                work = (unsigned int*)malloc( sizeof(unsigned int) * (65536+insize) );
                if( work )
                {
                    outsize = LZ_CompressFast( in, out, insize, work );
                    free( work );
                }
                else
                {
                    outsize = LZ_Compress( in, out, insize );
                }
                break;
            case 10:
                outsize = SF_Compress( in, out, insize );
                break;
        }
        printf( "Output file: %d bytes (%.1f%%)\n", outsize,100*(float)outsize/(float)insize );
    }
    else
    {
        switch( algo )
        {
            case 1:
                RLE_Uncompress( in, out, insize );
                break;
            case 2:
                Huffman_Uncompress( in, out, insize, outsize );
                break;
            case 3:
                Rice_Uncompress( in, out, insize, outsize, RICE_FMT_UINT8 );
                break;
            case 4:
                Rice_Uncompress( in, out, insize, outsize, RICE_FMT_UINT16 );
                break;
            case 5:
                Rice_Uncompress( in, out, insize, outsize, RICE_FMT_UINT32 );
                break;
            case 6:
                Rice_Uncompress( in, out, insize, outsize, RICE_FMT_INT8 );
                break;
            case 7:
                Rice_Uncompress( in, out, insize, outsize, RICE_FMT_INT16 );
                break;
            case 8:
                Rice_Uncompress( in, out, insize, outsize, RICE_FMT_INT32 );
                break;
            case 9:
                LZ_Uncompress( in, out, insize );
                break;
            case 10:
                SF_Uncompress( in, out, insize, outsize );
                break;
            default:
                printf( "Unknown compression algorithm: %d\n", algo );
                free( in );
                free( out );
                fclose( f );
                return 0;
        }
    }

    /* Write output file */
    fwrite( out, outsize, 1, f );
    fclose( f );

    /* Free memory */
    free( in );
    free( out );

    return 0;
}
