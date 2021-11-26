#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include "a.h"

void*
emalloc(ulong n)
{
	void *p;
	
	p = malloc(n);
	if(p==nil)
		sysfatal("malloc: %r");
	return p;
}

void*
erealloc(void *p, ulong n)
{
	void *q;
	
	q = realloc(p, n);
	if(q==nil)
		sysfatal("realloc: %r");
	return q;
}

Image*
eallocimage(int w, int h, ulong chan, int repl, ulong col)
{
	Image *i;
	
	i = allocimage(display, Rect(0, 0, w, h), chan, repl, col);
	if(i==nil)
		sysfatal("allocimage: %r");
	return i;
}

uchar*
readfile(char *f, int *len)
{
	uchar *buf;
	int fd, n, s, r;
	
	fd = open(f, OREAD);
	if(fd<0)
		sysfatal("open: %r");
	n = 0;
	s = 4096;
	buf = emalloc(s);
	for(;;){
		r = read(fd, buf + n, s - n);
		if(r<0)
			sysfatal("read: %r");
		if(r==0)
			break;
		n += r;
		if(n==s){
			s *= 1.5;
			buf = erealloc(buf, s);
		}
	}
	buf[n] = 0;
	close(fd);
	*len = n;
	return buf;
}

int
writefile(char *filename, char *data, int ndata)
{
	int fd;

	fd = create(filename, OWRITE|OEXCL, 0600);
	if(fd < 0)
		return -1;
	if(write(fd, data, ndata) != ndata)
		return -1;
	close(fd);
	return 0;
}

int
fileformat(char *filename)
{
	static struct {
		char *k;
		int	v;
	} mimes[] = {
	"text/html",	SVG,
	"image/jpeg",	JPEG,
	"image/gif",	GIF,
	"image/png",	PNG,
	"image/bmp",	BMP,
	"image/p9bit",	NINE,
	};	
	int fd[2], n, i;
	char s[32];

	if(pipe(fd) < 0)
		return -1;
	switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
	case -1:
		close(fd[0]);
		close(fd[1]);
		return -1;
	case 0:
		dup(fd[1], 1);
		close(fd[1]);
		close(fd[0]);
		execl("/bin/file", "file", "-m", filename, nil);
		_exits("execl");
	}
	if((n = read(fd[0], s, sizeof s)) <= 0)
		return -1;
	s[n-1] = 0; /* remove newline */
	close(fd[1]);
	close(fd[0]);
	for(i=0; i<nelem(mimes); i++){
		if(strncmp(s, mimes[i].k, strlen(mimes[i].k)) == 0)
			return mimes[i].v;
	}
	werrstr("unknown image type %s", s);
	return -1;
}

Image*
ipipeto(Image *in, char *cmd)
{
	Image *out;
	int ifd[2], ofd[2];
	char *argv[4] = { "rc", "-c", cmd, nil };

	out = nil;
	if(pipe(ifd) < 0)
		return nil;
	if(pipe(ofd) < 0){
		close(ifd[0]);
		close(ifd[1]);
		return nil;
	}
	switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
	case -1:
		goto Err;
	case 0:
		dup(ifd[1], 0);
		dup(ofd[1], 1);
		close(ifd[1]);
		close(ifd[0]);
		close(ofd[1]);
		close(ofd[0]);
		exec("/bin/rc", argv);
		_exits("exec");
	}
	if(writeimage(ifd[0], in, 1) < 0)
		goto Err;
	out = readimage(display, ofd[0], 1);
Err:
	close(ifd[0]);
	close(ifd[1]);
	close(ofd[0]);
	close(ofd[1]);
	return out;
}
