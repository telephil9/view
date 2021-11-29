enum
{
	NINE,
	SVG,
	JPEG,
	GIF,
	PNG,
	BMP,
};

Image*	load(char*);
int		save(Image*, char*);
int		export(Image*, char*);
void*	emalloc(ulong);
void*	erealloc(void*, ulong);
Image*	eallocimage(int, int, ulong, int, ulong);
uchar*	readfile(char*, int*);
int		writefile(char*, char*, int);
int		fileformat(char*);
Image*	ipipeto(Image*, char*);

int sepmenuhit(int, Mousectl*, Menu*, Screen*);

