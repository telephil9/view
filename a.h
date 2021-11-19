enum
{
	NINE,
	SVG,
	JPEG,
	GIF,
	PNG,
	BMP,
};

void*	emalloc(ulong);
void*	erealloc(void*, ulong);
Image*	eallocimage(int, int, ulong, int, ulong);
uchar*	readfile(char*, int*);
int		writefile(char*, char*, int);
int		fileformat(char*);
