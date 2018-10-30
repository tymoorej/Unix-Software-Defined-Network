a2sdn: shared.cpp controller.cpp switch.cpp a2sdn.cpp
	g++ shared.cpp controller.cpp switch.cpp a2sdn.cpp -o a2sdn.o

clean:
	rm -rf *.o -f; rm -rf .vscode -f; rm fifo-* -f

tar:
	tar -cvf submit.tar ./

end:
	pkill -U $$USER