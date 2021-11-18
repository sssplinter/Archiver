#include "stdafx.h"
#include "MownAlg.h"

using namespace MowN;

BufferArray InBuffer; //input file buffer 
BufferArray OutBuffer; //output file buffer 
char InName[80]; //input file name 
char OutName[80]; //output file name 
char CompStr[4]; //ответ от Expand?
FILE *InF; //input file
FILE *OutF; //output file 

TreeDownArray Left, Right; //дочерние ветви дерева 
TreeUpArray Up; //родительская ветвь
bool CompressFlag;  
byte BitPos; //текущий бит в байте 
CodeType InByte; //текущий входной байт 
CodeType OutByte; //текущий выходной байт 
word InSize; //текущий символ в входном буфере 
word OutSize; //текущий символ в выходном буфере 
word Index; //индекс общего назначения

char *Usage = {"Usage: mown [x] infile outfile\n\n"
				"Where 'x' denotes expand infile to outfile\n"
				"Normally compress infile to outfile\n"};

byte BitMask[8]={1, 2, 4, 8, 16, 32, 64, 128};

MownAlg::MownAlg(void)
{
	//MessageBox(0,_T("Not found"), _T("Error"),  MB_OK |MB_ICONERROR);
}

/* rearrange the mown tree for each succeeding character */
void Mown(CodeType Plain)
{
	DownIndex A, B;
	UpIndex C, D;
    
	A = Plain + MaxChar;
    
	do
	{
		/* walk up the tree semi-rotating pairs */
		C = Up[A];
		if (C != Root)
		{
			/* a pair remains */
			D = Up[C];
            
			/* exchange children of pair */
			B = Left[D];
			if (C == B)
			{
				B = Right[D];
				Right[D] = A;
			}
			else
				Left[D] = A;
            
			if (A == Left[C])
				Left[C] = B;
			else
				Right[C] = B;
            
			Up[A] = D;
			Up[B] = C;
			A = D;
		}
		else
			A = C;
	} while (A != Root);
}

/* flush output buffer and reset */
void FlushOutBuffer(void)
{
	if (OutSize > 0)
	{
		fwrite(OutBuffer+1, sizeof(byte), OutSize, OutF);
		OutSize = 0;
	}
}

/* output byte in OutByte */
void WriteByte(void)
{
	if (OutSize == BufSize)
		FlushOutBuffer();
	OutSize++;
	OutBuffer[OutSize] = OutByte;
}

/* compress a single char */
void Compress(CodeType Plain)
{
	DownIndex A;
	UpIndex U;
	word Sp;
	bool Stack[PredMax+1];
    
	A = Plain + MaxChar;
	Sp = 0;
    
	/* walk up the tree pushing bits onto stack */
	do
	{
		U = Up[A];
		Stack[Sp] = (Right[U] == A);
		Sp++;
		A = U;
	} while (A != Root);
    
	/* unstack to transmit bits in correct order */
	do
	{
		Sp--;
		if (Stack[Sp])
			OutByte |= BitMask[BitPos];
		if (BitPos == 7)
		{
			/* byte filled with bits, write it out */
			WriteByte();
			BitPos = 0;
			OutByte = 0;
		}
		else
			BitPos++;
	} while (Sp != 0);
    
	/* update the tree */
	Mown(Plain);
}

/* compress input file, writing to outfile */
void CompressFile(void)
{
	FileHeader Header;
    
	/* write header to output */
	Header.Signature = Sig;
	fwrite(&Header, sizeof(FileHeader), 1, OutF);
    
	/* compress file */
	OutSize = 0;
	BitPos = 0;
	OutByte = 0;
	do
	{
		InSize = fread(InBuffer+1, sizeof(byte), BufSize, InF);
		for (Index = 1; Index <= InSize; Index++)
			Compress(InBuffer[Index]);
	} while (InSize >= BufSize);
    
	/* Mark end of file */
	Compress(EofChar);
    
	/* Flush buffers */
	if (BitPos != 0)
		WriteByte();
	FlushOutBuffer();
}

/* initialize the mown tree - as a balanced tree */
void MownAlg::InitializeMown(void)
{
    DownIndex I;
    int /*UpIndex*/ J;
    DownIndex K;
    
    for (I = 1; I <= TwiceMax; I++)
        Up[I] = (I - 1) >> 1;
    for (J = 0; J <= PredMax; J++)
    {
        K = ((byte)J + 1) << 1;
        Left[J] = K - 1;
        Right[J] = K;
    }
}

/* read a compressed file header */
void MownAlg::ReadHeader(void)
{
    FileHeader Header;
    
    fread(&Header, sizeof(FileHeader), 1, InF);
    if (Header.Signature != Sig)
    {
        printf("Unrecognized file format!\n");
    }
}

/* return next byte from compressed input */
byte MownAlg::GetByte(void)
{
    Index++;
    if (Index > InSize)
    {
        /* reload file buffer */
        InSize = fread(InBuffer+1, sizeof(byte), BufSize, InF);
        Index = 1;
        /* end of file handled by special marker in compressed file */
    }
    
    /* get next byte from buffer */
    return InBuffer[Index];
}

/* return next char from compressed input */
CodeType MownAlg::Expand(void)
{
    DownIndex A;
    
    /* scan the tree to a leaf, which determines the character */
    A = Root;
    do
    {
        if (BitPos == 7)
        {
            /* used up bits in current byte, get another */
            InByte = GetByte();
            BitPos = 0;
        }
        else
            BitPos++;
        
        if ((InByte & BitMask[BitPos]) == 0)
            A = Left[A];
        else
            A = Right[A];
    } while (A <= PredMax);
    
    /* Update the code tree */
    A -= MaxChar;
    Mown(A);
    
    /* return the character */
    return A;
}

/* uncompress the file and write output */
void MownAlg::ExpandFile(void)
{
    /* force buffer load first time */
    Index = 0;
    InSize = 0;
    /* nothing in output buffer */
    OutSize = 0;
    /* force bit buffer load first time */
    BitPos = 7;
    
    /* read and expand the compressed input */
    OutByte = Expand();
    while (OutByte != EofChar)
    {
        WriteByte();
        OutByte = Expand();
    }
    
    /* flush the output buffer */
    FlushOutBuffer();
}

void MownAlg::mainMown(int count, char *args[])
{

    if (count < 3)
    {
        printf(Usage);
    }
    
    if (count == 4 && (strlen(args[1]) == 1) && toupper(args[1][0]) == 'X')
    {
        strcpy(InName, args[2]);
        strcpy(OutName, args[3]);
        CompressFlag = false;
    }
    else
    {
        if (count == 4)
        {
            printf(Usage);
        }
        CompressFlag = true;
        strcpy(InName, args[1]);
        strcpy(OutName, args[2]);
    }
        
    InitializeMown();
    
    if ((InF = fopen(InName, "rb")) == NULL)
    {
        printf("Unable to open input file: %s\n", InName);
    }
    if ((OutF = fopen(OutName, "wb")) == NULL)
    {
        printf("Unable to open output file: %s\n", OutName);
    }
    
    if (CompressFlag)
        CompressFile();
    else
    {
        ReadHeader();
        ExpandFile();
    }
    
    fclose(InF);
    fclose(OutF);
}