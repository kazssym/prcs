all: prcs_doc.html

dist:
	tar -cvf prcs-html.tar *.html *.gif -C .. NEWS TODO FAQ INSTALL README
	gzip prcs-html.tar

clean:
	rm -f prcs_doc.html

prcs_doc.html: ../doc/prcs.texi texi2html html_stub.html
	./texi2html -menu -expandinfo -monolithic ../doc/prcs.texi
