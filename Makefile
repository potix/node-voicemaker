.PHONY: test

all: make test

make:
	node-waf -vvv configure build
test:   
	node ./test/test.js
clean:
	node-waf -vvv clean
