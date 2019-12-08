SUB = lisp vtx vbidecode

all:
	for i in $(SUB); do ( cd $$i; make; ) done

clean:
	rm -f *~
	for i in $(SUB); do ( cd $$i; make clean; ) done

out:
	make clean
	cd ..; tar czvf vtx-emacs.tar.gz vtx-emacs
