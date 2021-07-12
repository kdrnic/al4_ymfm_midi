// PACKFILE emulation ----------------------------------------------------------
#ifndef KDR_PACKFILE_H
	#define KDR_PACKFILE_H
	struct PACKFILE
	{
		FILE *f;
		long sz;
	};
	typedef struct PACKFILE PACKFILE;
	static PACKFILE *pack_fopen(const char *fn, const char *mode)
	{
		PACKFILE *f = malloc(sizeof(PACKFILE));
		f->f = fopen(fn, "rb");
		if(!f->f){
			free(f);
			return 0;
		}
		fseek(f->f, 0L, SEEK_END);
		f->sz = ftell(f->f);
		rewind(f->f);
		return f;
	}
	static void pack_fclose(PACKFILE *f)
	{
		fclose(f->f);
		free(f);
	}
	static long pack_fread(void *p, long n,PACKFILE *f)
	{
		return fread(p, 1, n, f->f);
	}
	static long pack_mgetl(PACKFILE *f)
	{
		int b1, b2, b3, b4;
		ASSERT(f);
		if ((b1 = fgetc(f->f)) != EOF)
			if ((b2 = fgetc(f->f)) != EOF)
				if ((b3 = fgetc(f->f)) != EOF)
					if ((b4 = fgetc(f->f)) != EOF)
						return (((long)b1 << 24) | ((long)b2 << 16) | ((long)b3 << 8) | (long)b4);
		return EOF;
	}
	/* Finds out if you have reached the end of the file. It does not wait for you to attempt
	to read beyond the end of the file, contrary to the ISO C feof() function. The only way to
	know whether you have read beyond the end of the file is to check the return value of the
	read operation you use (and be wary of pack_*getl() as EOF is also a valid return value 
	with these functions). 
	*/
	static int pack_feof(PACKFILE *fp)
	{
		long old = ftell(fp->f);
		return old >= fp->sz;
	}
	static void pack_fseek(PACKFILE *f, long offs)
	{
		fseek(f->f, offs, SEEK_CUR);
	}
	static long pack_igetl(PACKFILE *f)
	{
		int b1, b2, b3, b4;
		ASSERT(f);
		if ((b1 = fgetc(f->f)) != EOF)
			if ((b2 = fgetc(f->f)) != EOF)
				if ((b3 = fgetc(f->f)) != EOF)
					if ((b4 = fgetc(f->f)) != EOF)
						return (((long)b4 << 24) | ((long)b3 << 16) | ((long)b2 << 8) | (long)b1);
		return EOF;
	}
	static int pack_mgetw(PACKFILE *f)
	{
		int b1, b2;
		ASSERT(f);
		if ((b1 = fgetc(f->f)) != EOF)
			if ((b2 = fgetc(f->f)) != EOF)
				return ((b1 << 8) | b2);
		return EOF;
	}
	static int pack_getc(PACKFILE *f)
	{
		return fgetc(f->f);
	}
#endif
