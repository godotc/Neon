
t:=Neon

.PHONY: 

r: 
	xmake b $(t) 
	xmake r $(t)

cfg: 
	xmake f -m debug -y
	xmake project -k compile_commands

